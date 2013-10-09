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
#include <iostream>
#include <ctime>

// All strings are utf8-encoded.
class Track
{
public:
	void setTitle(const std::string &trackTitle) { m_title = trackTitle; m_titleSet = true; }
	void setArtist(const std::string &artist) { m_artist = artist; m_artistSet = true; }
	void setAlbumTitle(const std::string &albumTitle) { m_album = albumTitle; m_albumSet = true; }
	void setDurationMillis(const long duration) { m_duration = duration; m_durationSet = true; }
private:
	std::string m_title;
	std::string m_artist;
	// TODO think about supporting full album info
	std::string m_album;
	// Track duration in milliseconds.
	long m_duration;
	bool m_titleSet = false;
	bool m_artistSet = false;
	bool m_albumSet = false;
	bool m_durationSet = false;

	friend std::ostream &operator<<(std::ostream &out, const Track &track);
};

struct ScrobbleInfo
{
	// Date and time when scrobble event was initiated.
	std::time_t scrobbleStartTimestamp;
	// Date and time when scrobble event was finished.
	std::time_t scrobbleEndTimestamp;
	// Scrobble length in milliseconds.
	long scrobbleDuration;
	// Track to scrobble.
	Track track;

	friend std::ostream &operator<<(std::ostream &out, const ScrobbleInfo &scrobbleInfo);
};

class GravifonClient
{
	GravifonClient(const GravifonClient &) = delete;
	GravifonClient &operator=(const GravifonClient &) = delete;
public:
	// TODO declare exceptions
	GravifonClient(const char *scrobblerUrl, const char *username, const char *password);
	~GravifonClient() {};

	// TODO declare exceptions
	void scrobble(const ScrobbleInfo &);
private:
	std::string m_scrobblerUrl;
	std::string m_username;
	std::string m_password;
};

#endif /* GRAVIFONCLIENT_HPP_ */
