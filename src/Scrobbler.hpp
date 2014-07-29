/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>. */
#ifndef SCROBBLER_HPP_
#define SCROBBLER_HPP_

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <condition_variable>
#include <cstddef>
#include <cstdio>
#include <iterator>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <afc/FastStringBuffer.hpp>
#include <sys/stat.h>
#include "logger.hpp"
#include "ScrobbleInfo.hpp"

// TODO make logging tag configurable.
template<typename ScrobbleQueue>
class Scrobbler
{
	static_assert(std::is_same<typename ScrobbleQueue::value_type, ScrobbleInfo>::value,
			"ScrobbleQueue must be a container of ScrobbleInfo objects.");

	Scrobbler(const Scrobbler &) = delete;
	Scrobbler(Scrobbler &&) = delete;
	Scrobbler &operator=(const Scrobbler &) = delete;
	Scrobbler &operator=(Scrobbler &&) = delete;
public:
	Scrobbler() : m_mutex(), m_scrobblingThread(), m_cv(), m_startStopMutex(), m_finishScrobblingFlag(false)
	{ std::lock_guard<std::mutex> lock(m_mutex); // synchronising memory
		m_started = false;
		m_configured = false;

		/* This instance is partially initialised here. It will be initialised completely
		 * when ::start() is invoked successfully.
		 */
	}

	virtual ~Scrobbler()
	{
		// Synchronising memory before destructing the member fields of this Scrobbler.
		std::lock_guard<std::mutex> lock(m_mutex);
	}

	void invalidateConfiguration()
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_configured = false;
	}

	/*
	 * Adds a given scrobble to the list of pending scrobbles. Optionally, it saves
	 * the given scrobble to the data file to keep if available even if an emergency
	 * happens. Scrobbler::stop() rewrites the data file, sweeping out all
	 * scrobbles whose processing is completed.
	 *
	 * Nothing is done if this Scrobbler is not started.
	 *
	 * @param scrobbleInfo the track scrobble to process.
	 * @param safeScrobble if true then the scrobble is stored to the data file
	 *         immediately, to save this scrobble even in case of an emergency.
	 *         It is false by default.
	 */
	void scrobble(ScrobbleInfo &&scrobbleInfo, const bool safeScrobbling = false);

	bool start();
	bool stop();

	bool started() const
	{ std::lock_guard<std::mutex> lock(m_mutex);
		return m_started;
	}
private:
	bool loadPendingScrobbles();
	bool storePendingScrobbles();
	void backgroundScrobbling();

	// Used by openDataFile().
	enum OpenResult {O_ERROR, O_OPENED, O_NOTEXIST};

	static OpenResult openDataFile(const std::string &path, const char * const mode, const bool storeMode,
			std::FILE *&dest);
	static bool createParentDirs(const std::string &path);

	template<typename Iterator>
	static bool storeScrobbles(Iterator begin, const Iterator end, const std::string &dataFilePath,
			const char * const storeMode);
protected:
	// Returns the number of scrobbles completed (successful and non-processable).
	virtual std::size_t doScrobbling() = 0;

	/**
	 * Returns the path to the file where to store pending scrobbles to.
	 * It should be a non-empty string.
	 */
	virtual const std::string &getDataFilePath() const = 0;

	/**
	 * Routine to be executed while Scrobbler is about to finish stopping itself.
	 * In particular, the scrobbling is already disabled but the pending scrobbles
	 * are still available and not stored to the persistent storage.
	 *
	 * It is executed within locks on m_startStopMutex and m_mutex.
	 *
	 * @return true if this routine succeeds; false otherwise.
	 */
	virtual void stopExtra() { /* Nothing to do by default. */ }

	/* Ensures that this function is executed within the critical section against m_mutex.
	 * Even though mutex::try_lock() has side effects it is fine to acquire the lock m_mutex
	 * since the application is terminated immediately in this case.
	 */
	inline void assertLocked() noexcept { assert(!m_mutex.try_lock()); }

	static constexpr std::size_t minScrobblesToWait() noexcept { return 1; }
	static constexpr std::size_t maxScrobblesToWait() noexcept { return 32; }

	ScrobbleQueue m_pendingScrobbles;

	mutable std::size_t m_scrobblesToWait;
	mutable std::mutex m_mutex;
private:
	mutable std::thread m_scrobblingThread;
	mutable std::condition_variable m_cv;
protected:
	/* Used to prevent parallel execution of the functions start() and stop().
	 * This is needed for stop() to know that the scrobbling thread is stopped
	 * at some time to store all pending scrobbles to the data file.
	 */
	mutable std::mutex m_startStopMutex;
	/* Indicates if the background scrobbling thread should finish its work
	 * including aborting an active connection to the scrobbling service
	 * (e.g. Gravifon) if it is established.
	 */
	mutable std::atomic<bool> m_finishScrobblingFlag;
	bool m_started;
	bool m_configured;
};

// storeMode is the file access specifier valid for std::fopen().
template<typename ScrobbleQueue>
template<typename Iterator>
inline bool Scrobbler<ScrobbleQueue>::storeScrobbles(Iterator begin, const Iterator end,
		const std::string &dataFilePath, const char * const storeMode)
{
	static_assert(std::is_convertible<decltype(*begin), const ScrobbleInfo &>::value,
			"An iterator over ScrobbleInfo objects is expected.");
	assert(storeMode != nullptr);

	std::FILE *dataFile; // initialised by openDataFile();
	if (openDataFile(dataFilePath, storeMode, true, dataFile) != O_OPENED) {
		return false;
	}

	assert(dataFile != nullptr);

	bool result = true;

	afc::FastStringBuffer<char> buf;
	for (auto it = begin; it != end; ++it) {
		buf.clear();
		appendAsJson(*it, buf);
		const std::size_t bufSize = buf.size();
		if (std::fwrite(buf.c_str(), sizeof(unsigned char), bufSize, dataFile) != bufSize) {
			result = false;
			goto finish;
		}
		/* ScrobbleInfo in the JSON form does not contain the character 'line feed' ('\n')
		 * so that using the latter as a separator is safe.
		 *
		 * The file must end with the empty line.
		 */
		if (std::fwrite(u8"\n", sizeof(unsigned char), 1, dataFile) != 1) {
			result = false;
			goto finish;
		}
	}
finish:
	/* An assumption works that this code is compiled with exceptions disabled.
	 * If this is not the case then unique_ptr<FILE> with a custom deleter must
	 * be used to avoid resource leaks.
	 */
	if (std::fclose(dataFile) != 0) {
		result = false;
	}

	return result;
}

template<typename ScrobbleQueue>
void Scrobbler<ScrobbleQueue>::scrobble(ScrobbleInfo &&scrobbleInfo, const bool safeScrobbling)
{ std::lock_guard<std::mutex> lock(m_mutex);
	if (!m_started) {
		// This Scrobbler is not started or is already stopped or is disabled.
		return;
	}

	m_pendingScrobbles.emplace_back(std::move(scrobbleInfo));

	m_cv.notify_one();

	if (safeScrobbling) {
		/* Storing the scrobble that has just been added to the list.
		 * The data file is appended, not re-written.
		 */
		auto end = m_pendingScrobbles.cend();
		if (storeScrobbles(std::prev(end), end, getDataFilePath(), "ab")) {
			logDebug("[Scrobbler] The scrobble that has just been scrobbled "
					"is stored (failure-safe scrobbling).");
		} else {
			logErrorMsg("[Scrobbler] Unable to store the scrobble (failure-safe scrobbling).");
		}
	}
}

template<typename ScrobbleQueue>
void Scrobbler<ScrobbleQueue>::backgroundScrobbling()
{ std::unique_lock<std::mutex> lock(m_mutex);
	logDebug("[Scrobbler] The background scrobbling thread has started.");

	bool lastAttemptFailed = false;
	std::size_t prevScrobbleCount = m_pendingScrobbles.size();
	std::size_t idleScrobbleCount = 0;

	while (!m_finishScrobblingFlag.load(std::memory_order_relaxed)) {
		/* An attempt to submit is performed iff this Scrobbler is configured properly AND:
		 * - the last scrobbling call did not fail and the list of pending scrobbles is not empty
		 *     (useful when there is already a long list of pending scrobbles)
		 * OR
		 * - the number of scrobbles has changed
		 */
		while (!(m_configured &&
				(m_pendingScrobbles.size() != prevScrobbleCount ||
						(!lastAttemptFailed && !m_pendingScrobbles.empty())))) {
			m_cv.wait(lock);

			if (m_finishScrobblingFlag.load(std::memory_order_relaxed)) {
				// Finishing the background scrobbling thread since this Scrobbler is stopped.
				logDebug("[Scrobbler] The background scrobbling thread is going to be stopped...");
				return;
			}
		}

		if (lastAttemptFailed) {
			// It is possible that idling mode is to be enabled in this iteration.

			// Updating the number of tracks scrobbled while idling.
			const std::size_t size = m_pendingScrobbles.size();
			idleScrobbleCount += size - prevScrobbleCount;

			/* The scrobble count must be updated even when idling because in this case
			 * the next iteration must see the correct number of pending scrobbles to decide
			 * whether to wait for another scrobble or not.
			 */
			prevScrobbleCount = size;

			if (idleScrobbleCount < m_scrobblesToWait) {
				// Idling due to failed previous attempt to scrobble tracks.
				logDebug("Idling is to last for # tracks scrobbled. Scrobbles passed: #.",
						m_scrobblesToWait, idleScrobbleCount);

				/* Idling is forced due to some last scrobble requests failed.
				 * No scrobble request is submitted.
				 */
				continue;
			}
		}

		// Idling has finished so resetting the counter of tracks scrobbled while idling.
		idleScrobbleCount = 0;

		// Scrobbling tracks.
		const std::size_t scrobbledCount = doScrobbling();
		lastAttemptFailed = scrobbledCount == 0;

		if (lastAttemptFailed) {
			/* If this attempt has failed then increasing the timeout by two times
			 * up to max allowed limit.
			 */
			m_scrobblesToWait = std::min(m_scrobblesToWait * 2, maxScrobblesToWait());

			logDebug("Idling is to last now for # tracks scrobbled.", m_scrobblesToWait);
		} else {
			// If the attempt is (partially) successful then the timeout is reset.
			m_scrobblesToWait = minScrobblesToWait();
		}

		prevScrobbleCount = m_pendingScrobbles.size();
	}
}

template<typename ScrobbleQueue>
bool Scrobbler<ScrobbleQueue>::start()
// m_startStopMutex must be locked first to co-operate with ::stop() properly.
{ std::lock_guard<std::mutex> startStopLock(m_startStopMutex); std::lock_guard<std::mutex> lock(m_mutex);
	if (m_started) {
		// This Scrobbler is already started.
		return false;
	}

	if (!loadPendingScrobbles()) {
		return false;
	}

	m_finishScrobblingFlag.store(false, std::memory_order_relaxed);

	logDebugMsg("[Scrobbler] Starting the background scrobbling thread...");

	m_scrobblingThread = std::thread([this]() { this->backgroundScrobbling(); });

	m_scrobblesToWait = minScrobblesToWait();

	m_started = true;
	return true;
}

template<typename ScrobbleQueue>
bool Scrobbler<ScrobbleQueue>::stop()
// m_startStopMutex must be locked first to co-operate with ::start() properly.
{ std::lock_guard<std::mutex> startStopLock(m_startStopMutex);
	std::thread threadToStop;

	{ std::lock_guard<std::mutex> lock(m_mutex);
		if (!m_started) {
			// This Scrobbler is not started or is already stopped.
			return true;
		}

		/* The scrobbling thread is disassociated with this Scrobbler so that
		 * after this critical section is exited this Scrobbler could be used safely.
		 */
		threadToStop.swap(m_scrobblingThread);

		m_finishScrobblingFlag.store(true, std::memory_order_relaxed);

		logDebug("[Scrobbler] The scrobbling thread is being stopped...");

		// Waking up the scrobbing thread to let it finish quickly.
		m_cv.notify_one();
	}

	/* Waiting for the old scrobbling thread to finish. This is used to ensure that
	 * there are no scrobbles that are being submitted so that the list of pending
	 * scrobbles could be serialised safely.
	 */
	threadToStop.join();
	logDebugMsg("[Scrobbler] The scrobbling thread is stopped.");

	{ std::lock_guard<std::mutex> lock(m_mutex);
		/* Invocation of stopExtra() must go after thread join and
		 * before storing the pending scrobbles as per documentation.
		 */
		stopExtra();

		if (!storePendingScrobbles()) {
			logErrorMsg("[Scrobbler] Unable to store pending scrobbles. These scrobbles are lost.");
		}

		/* TODO do not clear the list of pending scrobbles. Instead, report an error so that
		 * the user has a chance to identify the issue and fix it and then store the scrobbles
		 * successfully. This implementation should co-operate with possible ::start()
		 * invocation after stop() returns.
		 */
		m_pendingScrobbles.clear();

		/* Clearing configuration so that this Scrobbler is to be re-configured
		 * if it is re-used later.
		 */
		m_configured = false;

		m_started = false;
	}

	return true;
}

template<typename ScrobbleQueue>
// The last path element is considered as a file and therefore is not created.
inline bool Scrobbler<ScrobbleQueue>::createParentDirs(const std::string &path)
{
	if (path.empty()) {
		return true;
	}
	for (std::size_t start = path[0] == '/' ? 1 : 0;;) {
		const std::size_t end = path.find_first_of('/', start);
		if (end == std::string::npos) {
			return true;
		}
		if (mkdir(path.substr(0, end).c_str(), 0775) != 0 && errno != EEXIST) {
			return false;
		}
		start = end + 1;
	}
}

template<typename ScrobbleQueue>
inline typename Scrobbler<ScrobbleQueue>::OpenResult
Scrobbler<ScrobbleQueue>::openDataFile(const std::string &path, const char * const mode, const bool storeMode,
		std::FILE *&dest)
{
	if (path.empty()) {
		return O_ERROR;
	}

	/* - if the file or some parent directories do not exist then create missing parts if
	 *   the store mode is enabled.
	 * - if the file exists but it is not a regular file or a symbolic link then return failure
	 *   because such a file cannot be used to load/store pending scrobbles
	 * - if the file exists and it a regular file or a symbolic link then proceed with
	 *   loading/storing pending scrobbles from/into it
	 */
	const char * const cPath = path.c_str();
	struct stat fileStatus;
	if (stat(cPath, &fileStatus) != 0) {
		if (errno == ENOTDIR || errno == ENOENT) {
			if (!storeMode) {
				return O_NOTEXIST;
			}
			if (!createParentDirs(path)) {
				return O_ERROR;
			}
		} else {
			return O_ERROR;
		}
	} else if (!(S_ISREG(fileStatus.st_mode) || S_ISLNK(fileStatus.st_mode))) {
		return O_ERROR;
	}

	// If we are here then the file either exists or the store mode is enabled.
	dest = fopen(cPath, mode);
	// If the file is not opened here then reporting an error for both cases.
	return dest != nullptr ? O_OPENED : O_ERROR;
}

template<typename ScrobbleQueue>
inline bool Scrobbler<ScrobbleQueue>::loadPendingScrobbles()
{
	assertLocked();

	logDebug("[Scrobbler] Loading pending scrobbles...");

	std::FILE *dataFile; // initialised by openDataFile();
	const OpenResult openResult = openDataFile(getDataFilePath(), "rb", false, dataFile);
	if (openResult == O_ERROR) {
		return false;
	} else if (openResult == O_NOTEXIST) {
		// There are no pending scrobbles.
		return true;
	}

	assert(openResult == O_OPENED);
	assert(dataFile != nullptr);

	bool result = true;
	std::string buf;
	for (;;) {
		const int c = std::fgetc(dataFile);
		if (c == EOF) {
			break;
		}
		if (c == 0x0a) { // c == u8'\n'
			/* Instantiating the destination scrobble within the queue to minimise copying/moving.
			 * It parsing fails then it is ejected. It is assumed that exceptions are disabled.
			 */
			m_pendingScrobbles.emplace_back();
			if (ScrobbleInfo::parse(buf.data(), buf.data() + buf.size(), m_pendingScrobbles.back())) {
				buf.clear();
			} else {
				m_pendingScrobbles.pop_back();
				result = false;
				goto finish;
			}
		} else {
			buf += (char) c;
		}
	}
	if (feof(dataFile) == 0) {
		result = false;
	} else {
		/* The last byte of the data file must be either 0x0a or just the end
		 * of the last scrobble. In either case buf is empty.
		 */
		result &= buf.empty();
	}
finish:
	if (std::fclose(dataFile) != 0) {
		result = false;
	}

	logDebug("[Scrobbler] Pending scrobbles loaded: #.", m_pendingScrobbles.size());
	return result;
}

template<typename ScrobbleQueue>
inline bool Scrobbler<ScrobbleQueue>::storePendingScrobbles()
{
	assertLocked();

	logDebugMsg("[Scrobbler] Storing pending scrobbles...");

	/* The assumption is that all tracks are loaded into the list of pending scrobbles
	 * so that the file could be overwritten with the remaining pending scrobbles.
	 */
	const bool result = storeScrobbles(m_pendingScrobbles.cbegin(), m_pendingScrobbles.cend(), getDataFilePath(), "wb");

	logDebug("[Scrobbler] Pending scrobbles stored: #.", m_pendingScrobbles.size());

	return result;
}

#endif /* SCROBBLER_HPP_ */
