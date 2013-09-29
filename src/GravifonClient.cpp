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
#include "GravifonClient.hpp"
#include <stdexcept>

using std::string;
using std::invalid_argument;
using std::ostream;

namespace
{
	inline void writeJsonString(const string &str, ostream&out)
	{
		// TODO escape strings
		out << str;
	}

	inline void writeJsonTimestamp(const long timestamp, ostream&out)
	{
		// TODO format it as required by https://github.com/gravidence/gravifon/wiki/Date-Time
		out << '"' << timestamp << '"';
	}
}

GravifonClient::GravifonClient(const char *scrobblerUrl, const char *username, const char *password)
{
	if (scrobblerUrl == nullptr) {
		throw invalid_argument("scrobblerUrl");
	}
	if (username == nullptr) {
		throw invalid_argument("username");
	}
	if (password == nullptr) {
		throw invalid_argument("password");
	}

	m_scrobblerUrl = scrobblerUrl;
	m_username = username;
	m_username = password;
}

void GravifonClient::scrobble(const ScrobbleInfo &scrobbleInfo)
{
	// TODO implement me
}

ostream &operator<<(ostream &out, const ScrobbleInfo &scrobbleInfo)
{
	out << "{\"scrobble_start_datetime\":";
	writeJsonTimestamp(scrobbleInfo.scrobbleStartTimestamp, out);
	out << ",\"scrobble_end_datetime\":";
	writeJsonTimestamp(scrobbleInfo.scrobbleEndTimestamp, out);
	out << ",\"duration\":{\"amount\":" << scrobbleInfo.scrobbleDuration <<
			",\"unit\":\"ms\"},\"track\":" << scrobbleInfo.track;
	return out;
}

ostream &operator<<(ostream &out, const Track &track)
{
	out << "{\"title\":\"";
	writeJsonString(track.trackName, out);
	// A single artist is currently supported.
	out << "\",\"artists\":[\"";
	writeJsonString(track.artist, out);
	out << "\"],\"album\":{\"title\":\"";
	writeJsonString(track.album, out);
	out << "\"},\"length\":{\"amount\":" << track.duration << ",\"unit\":\"ms\"},\"number\":" << track.trackNumber;
	return out;
}
