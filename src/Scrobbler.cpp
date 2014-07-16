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
#include "Scrobbler.hpp"
#include <cassert>
#include <afc/utils.h>
#include <time.h>
#include <cstdlib>
#include <cstdio>
#include <cerrno>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <sys/stat.h>
#include "logger.hpp"
#include <functional>
#include <type_traits>
#include <iterator>
#include <algorithm>
#include "jsonutil.hpp"
#include "pathutil.hpp"
#include <afc/ensure_ascii.hpp>


using namespace std;
using namespace afc;
using Json::Value;
using Json::ValueType;

const size_t Scrobbler::MIN_SCROBBLES_TO_WAIT = 1;
const size_t Scrobbler::MAX_SCROBBLES_TO_WAIT = 32;

namespace
{
	inline void writeJsonString(const string &src, string &dest)
	{
		for (const char c : src) {
			switch (c) {
			case '"':
			case '\\':
				dest.push_back('\\');
				dest.push_back(c);
				break;
			case '\b':
				dest.append(u8"\\b");
				break;
			case '\f':
				dest.append(u8"\\f");
				break;
			case '\n':
				dest.append(u8"\\n");
				break;
			case '\r':
				dest.append(u8"\\r");
				break;
			case '\t':
				dest.append(u8"\\t");
				break;
			default:
				dest.push_back(c);
				break;
			}
		}
	}

	inline void writeJsonTimestamp(const DateTime &timestamp, string &dest)
	{
		/* The datetime format as required by https://github.com/gravidence/gravifon/wiki/Date-Time
		 * Milliseconds are not supported.
		 */
		std::tm dateTime = static_cast<std::tm>(timestamp);

		/* Twenty five characters (including the terminating character) are really used
		 * to store an ISO-8601-formatted date for a single-byte encoding. For multi-byte
		 * encodings this is not the case, and a larger buffer could be needed.
		 */
		for (size_t outputSize = 25;; outputSize *= 2) {
			char buf[outputSize];
			if (strftime(buf, outputSize, "%Y-%m-%dT%H:%M:%S%z", &dateTime) != 0) {
				// The date is formatted successfully.
				dest.append(u8"\"").append(convertToUtf8(buf, systemCharset().c_str())).append(u8"\"");
				return;
			}
			// The size of the destination buffer is too small. Repeating with a larger buffer.
		}
	}

	// writes value to out in utf-8
	inline void writeJsonLong(const long value, string &dest)
	{
		dest.append(convertToUtf8(to_string(value), systemCharset().c_str()));
	}

	// The last path element is considered as a file and therefore is not created.
	inline bool createParentDirs(const string &path)
	{
		if (path.empty()) {
			return true;
		}
		for (size_t start = path[0] == '/' ? 1 : 0;;) {
			const size_t end = path.find_first_of('/', start);
			if (end == string::npos) {
				return true;
			}
			if (mkdir(path.substr(0, end).c_str(), 0775) != 0 && errno != EEXIST) {
				return false;
			}
			start = end + 1;
		}
	}

	// Used by openDataFile().
	enum OpenResult {O_ERROR, O_OPENED, O_NOTEXIST};

	inline OpenResult openDataFile(const string &path, const char * const mode, const bool storeMode, FILE *&dest)
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

	inline bool parseDateTime(const Value &dateTimeObject, DateTime &dest)
	{
		return isType(dateTimeObject, stringValue) && parseISODateTime(dateTimeObject.asString(), dest);
	}

	inline bool parseDuration(const Value &durationObject, long &dest)
	{
		if (!isType(durationObject, objectValue)) {
			return false;
		}
		const Value &amount = durationObject["amount"];
		const Value &unit = durationObject["unit"];
		if (!isType(amount, intValue) || !isType(unit, stringValue)) {
			return false;
		}
		const long val = amount.asInt();
		const string unitValue = unit.asString();
		if (unitValue == "ms") {
			dest = val;
			return true;
		} else if (unitValue == "s") {
			dest = val * 1000;
			return true;
		}
		return false;
	}

	template<typename AddArtistOp>
	inline bool parseArtists(const Value &artists, const bool artistsRequired, AddArtistOp addArtistOp)
	{
		if (!isType(artists, arrayValue) || artists.empty()) {
			return !artistsRequired;
		}
		for (auto i = 0u, n = artists.size(); i < n; ++i) {
			const Value &artist = artists[i];
			if (!isType(artist, objectValue)) {
				return false;
			}
			const Value &artistName = artist["name"];
			if (!isType(artistName, stringValue)) {
				return false;
			}
			addArtistOp(artistName.asString());
		}
		return true;
	}

	// storeMode is the file access specifier valid for std::fopen().
	template<typename Iterator>
	inline bool storeScrobbles(Iterator begin, const Iterator end, const string &dataFilePath,
			const char * const storeMode)
	{
		static_assert(std::is_convertible<decltype(*begin), const ScrobbleInfo &>::value,
				"An iterator over ScrobbleInfo objects is expected.");
		assert(storeMode != nullptr);

		FILE *dataFile; // initialised by openDataFile();
		if (openDataFile(dataFilePath, storeMode, true, dataFile) != O_OPENED) {
			return false;
		}

		assert(dataFile != nullptr);

		bool result = true;

		string buf;
		for (auto it = begin; it != end; ++it) {
			buf.clear();
			it->appendAsJsonTo(buf);
			const size_t bufSize = buf.size();
			if (fwrite(buf.c_str(), sizeof(unsigned char), bufSize, dataFile) != bufSize) {
				result = false;
				goto finish;
			}
			/* ScrobbleInfo in the JSON form does not contain the character 'line feed' ('\n')
			 * so that using the latter as a separator is safe.
			 *
			 * The file must end with the empty line.
			 */
			if (fwrite(u8"\n", sizeof(unsigned char), 1, dataFile) != 1) {
				result = false;
				goto finish;
			}
		}
	finish:
		/* An assumption works that this code is compiled with exceptions disabled.
		 * If this is not the case then unique_ptr<FILE> with a custom deleter must
		 * be used to avoid resource leaks.
		 */
		if (fclose(dataFile) != 0) {
			result = false;
		}

		return result;
	}
}

void Scrobbler::scrobble(const ScrobbleInfo &scrobbleInfo, const bool safeScrobbling)
{ lock_guard<mutex> lock(m_mutex);
	if (!m_started) {
		// This Scrobbler is not started or is already stopped or is disabled.
		return;
	}

	m_pendingScrobbles.emplace_back(scrobbleInfo);

	m_cv.notify_one();

	if (safeScrobbling) {
		/* Storing the scrobble that has just been added to the list.
		 * The data file is appended, not re-written.
		 */
		auto end = m_pendingScrobbles.cend();
		if (storeScrobbles(prev(end), end, getDataFilePath(), "ab")) {
			logDebug("[Scrobbler] The scrobble that has just been scrobbled "
					"is stored (failure-safe scrobbling).");
		} else {
			logError("[Scrobbler] Unable to store the scrobble (failure-safe scrobbling).");
		}
	}
}

void Scrobbler::backgroundScrobbling()
{ unique_lock<mutex> lock(m_mutex);
	logDebug("[Scrobbler] The background scrobbling thread has started.");

	bool lastAttemptFailed = false;
	size_t prevScrobbleCount = m_pendingScrobbles.size();
	size_t idleScrobbleCount = 0;

	while (!m_finishScrobblingFlag.load(memory_order_relaxed)) {
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

			if (m_finishScrobblingFlag.load(memory_order_relaxed)) {
				// Finishing the background scrobbling thread since this Scrobbler is stopped.
				logDebug("[Scrobbler] The background scrobbling thread is going to be stopped...");
				return;
			}
		}

		if (lastAttemptFailed) {
			// It is possible that idling mode is to be enabled in this iteration.

			// Updating the number of tracks scrobbled while idling.
			const size_t size = m_pendingScrobbles.size();
			idleScrobbleCount += size - prevScrobbleCount;

			/* The scrobble count must be updated even when idling because in this case
			 * the next iteration must see the correct number of pending scrobbles to decide
			 * whether to wait for another scrobble or not.
			 */
			prevScrobbleCount = size;

			if (idleScrobbleCount < m_scrobblesToWait) {
				// Idling due to failed previous attempt to scrobble tracks.
				logDebug(string("Idling is to last for ") + to_string(m_scrobblesToWait) +
						" tracks scrobbled. Scrobbles passed: " + to_string(idleScrobbleCount));

				/* Idling is forced due to some last scrobble requests failed.
				 * No scrobble request is submitted.
				 */
				continue;
			}
		}

		// Idling has finished so resetting the counter of tracks scrobbled while idling.
		idleScrobbleCount = 0;

		// Scrobbling tracks.
		const size_t scrobbledCount = doScrobbling();
		lastAttemptFailed = scrobbledCount == 0;

		if (lastAttemptFailed) {
			/* If this attempt has failed then increasing the timeout by two times
			 * up to max allowed limit.
			 */
			m_scrobblesToWait = min(m_scrobblesToWait * 2, MAX_SCROBBLES_TO_WAIT);

			logDebug(string("Idling is to last now for ") + to_string(m_scrobblesToWait) +
					" tracks scrobbled.");
		} else {
			// If the attempt is (partially) successful then the timeout is reset.
			m_scrobblesToWait = MIN_SCROBBLES_TO_WAIT;
		}

		prevScrobbleCount = m_pendingScrobbles.size();
	}
}

bool Scrobbler::start()
// m_startStopMutex must be locked first to co-operate with ::stop() properly.
{ lock_guard<mutex> startStopLock(m_startStopMutex); lock_guard<mutex> lock(m_mutex);
	if (m_started) {
		// This Scrobbler is already started.
		return false;
	}

	if (!loadPendingScrobbles()) {
		return false;
	}

	m_finishScrobblingFlag.store(false, memory_order_relaxed);

	logDebug("[Scrobbler] Starting the background scrobbling thread...");

	m_scrobblingThread = thread(&Scrobbler::backgroundScrobbling, ref(*this));

	m_scrobblesToWait = MIN_SCROBBLES_TO_WAIT;

	m_started = true;
	return true;
}

bool Scrobbler::stop()
// m_startStopMutex must be locked first to co-operate with ::start() properly.
{ lock_guard<mutex> startStopLock(m_startStopMutex);
	thread threadToStop;

	{ lock_guard<mutex> lock(m_mutex);
		if (!m_started) {
			// This Scrobbler is not started or is already stopped.
			return true;
		}

		/* The scrobbling thread is disassociated with this Scrobbler so that
		 * after this critical section is exited this Scrobbler could be used safely.
		 */
		threadToStop.swap(m_scrobblingThread);

		m_finishScrobblingFlag.store(true, memory_order_relaxed);

		logDebug("[Scrobbler] The scrobbling thread is being stopped...");

		// Waking up the scrobbing thread to let it finish quickly.
		m_cv.notify_one();
	}

	/* Waiting for the old scrobbling thread to finish. This is used to ensure that
	 * there are no scrobbles that are being submitted so that the list of pending
	 * scrobbles could be serialised safely.
	 */
	threadToStop.join();
	logDebug("[Scrobbler] The scrobbling thread is stopped.");

	{ lock_guard<mutex> lock(m_mutex);
		/* Invocation of stopExtra() must go after thread join and
		 * before storing the pending scrobbles as per documentation.
		 */
		stopExtra();

		if (!storePendingScrobbles()) {
			logError("[Scrobbler] Unable to store pending scrobbles. These scrobbles are lost.");
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

inline bool Scrobbler::loadPendingScrobbles()
{
	logDebug("[Scrobbler] Loading pending scrobbles...");

	FILE *dataFile; // initialised by openDataFile();
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
	string buf;
	for (;;) {
		const int c = fgetc(dataFile);
		if (c == EOF) {
			break;
		}
		if (c == 0x0a) { // c == u8'\n'
			m_pendingScrobbles.emplace_back();
			if (!ScrobbleInfo::parse(buf, m_pendingScrobbles.back())) {
				result = false;
				goto finish;
			}
			buf.clear();
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
	if (fclose(dataFile) != 0) {
		result = false;
	}

	logDebug("[Scrobbler] Pending scrobbles loaded: " + to_string(m_pendingScrobbles.size()));
	return result;
}

inline bool Scrobbler::storePendingScrobbles()
{
	logDebug("[Scrobbler] Storing pending scrobbles...");

	/* The assumption is that all tracks are loaded into the list of pending scrobbles
	 * so that the file could be overwritten with the remaining pending scrobbles.
	 */
	const bool result = storeScrobbles(m_pendingScrobbles.cbegin(), m_pendingScrobbles.cend(), getDataFilePath(), "wb");

	logDebug("[Scrobbler] Pending scrobbles stored: " + to_string(m_pendingScrobbles.size()));

	return result;
}

bool ScrobbleInfo::parse(const string &str, ScrobbleInfo &dest)
{
	Json::Reader jsonReader;
	Value object;
	if (!jsonReader.parse(str, object, false)) {
		logError(string("[Scrobbler] Unable to parse the scrobble JSON object: ") +
				jsonReader.getFormatedErrorMessages());
		return false;
	}
	if (!isType(object, objectValue)) {
		return false;
	}
	if (!parseDateTime(object["scrobble_start_datetime"], dest.scrobbleStartTimestamp) ||
			!parseDateTime(object["scrobble_end_datetime"], dest.scrobbleEndTimestamp) ||
			!parseDuration(object["scrobble_duration"], dest.scrobbleDuration)) {
		return false;
	}

	Track &track = dest.track;

	const Value trackObject = object["track"];
	if (!isType(trackObject, objectValue)) {
		return false;
	}
	const Value &trackTitle = trackObject["title"];
	if (!isType(trackTitle, stringValue)) {
		return false;
	}
	track.setTitle(trackTitle.asString());

	const Value &trackAlbum = trackObject["album"];
	const ValueType trackAlbumObjType = trackAlbum.type();
	if (trackAlbumObjType != nullValue) {
		if (trackAlbumObjType != objectValue) {
			return false;
		}
		const Value &trackAlbumTitle = trackAlbum["title"];
		if (!isType(trackAlbumTitle, stringValue)) {
			return false;
		}
		track.setAlbumTitle(trackAlbumTitle.asString());

		if (!parseArtists(trackAlbum["artists"], false,
				[&](string &&artistName) { track.addAlbumArtist(artistName); })) {
			return false;
		}
	}

	long trackDuration;
	if (!parseDuration(trackObject["length"], trackDuration)) {
		return false;
	}
	track.setDurationMillis(trackDuration);

	if (!parseArtists(trackObject["artists"], true,
			[&](string &&artistName) { track.addArtist(artistName); })) {
		return false;
	}
	return true;
}

void ScrobbleInfo::appendAsJsonTo(string &str) const
{
	str.append(u8R"({"scrobble_start_datetime":)");
	writeJsonTimestamp(scrobbleStartTimestamp, str);
	str.append(u8R"(,"scrobble_end_datetime":)");
	writeJsonTimestamp(scrobbleEndTimestamp, str);
	str.append(u8R"(,"scrobble_duration":{"amount":)");
	writeJsonLong(scrobbleDuration, str);
	str.append(u8R"(,"unit":"ms"},"track":)");
	track.appendAsJsonTo(str);
	str.append(u8"}");
}

void Track::appendAsJsonTo(string &str) const
{
	str.append(u8R"({"title":")");
	writeJsonString(m_title, str);
	// A single artist is currently supported.
	str.append(u8R"(","artists":[)");
	for (const string &artist : m_artists) {
		str.append(u8R"({"name":")");
		writeJsonString(artist, str);
		str.append(u8"\"},");
	}
	str.pop_back(); // removing the last redundant comma.
	str.append(u8"],");
	if (m_albumSet) {
		str.append(u8R"("album":{"title":")");
		writeJsonString(m_album, str);
		str.append(u8"\"");
		if (hasAlbumArtist()) {
			str.append(u8R"(,"artists":[)");
			for (const string &artist : m_albumArtists) {
				str.append(u8R"({"name":")");
				writeJsonString(artist, str);
				str.append(u8"\"},");
			}
			str.back() = u8"]"[0]; // removing the last redundant comma as well.
		}
		str.append(u8R"(},)");
	}
	str.append(u8R"("length":{"amount":)");
	writeJsonLong(m_duration, str);
	str.append(u8R"(,"unit":"ms"}})");
}
