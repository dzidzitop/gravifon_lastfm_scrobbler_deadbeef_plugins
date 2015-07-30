/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2014-2015 Dźmitry Laŭčuk

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
#ifndef LASTFMSCROBBLER_HPP_
#define LASTFMSCROBBLER_HPP_

#include "ScrobbleInfo.hpp"
#include "Scrobbler.hpp"
#include <cstddef>
#include <deque>
#include <mutex>
#include <string>
#include <utility>

#include <afc/ensure_ascii.hpp>
#include <afc/SimpleString.hpp>

/* For Last.fm submission API v1.2.1 the scrobbles submitted are either all accepted or
 * all rejected, so they are removed all at once strictly from the front of the queue.
 * In this case deque is the best choice to store pending scrobbles in memory.
 */
class LastfmScrobbler : public Scrobbler<std::deque<ScrobbleInfo>>
{
public:
	LastfmScrobbler() : Scrobbler(), m_scrobblerUrl(), m_username(), m_password(), m_dataFilePath(),
			m_sessionId(), m_submissionUrl(), m_nowPlayingTrack(), m_authenticated(false),
			m_hasNowPlayingTrack(false)
	{ std::lock_guard<std::mutex> lock(m_mutex); // synchronising memory
		/* This instance is partially initialised here. It will be initialised completely
		 * when ::start() is invoked successfully.
		 */
	}

	virtual ~LastfmScrobbler()
	{
		// Synchronising memory before destructing the member fields of this GravifonScrobbler.
		std::lock_guard<std::mutex> lock(m_mutex);
	}

	void playStarted(Track &&track)
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_nowPlayingTrack = std::move(track);
		m_hasNowPlayingTrack = true;
		m_cv.notify_one();
	}

	// username and password should be in UTF-8; serverUrl must be in ASCII.
	void configure(const char *serverUrl, std::size_t serverUrlSize, const char *username, const char *password);

	void setDataFilePath(afc::SimpleString &dataFilePath)
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_dataFilePath = dataFilePath;
	}

	void setDataFilePath(afc::SimpleString &&dataFilePath)
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_dataFilePath = std::move(dataFilePath);
	}
protected:
	virtual std::size_t doScrobbling() override;
	/* Since the worker thread can receive awaking events from ::scrobble() and ::playStarted()
	 * the now-playing submission event can be submitted while the scrobbler is processing
	 * scrobbles (outside the critical secion on m_mutex). To prevent the now-playing event lost
	 * during this routine the scrobbler checks if there is the now-playing track reported
	 * before and after the worker thread falls asleep.
	 */
	virtual void preSleep() override { submitNowPlayingTrack(); }
	virtual void postSleep() override { submitNowPlayingTrack(); }

	virtual const afc::SimpleString &getDataFilePath() const override { return m_dataFilePath; }

	virtual void stopExtra() override;
private:
	// All these functions must be invoked within the critical section upon Scrobbler::m_mutex.
	bool ensureAuthenticated();
	void deauthenticate() noexcept;
	void notifyPlayStarted();

	void submitNowPlayingTrack();

	std::string m_scrobblerUrl;
	std::string m_username;
	std::string m_password;

	afc::SimpleString m_dataFilePath;

	std::string m_sessionId;
	std::string m_submissionUrl;
	std::string m_nowPlayingUrl;

	Track m_nowPlayingTrack;

	bool m_authenticated;
	bool m_hasNowPlayingTrack;
};

#endif /* LASTFMSCROBBLER_HPP_ */
