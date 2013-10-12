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
#include <cassert>
#include <afc/base64.hpp>
#include <afc/utils.h>
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

	inline void writeJsonString(const string &src, string &dest)
	{
		for (const char c : src) {
			switch (c) {
			case '\'':
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

	inline void writeJsonTimestamp(const std::time_t timestamp, string &dest)
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
		dest.append(u8"\"").append(convertToUtf8(buf, systemCharset().c_str())).append(u8"\"");
	}

	// writes value to out in utf-8
	inline void writeJsonLong(const long value, string &dest)
	{
		// TODO avoid copying that is performed by ostringstream::str().
		dest.append(convertToUtf8(to_string(value), systemCharset().c_str()));
	}
}

GravifonClient::GravifonClient(const char *scrobblerUrl, const char *username, const char *password)
{
	assert(scrobblerUrl != nullptr);
	assert(username != nullptr);
	assert(password != nullptr);

	m_scrobblerUrl = scrobblerUrl;
	// TODO check if username and password are to be encoded to utf-8 here.
	m_username = username;
	m_password = password;
}

void GravifonClient::scrobble(const ScrobbleInfo &scrobbleInfo)
{
	HttpEntity request;
	request.body += scrobbleInfo;

	string authHeader("Authorization: Basic "); // HTTP Basic authentication is used.
	// TODO think what to do is the username contains colon (':').
	// TODO think of moving encodeBase64(string(m_username) + ':' + m_password) to the constructor to perform this once.
	authHeader += encodeBase64(string(m_username) + ':' + m_password);

	request.headers.reserve(4);
	request.headers.push_back(authHeader.c_str());
	request.headers.push_back("Content-Type: application/json; charset=utf-8");
	request.headers.push_back("Accept: application/json");
	request.headers.push_back("Accept-Charset: utf-8");

	HttpEntity response;

	HttpClient client;
	// TODO set timeouts
	if (client.send(m_scrobblerUrl, request, response) != 0) {
		// TODO handle error
	}

	// TODO handle response
}

string &operator+=(string &str, const ScrobbleInfo &scrobbleInfo)
{
	str.append(u8R"({"scrobble_start_datetime":)");
	writeJsonTimestamp(scrobbleInfo.scrobbleStartTimestamp, str);
	str.append(u8R"(,"scrobble_end_datetime":)");
	writeJsonTimestamp(scrobbleInfo.scrobbleEndTimestamp, str);
	str.append(u8R"(,"duration":{"amount":)");
	writeJsonLong(scrobbleInfo.scrobbleDuration, str);
	str.append(u8R"(,"unit":"ms"},"track":)");
	str += scrobbleInfo.track;
	return str;
}

string &operator+=(string &str, const Track &track)
{
	str.append(u8R"({"title":")");
	writeJsonString(track.m_title, str);
	// A single artist is currently supported.
	str.append(u8R"(","artists":[{"name":")");
	writeJsonString(track.m_artist, str);
	str.append(u8R"("}],)");
	if (track.m_albumSet) {
		str.append(u8R"("album":{"title":")");
		writeJsonString(track.m_album, str);
		str.append(u8R"("},)");
	}
	str.append(u8R"("length":{"amount":)");
	writeJsonLong(track.m_duration, str);
	str.append(u8R"(,"unit":"ms"}})");
	return str;
}
