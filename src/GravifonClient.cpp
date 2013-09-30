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
#include <cassert>

using std::string;
using std::invalid_argument;
using std::ostream;
using std::localtime;

namespace
{
	inline void writeJsonString(const string &str, ostream&out)
	{
		for (const char c : str) {
			switch (c) {
			case '\'':
			case '"':
			case '\\':
				out << '\\' << c;
				break;
			case '\b':
				out << "\\b";
				break;
			case '\f':
				out << "\\f";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				out << c;
				break;
			}
		}
	}

	// TODO support conversion to utf-8 instead of converting to the system-default encoding
	inline void writeJsonTimestamp(const std::time_t timestamp, ostream&out)
	{
		/* The datetime format as required by https://github.com/gravidence/gravifon/wiki/Date-Time
		 * Milliseconds are not supported.
		 */
		std::tm * const dateTime = localtime(&timestamp);
		if (dateTime == nullptr) {
			// TODO
		}
		const size_t outputSize = 32; // 25 are really used.
		char buf[outputSize];
		const size_t count = std::strftime(buf, outputSize, "%Y-%m-%dT%H-%M-%S%z", dateTime);
		if (count == 0) {
			// TODO If count was reached before the entire string could be stored, ​0​ is returned and the contents are undefined.
		}
		out << '"' << buf << '"';
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
	out << u8R"({"title":")";
	writeJsonString(track.trackName, out);
	// A single artist is currently supported.
	out << u8R"(","artists":[{"name":")";
	writeJsonString(track.artist, out);
	out << u8R"("}],"album":{"title":")";
	writeJsonString(track.album, out);
	out << u8R"("},"length":{"amount":)" << track.duration <<
			u8R"(,"unit":"ms"},"number":)" << track.trackNumber << '}';
	return out;
}
