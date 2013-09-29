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

// All strings are utf8-encoded.
struct Track
{
	std::string trackName;
	std::string artist;
	// TODO think about supporting full album info
	std::string album;
	long duration;
	long trackNumber;

	friend std::ostream &operator<<(std::ostream &out, const Track &track);
};

struct ScrobbleInfo
{
	// Date and time when scrobble event was initiated.
	long scrobbleStartTimestamp;
	// Date and time when scrobble event was finished.
	long scrobbleEndTimestamp;
	// Scrobble length.
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
