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
#include <afc/utils.h>
#include <sstream>
#include "HttpClient.hpp"

using namespace std;
using namespace afc;

namespace
{
	static_assert('\'' == u8"'"[0], "An ASCII-incompatible basic charset is used.");
	static_assert('"' == u8"\""[0], "An ASCII-incompatible basic charset is used.");
	static_assert('\\' == u8"\\"[0], "An ASCII-incompatible basic charset is used.");
	static_assert('\b' == u8"\b"[0], "An ASCII-incompatible basic charset is used.");
	static_assert('\f' == u8"\f"[0], "An ASCII-incompatible basic charset is used.");
	static_assert('\n' == u8"\n"[0], "An ASCII-incompatible basic charset is used.");
	static_assert('\r' == u8"\r"[0], "An ASCII-incompatible basic charset is used.");
	static_assert('\t' == u8"\t"[0], "An ASCII-incompatible basic charset is used.");

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
				out << u8"\\b";
				break;
			case '\f':
				out << u8"\\f";
				break;
			case '\n':
				out << u8"\\n";
				break;
			case '\r':
				out << u8"\\r";
				break;
			case '\t':
				out << u8"\\t";
				break;
			default:
				out << c;
				break;
			}
		}
	}

	inline void writeJsonTimestamp(const std::time_t timestamp, ostream&out)
	{
		/* The datetime format as required by https://github.com/gravidence/gravifon/wiki/Date-Time
		 * Milliseconds are not supported.
		 */
		std::tm * const dateTime = localtime(&timestamp);
		if (dateTime == nullptr) {
			// TODO
		}
		// TODO support multi-byte system charsets. 32 could be not enough for them.
		const size_t outputSize = 32; // 25 are really used.
		char buf[outputSize];
		const size_t count = std::strftime(buf, outputSize, "%Y-%m-%dT%H-%M-%S%z", dateTime);
		if (count == 0) {
			// TODO If count was reached before the entire string could be stored, ​0​ is returned and the contents are undefined.
		}
		out << '"' << convertToUtf8(buf, systemCharset().c_str()) << '"';
	}

	// writes value to out in utf-8
	inline void writeJsonLong(const long value, ostream&out)
	{
		ostringstream buf;
		buf << value;

		// TODO avoid copying that is performed by ostringstream::str().
		out << convertToUtf8(buf.str(), systemCharset().c_str());
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
	ostringstream buf;
	buf << scrobbleInfo;

	HttpRequest request;
	request.url = &m_scrobblerUrl;
	// TODO avoid copying that is performed by ostringstream::str().
	string body = buf.str();
	request.body = &body;

	request.headers.reserve(3);
	request.headers.push_back("Content-Type: application/json; charset=utf-8");
	request.headers.push_back("Accept: application/json");
	request.headers.push_back("Accept-Charset: utf-8");

	HttpClient client;
	// TODO set timeouts
	// TODO handle response
	string response = client.send(request);
}

ostream &operator<<(ostream &out, const ScrobbleInfo &scrobbleInfo)
{
	out << "{\"scrobble_start_datetime\":";
	writeJsonTimestamp(scrobbleInfo.scrobbleStartTimestamp, out);
	out << ",\"scrobble_end_datetime\":";
	writeJsonTimestamp(scrobbleInfo.scrobbleEndTimestamp, out);
	out << ",\"duration\":{\"amount\":";
	writeJsonLong(scrobbleInfo.scrobbleDuration, out);
	out << ",\"unit\":\"ms\"},\"track\":" << scrobbleInfo.track;
	return out;
}

ostream &operator<<(ostream &out, const Track &track)
{
	out << u8R"({"title":")";
	writeJsonString(track.m_title, out);
	// A single artist is currently supported.
	out << u8R"(","artists":[{"name":")";
	writeJsonString(track.m_artist, out);
	out << u8R"("}],)";
	if (track.m_albumSet) {
		out << u8R"("album":{"title":")";
		writeJsonString(track.m_album, out);
		out << u8R"("},)";
	}
	out << u8R"("length":{"amount":)";
	writeJsonLong(track.m_duration, out);
	out << u8R"(,"unit":"ms"}})";
	return out;
}
