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
#include "dateutil.hpp"
#include "HttpClient.hpp"
#include <time.h>
#include <cstdlib>
#include <cstdio>
#include <jsoncpp/json/reader.h>
#include <sys/stat.h>
#include "logger.hpp"

using namespace std;
using namespace afc;
using Json::Value;

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
		::gmtime_r(&timestamp, &dateTime);

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

	inline bool parseDateTime(const Value &dateTimeObject, time_t &dest)
	{
		return dateTimeObject.isString() && parseISODateTime(dateTimeObject.asString(), dest);
	}

	inline bool parseDuration(const Value &durationObject, long &dest)
	{
		if (!durationObject.isObject() ||
				!durationObject.isMember("amount") ||
				!durationObject.isMember("unit")) {
			return false;
		}
		const Value &amount = durationObject["amount"];
		const Value &unit = durationObject["unit"];
		if (!amount.isInt() || !unit.isString()) {
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
}

void GravifonClient::configure(const char * const scrobblerUrl, const char * const username,
		const char * const password)
{ lock_guard<mutex> lock(m_mutex);
	assert(scrobblerUrl != nullptr);
	assert(username != nullptr);
	assert(password != nullptr);

	m_scrobblerUrl = scrobblerUrl;
	m_username = username;
	m_password = password;
}

void GravifonClient::scrobble(const ScrobbleInfo &scrobbleInfo)
{ lock_guard<mutex> lock(m_mutex);
	m_pendingScrobbles.emplace_back(scrobbleInfo);

	// TODO move this functionality to a different thread.
	HttpEntity request;
	string &body = request.body;
	body += u8"[";

	// Adding up to 20 scrobbles to the request.
	// 20 is the max number of scrobbles in a single request.
	int submittedCount = 0;
	for (auto it = m_pendingScrobbles.begin(), end = m_pendingScrobbles.end();
			submittedCount < 20 && it != end; ++submittedCount, ++it) {
		body += *it;
		body += u8",";
	}
	body.pop_back(); // Removing the redundant comma.
	body += u8"]";

	string authHeader("Authorization: Basic "); // HTTP Basic authentication is used.
	// TODO think of moving encodeBase64(string(m_username) + ':' + m_password) to the constructor to perform this once.
	// Colon (':') is not allowed to be in a username by Gravifon. This concatenation is safe.
	authHeader += encodeBase64(string(m_username) + ':' + m_password);

	request.headers.reserve(4);
	request.headers.push_back(authHeader.c_str());
	request.headers.push_back("Content-Type: application/json; charset=utf-8");
	request.headers.push_back("Accept: application/json");
	request.headers.push_back("Accept-Charset: utf-8");

	logDebug(string("[GravifonClient] Request body: ") + request.body);

	HttpResponseEntity response;

	HttpClient client;
	// TODO Check whether or not these timeouts are enough.
	// TODO Think of making these timeouts configurable.
	if (client.send(m_scrobblerUrl, request, response, 3000L, 5000L) != HttpClient::StatusCode::SUCCESS) {
		// TODO Handle error.
		return;
	}

	logDebug(string("[GravifonClient] Response status code: ") + to_string(response.statusCode) +
			"; response body: " + response.body);

	if (response.statusCode != 200) {
		// TODO Handle error (probably distinguish different status codes).
		return;
	}

	Json::Reader jsonReader;
	Value object;
	if (!jsonReader.parse(response.body, object, false)) {
		fprintf(stderr, "[GravifonClient] Invalid response: %s", response.body.c_str());
		return;
	}
	if (object.isObject()) { // if the response is a global error.
		if (!object.isMember("ok")) {
			fprintf(stderr, "[GravifonClient] Invalid response: %s", response.body.c_str());
			return;
		}
		const Value &success = object["ok"];
		if (!success.isBool()) {
			fprintf(stderr, "[GravifonClient] Invalid response: %s", response.body.c_str());
			return;
		}
		if (success.asBool() == true) {
			fprintf(stderr, "[GravifonClient] Unexpected 'ok' global status response: %s", response.body.c_str());
			return;
		}
		// TODO Report error (use status' error code and error description fields).
	}
	if (!object.isArray() || object.size() != Json::ArrayIndex(submittedCount)) {
		fprintf(stderr, "[GravifonClient] Invalid response: %s", response.body.c_str());
		return;
	}
	auto it = m_pendingScrobbles.begin();
	for (Json::ArrayIndex i = 0, n = object.size(); i < n; ++i) {
		const Value &status = object[i];
		if (!status.isObject() || !status.isMember("ok")) {
			fprintf(stderr, "[GravifonClient] Invalid response: %s", response.body.c_str());
			++it;
			continue;
		}
		const Value &success = status["ok"];
		if (!success.isBool()) {
			fprintf(stderr, "[GravifonClient] Invalid response: %s", response.body.c_str());
			++it;
			continue;
		}
		// If the track is scrobbled successfully then it is removed from the queue.
		if (success.asBool() == true) {
			it = m_pendingScrobbles.erase(it);
		} else {
			// TODO Report error (use status' error code and error description fields).
			++it;
		}
	}
}

// TODO Do not load all scrobbles to memory. Use mmap for this?
bool GravifonClient::loadPendingScrobbles()
{ lock_guard<mutex> lock(m_mutex);
	string dataFilePath;
	if (getDataFilePath(dataFilePath) != 0) {
		return false;
	}

	/* - if the file does not exist then return success (i.e. there are no pending scrobbles)
	 * - if he file exists but it is not a regular file or a symbolic link then return failure
	 *   because such a file cannot be used to store pending scrobbles
	 * - if the file exists and it a regular file or a symbolic link then proceed with loading
	 *   pending scrobbles stored in it
	 */
	struct stat fileStatus;
	if (stat(dataFilePath.c_str(), &fileStatus) != 0) {
		return errno == ENOTDIR || errno == ENOENT;
	} else if (!(S_ISREG(fileStatus.st_mode) || S_ISLNK(fileStatus.st_mode))) {
		return false;
	}

	FILE * const dataFile = fopen(dataFilePath.c_str(), "rb");
	if (dataFile == nullptr) {
		return false;
	}

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
	return result;
}

bool GravifonClient::storePendingScrobbles()
{ lock_guard<mutex> lock(m_mutex);
	string dataFilePath;
	if (getDataFilePath(dataFilePath) != 0) {
		return false;
	}

	/* - if the file or some parent directories do not exist then create missing parts.
	 * - if he file exists but it is not a regular file or a symbolic link then return failure
	 *   because such a file cannot be used to store pending scrobbles
	 * - if the file exists and it a regular file or a symbolic link then proceed with storing
	 *   pending scrobbles into it
	 */
	struct stat fileStatus;
	if (stat(dataFilePath.c_str(), &fileStatus) != 0) {
		if (errno == ENOTDIR || errno == ENOENT) {
			if (!createParentDirs(dataFilePath)) {
				return false;
			}
		} else {
			return false;
		}
	} else if (!(S_ISREG(fileStatus.st_mode) || S_ISLNK(fileStatus.st_mode))) {
		return false;
	}

	// TODO Create the parent directory if it does not exist.
	/* The assumption that all tracks are loaded into the list of pending scrobbles
	 * so that the file could be overwritten with the remaining pending scrobbles.
	 */
	FILE * const dataFile = fopen(dataFilePath.c_str(), "wb");
	if (dataFile == nullptr) {
		return false;
	}

	bool result = true;

	string buf;
	for (const ScrobbleInfo &scrobbleInfo : m_pendingScrobbles) {
		buf.resize(0);
		buf += scrobbleInfo;
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
	if (fclose(dataFile) != 0) {
		result = false;
	}
	return result;
}

bool ScrobbleInfo::parse(const string &str, ScrobbleInfo &dest)
{
	Json::Reader jsonReader;
	Value object;
	if (!jsonReader.parse(str, object, false)) {
		// TODO Handle error (use jsonReader.getFormattedErrorMessages()).
		return false;
	}
	if (!object.isObject() ||
			!object.isMember("scrobble_start_datetime") ||
			!object.isMember("scrobble_end_datetime") ||
			!object.isMember("scrobble_duration") ||
			!object.isMember("track")) {
		return false;
	}
	if (!parseDateTime(object["scrobble_start_datetime"], dest.scrobbleStartTimestamp) ||
			!parseDateTime(object["scrobble_end_datetime"], dest.scrobbleEndTimestamp) ||
			!parseDuration(object["scrobble_duration"], dest.scrobbleDuration)) {
		return false;
	}

	const Value trackObject = object["track"];
	if (!trackObject.isObject() ||
			!trackObject.isMember("title") ||
			!trackObject.isMember("artists") ||
			!trackObject.isMember("length")) {
		return false;
	}
	const Value &trackTitle = trackObject["title"];
	if (!trackTitle.isString()) {
		return false;
	}
	dest.track.setTitle(trackTitle.asString());

	if (trackObject.isMember("album")) {
		const Value &trackAlbum = trackObject["album"];
		if (!trackAlbum.isObject() ||
				!trackAlbum.isMember("title")) {
			return false;
		}
		const Value &trackAlbumTitle = trackAlbum["title"];
		if (!trackAlbumTitle.isString()) {
			return false;
		}
		dest.track.setAlbumTitle(trackAlbumTitle.asString());
	}

	long trackDuration;
	if (!parseDuration(trackObject["length"], trackDuration)) {
		return false;
	}
	dest.track.setDurationMillis(trackDuration);

	const Value &trackArtists = trackObject["artists"];
	if (!trackArtists.isArray() || trackArtists.empty()) {
		return false;
	}
	for (Json::ArrayIndex i = 0, n = trackArtists.size(); i < n; ++i) {
		const Value &trackArtist = trackArtists[i];
		if (!trackArtist.isObject() ||
				!trackArtist.isMember("name")) {
			return false;
		}
		const Value &trackArtistName = trackArtist["name"];
		if (!trackArtistName.isString()) {
			return false;
		}
		dest.track.addArtist(trackArtistName.asString());
	}
	return true;
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
