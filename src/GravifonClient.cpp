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
#include <time.h>
#include <cstdlib>
#include <cstdio>

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
		std::tm dateTime;
		/* Initialises the system time zone data. According to POSIX.1-2004, localtime() is required
		 * to behave as though tzset(3) was called, while localtime_r() does not have this requirement.
		 */
		::tzset();
		::localtime_r(&timestamp, &dateTime);

		// TODO support multi-byte system charsets. 32 could be not enough for them.
		const size_t outputSize = 32; // 25 are really used.
		char buf[outputSize];
		const size_t count = std::strftime(buf, outputSize, "%Y-%m-%dT%H:%M:%S%z", &dateTime);
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

	inline long getDataFilePath(string &dest)
	{
		// TODO Handle dir separators accurately.
		const char * const dataDir = getenv("XDG_DATA_HOME");
		if (dataDir != nullptr && dataDir[0] != '\0') {
			dest = dataDir;
		} else {
			// Trying to assign the default data dir ($HOME/.local/share/).
			const char * const homeDir = getenv("HOME");
			if (homeDir == nullptr || homeDir == '\0') {
				return 1;
			}
			dest.assign(homeDir).append("/.local/share");
		}
		dest.append("/deadbeef/gravifon_scrobbler_data");
		return 0;
	}
}

void GravifonClient::configure(const char * const scrobblerUrl, const char * const username,
		const char * const password)
{
	assert(scrobblerUrl != nullptr);
	assert(username != nullptr);
	assert(password != nullptr);

	m_scrobblerUrl = scrobblerUrl;
	m_username = username;
	m_password = password;
}

void GravifonClient::scrobble(const ScrobbleInfo &scrobbleInfo)
{
	HttpEntity request;
	request.body += u8"[";
	request.body += scrobbleInfo;
	request.body += u8"]";

	string authHeader("Authorization: Basic "); // HTTP Basic authentication is used.
	// TODO think of moving encodeBase64(string(m_username) + ':' + m_password) to the constructor to perform this once.
	// Colon (':') is not allowed to be in a username by Gravifon. This concatenation is safe.
	authHeader += encodeBase64(string(m_username) + ':' + m_password);

	request.headers.reserve(4);
	request.headers.push_back(authHeader.c_str());
	request.headers.push_back("Content-Type: application/json; charset=utf-8");
	request.headers.push_back("Accept: application/json");
	request.headers.push_back("Accept-Charset: utf-8");

	HttpEntity response;

	HttpClient client;
	// TODO Check whether or not these timeouts are enough.
	// TODO Think of making these timeouts configurable.
	if (client.send(m_scrobblerUrl, request, response, 3000L, 5000L) != 0) {
		// TODO handle error
	}

	// TODO handle response
}

void GravifonClient::storePendingScrobbles()
{
	string dataFilePath;
	if (getDataFilePath(dataFilePath) != 0) {
		// TODO Handle error.
		return;
	}
	FILE * const dataFile = fopen(dataFilePath.c_str(), "ab");
	if (dataFile == nullptr) {
		// TODO Handle error.
		return;
	}
	string buf;
	for (const ScrobbleInfo &scrobbleInfo : m_pendingScrobbles) {
		buf.resize(0);
		buf += scrobbleInfo;
		const size_t bufSize = buf.size();
		if (fwrite(buf.c_str(), sizeof(unsigned char), bufSize, dataFile) != bufSize) {
			// TODO Handle error.
			goto finish;
		}
		/* ScrobbleInfo in the JSON form does not contain the character 'line feed' ('\n')
		 * so that using the latter as a separator is safe.
		 *
		 * The file must end with the empty line.
		 */
		if (fwrite(u8"\n", sizeof(unsigned char), 1, dataFile) != 1) {
			// TODO Handle error.
			goto finish;
		}
	}
finish:
	if (fclose(dataFile) != 0) {
		// TODO Handle error.
	}
}

string &operator+=(string &str, const ScrobbleInfo &scrobbleInfo)
{
	str.append(u8R"({"scrobble_start_datetime":)");
	writeJsonTimestamp(scrobbleInfo.scrobbleStartTimestamp, str);
	str.append(u8R"(,"scrobble_end_datetime":)");
	writeJsonTimestamp(scrobbleInfo.scrobbleEndTimestamp, str);
	str.append(u8R"(,"scrobble_duration":{"amount":)");
	writeJsonLong(scrobbleInfo.scrobbleDuration, str);
	str.append(u8R"(,"unit":"ms"},"track":)");
	str += scrobbleInfo.track;
	str.append(u8"}");
	return str;
}

string &operator+=(string &str, const Track &track)
{
	str.append(u8R"({"title":")");
	writeJsonString(track.m_title, str);
	// A single artist is currently supported.
	str.append(u8R"(","artists":[)");
	for (const string &artist : track.m_artists) {
		str.append(u8R"({"name":")");
		writeJsonString(artist, str);
		str.append(u8"\"},");
	}
	str.resize(str.size()-1); // removing the last redundant comma.
	str.append(u8"],");
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
