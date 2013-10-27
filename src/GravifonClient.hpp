/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013 Dźmitry Laŭčuk

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
#ifndef GRAVIFONCLIENT_HPP_
#define GRAVIFONCLIENT_HPP_

#include <string>
#include <vector>
#include <list>
#include <ctime>
#include <mutex>

// All strings are utf8-encoded.
class Track
{
public:
	void setTitle(const std::string &trackTitle) { m_title = trackTitle; m_titleSet = true; }
	void addArtist(const std::string &artist) { m_artists.emplace_back(artist); m_artistSet = true; }
	void addAlbumArtist(const std::string &artist) { m_albumArtists.emplace_back(artist); m_albumArtistSet = true; }
	void setAlbumTitle(const std::string &albumTitle) { m_album = albumTitle; m_albumSet = true; }
	void setDurationMillis(const long duration) { m_duration = duration; m_durationSet = true; }
private:
	std::string m_title;
	std::vector<std::string> m_artists;
	std::vector<std::string> m_albumArtists;
	std::string m_album;
	// Track duration in milliseconds.
	long m_duration;
	bool m_titleSet = false;
	bool m_artistSet = false;
	bool m_albumArtistSet = false;
	bool m_albumSet = false;
	bool m_durationSet = false;

	friend std::string &operator+=(std::string &str, const Track &track);
};

struct ScrobbleInfo
{
	ScrobbleInfo() = default;
	ScrobbleInfo(const ScrobbleInfo &) = default;
	ScrobbleInfo(ScrobbleInfo &&) = default;

	ScrobbleInfo &operator=(const ScrobbleInfo &) = default;
	ScrobbleInfo &operator=(ScrobbleInfo &&) = default;

	static bool parse(const std::string &str, ScrobbleInfo &dest);

	// Date and time when scrobble event was initiated.
	std::time_t scrobbleStartTimestamp;
	// Date and time when scrobble event was finished.
	std::time_t scrobbleEndTimestamp;
	// Scrobble length in milliseconds.
	long scrobbleDuration;
	// Track to scrobble.
	Track track;

	friend std::string &operator+=(std::string &str, const ScrobbleInfo &scrobbleInfo);
};

// TODO make GravifonClient thread-safe.
// TODO think if GravifonClient's destructor should use lock_guard<mutex> lock(m_mutex);
class GravifonClient
{
	GravifonClient(const GravifonClient &) = delete;
	GravifonClient(GravifonClient &&) = delete;
	GravifonClient &operator=(const GravifonClient &) = delete;
	GravifonClient &operator=(GravifonClient &&) = delete;
public:
	GravifonClient() {};
	~GravifonClient() {};

	// username and password are to be in UTF-8; gravifonUrl is to be in the system encoding.
	void configure(const char *gravifonUrl, const char *username, const char *password);

	void scrobble(const ScrobbleInfo &);

	bool loadPendingScrobbles();
	bool storePendingScrobbles();
private:
	std::string m_scrobblerUrl;
	std::string m_username;
	std::string m_password;

	std::list<ScrobbleInfo> m_pendingScrobbles;

	std::mutex m_mutex;
};

#endif /* GRAVIFONCLIENT_HPP_ */
