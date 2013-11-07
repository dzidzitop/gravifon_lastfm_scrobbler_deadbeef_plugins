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
#include <cerrno>
#include <jsoncpp/json/reader.h>
#include <sys/stat.h>
#include "logger.hpp"
#include <functional>

using namespace std;
using namespace afc;
using Json::Value;
using Json::ValueType;
using StatusCode = HttpClient::StatusCode;

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

	static const ValueType nullValue = ValueType::nullValue;
	static const ValueType objectValue = ValueType::objectValue;
	static const ValueType arrayValue = ValueType::arrayValue;
	static const ValueType stringValue = ValueType::stringValue;
	static const ValueType intValue = ValueType::intValue;
	static const ValueType booleanValue = ValueType::booleanValue;

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
		dest.append(convertToUtf8(to_string(value), systemCharset().c_str()));
	}

	// Removes trailing slash if it is not the only character in the path.
	inline void appendToPath(string &path, const char *child)
	{
		const bool needsSeparator = path.size() > 1 && path.back() != '/';
		const bool childHasSeparator = child[0] == '/';
		if (needsSeparator) {
			if (!childHasSeparator) {
				path += '/';
			}
		} else {
			if (childHasSeparator) {
				/* This works even if the path is an empty string.
				 * Child's heading separator is deleted.
				 */
				++child;
			}
		}
		path += child;
	}

	inline long getDataFilePath(string &dest)
	{
		const char * const dataDir = getenv("XDG_DATA_HOME");
		if (dataDir != nullptr && dataDir[0] != '\0') {
			dest = dataDir;
		} else {
			// Trying to assign the default data dir ($HOME/.local/share/).
			const char * const homeDir = getenv("HOME");
			if (homeDir == nullptr || homeDir == '\0') {
				return 1;
			}
			dest = homeDir;
			appendToPath(dest, ".local/share");
		}
		appendToPath(dest, "deadbeef/gravifon_scrobbler_data");
		return 0;
	}

	inline bool isType(const Value &val, const ValueType type)
	{
		return val.type() == type;
	}

	inline bool parseDateTime(const Value &dateTimeObject, time_t &dest)
	{
		return isType(dateTimeObject, stringValue) && parseISODateTime(dateTimeObject.asString(), dest);
	}

	inline bool parseDuration(const Value &durationObject, long &dest)
	{
		if (!isType(durationObject, objectValue)) {
			return false;
		}
		const Value &amount = durationObject["amount"];
		const Value &unit = durationObject["unit"];
		if (!isType(amount, intValue) || !isType(unit, stringValue)) {
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

	template<typename AddArtistOp>
	inline bool parseArtists(const Value &artists, const bool artistsRequired, AddArtistOp addArtistOp)
	{
		if (!isType(artists, arrayValue) || artists.empty()) {
			return !artistsRequired;
		}
		for (auto i = 0u, n = artists.size(); i < n; ++i) {
			const Value &artist = artists[i];
			if (!isType(artist, objectValue)) {
				return false;
			}
			const Value &artistName = artist["name"];
			if (!isType(artistName, stringValue)) {
				return false;
			}
			addArtistOp(artistName.asString());
		}
		return true;
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

	inline void reportHttpClientError(const StatusCode result)
	{
		assert(result != StatusCode::SUCCESS);
		const char *message;
		switch (result) {
		case StatusCode::UNABLE_TO_CONNECT:
			message = "unable to connect";
			break;
		case StatusCode::OPERATION_TIMEOUT:
			message = "sending the scrobble message has timed out";
			break;
		default:
			message = "unknown error";
		}
		fprintf(stderr, "[GravifonClient] Unable to send the scrobble message: %s\n", message);
	}

	template<typename SuccessOp, typename FailureOp, typename InvalidOp>
	inline void processStatus(const Value &status, SuccessOp successOp, FailureOp failureOp, InvalidOp invalidOp)
	{
		if (isType(status, objectValue)) {
			const Value &success = status["ok"];
			if (isType(success, booleanValue)) {
				if (success.asBool() == true) {
					successOp();
					return;
				} else {
					const Value &errCodeValue = status["error_code"];
					if (isType(errCodeValue, intValue)) {
						const unsigned long errorCode = static_cast<unsigned long>(errCodeValue.asInt());
						const Value &errorDescription = status["error_description"];
						if (isType(errorDescription, stringValue)) {
							failureOp(errorCode, errorDescription.asString());
							return;
						} else if (isType(errorDescription, nullValue)) {
							failureOp(errorCode, string());
							return;
						} // else invalid error description type
					}
				}
			}
		}
		invalidOp();
	}

	inline bool isRecoverableError(const unsigned long errorCode)
	{
		return errorCode < 10000 || errorCode == 10003 || errorCode > 10005;
	}
}

void GravifonClient::configure(const char * const gravifonUrl, const string &username, const string &password)
{ lock_guard<mutex> lock(m_mutex);
	assert(gravifonUrl != nullptr);

	m_scrobblerUrl = gravifonUrl;
	if (!m_scrobblerUrl.empty()) {
		appendToPath(m_scrobblerUrl, "scrobbles");
	}

	// Curl expects the basic charset in headers.
	m_authHeader = "Authorization: Basic "; // HTTP Basic authentication is used.

	/* Colon (':') is not allowed to be in a username by Gravifon. This concatenation is safe.
	 * In addition, the character 'colon' in UTF-8 is equivalent to those in ISO-8859-1.
	 */
	m_authHeader += encodeBase64(username + u8":"[0] + password);

	m_configured = true;
}

void GravifonClient::scrobble(const ScrobbleInfo &scrobbleInfo)
{ lock_guard<mutex> lock(m_mutex);
	if (!m_started) {
		// This GravifonClient is not started or is already stopped.
		return;
	}

	m_pendingScrobbles.emplace_back(scrobbleInfo);

	m_cv.notify_one();
}

void GravifonClient::backgroundScrobbling()
{ unique_lock<mutex> lock(m_mutex);
	for (;;) {
		while (m_pendingScrobbles.empty()) {
			m_cv.wait(lock);
			if (!m_started) { // if stopped
				return;
			}
		}

		doScrobbling();
	}
}

inline void GravifonClient::doScrobbling()
{
	if (!m_configured) {
		logError("Gravifon client is not configured properly.");
		return;
	}

	if (m_scrobblerUrl.empty()) {
		/* There is no sense to try to send a request because the URL to Gravifon API
		 * is undefined. The scrobble is added to the list of pending scrobbles
		 * (see above) to be submitted * (among other scrobbles) when the URL is configured
		 * to point to an instance of Gravifon.
		 */
		logError("URL to Gravifon API is undefined.");
		return;
	}

	HttpEntity request;
	string &body = request.body;
	body += u8"[";

	// Adding up to 20 scrobbles to the request.
	// 20 is the max number of scrobbles in a single request.
	unsigned submittedCount = 0;
	for (auto it = m_pendingScrobbles.begin(), end = m_pendingScrobbles.end();
			submittedCount < 20 && it != end; ++submittedCount, ++it) {
		body += *it;
		body += u8",";
	}
	body.pop_back(); // Removing the redundant comma.
	body += u8"]";

	request.headers.reserve(4);
	request.headers.push_back(m_authHeader.c_str());
	// Curl expects the basic charset in headers.
	request.headers.push_back("Content-Type: application/json; charset=utf-8");
	request.headers.push_back("Accept: application/json");
	request.headers.push_back("Accept-Charset: utf-8");

	logDebug(string("[GravifonClient] Request body: ") + request.body);

	HttpResponseEntity response;

	HttpClient client;

	// TODO unlock mutex while making this HTTP call to allow for better concurrency.
	// TODO Check whether or not these timeouts are enough.
	// TODO Think of making these timeouts configurable.
	const StatusCode result = client.send(m_scrobblerUrl, request, response, 3000L, 5000L);
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return;
	}

	logDebug(string("[GravifonClient] Response status code: ") + to_string(response.statusCode));

	const string &responseBody = response.body;

	Value rs;
	if (!Json::Reader().parse(responseBody, rs, false)) {
		fprintf(stderr, "[GravifonClient] Invalid response: '%s'.\n", responseBody.c_str());
		return;
	}

	if (response.statusCode == 200) {
		// An array of status entities is expected for a 200 response, one per scrobble submitted.
		if (!isType(rs, arrayValue) || rs.size() != submittedCount) {
			fprintf(stderr, "[GravifonClient] Invalid response: '%s'.\n", response.body.c_str());
			return;
		}

		auto it = m_pendingScrobbles.begin();
		for (auto i = 0u, n = rs.size(); i < n; ++i) {
			processStatus(rs[i],
					// Successful status: if the track is scrobbled successfully then it is removed from the list.
					[&responseBody, &it, this]()
					{
						logDebug(string("[GravifonClient] Successful response: ") + responseBody);
						it = m_pendingScrobbles.erase(it);
					},

					/* Error status. If the error is unprocessable then the scrobble is removed from the list;
					 * otherwise another attempt will be done to submit it.
					 */
					[&responseBody, &it, this](const unsigned long errorCode, const string &errorDescription)
					{
						string scrobbleAsStr;
						scrobbleAsStr += *it;
						if (isRecoverableError(errorCode)) {
							fprintf(stderr, "[GravifonClient] Scrobble '%s' is not processed. "
									"Error: '%s' (%lu). It will be re-submitted later.\n",
									scrobbleAsStr.c_str(), errorDescription.c_str(), errorCode);
							++it;
						} else {
							fprintf(stderr, "[GravifonClient] Scrobble '%s' cannot be processed. "
									"Error: '%s' (%lu). It is removed as non-processable.\n",
									scrobbleAsStr.c_str(), errorDescription.c_str(), errorCode);
							it = m_pendingScrobbles.erase(it);
						}
					},

					// Invalid status: report an error and leave the scrobble in the list of pending scrobbles.
					[&responseBody, &it]()
					{
						fprintf(stderr, "[GravifonClient] Invalid response: '%s'.\n", responseBody.c_str());
						++it;
					});
		}
	} else {
		// A global status entity is expected for a non-200 response.
		processStatus(rs,
				// Success status. It is not expected. Report an error and finish processing.
				[&responseBody]()
				{
					fprintf(stderr, "[GravifonClient] Unexpected 'ok' global status response: '%s'.\n",
							responseBody.c_str());
				},

				// Error status: report an error and finish processing.
				[&responseBody](const unsigned long errorCode, const string &errorDescription)
				{
					fprintf(stderr, "[GravifonClient] Error global status response: '%s'. Error: '%s' (%lu).\n",
							responseBody.c_str(), errorDescription.c_str(), errorCode);
				},

				// Invalid status: report an error and finish processing.
				[&responseBody]()
				{
					fprintf(stderr, "[GravifonClient] Invalid response: '%s'.\n", responseBody.c_str());
				});
	}
}

bool GravifonClient::start()
{ lock_guard<mutex> lock(m_mutex);
	if (m_started) {
		// This GravifonClient is already started.
		return false;
	}

	if (!loadPendingScrobbles()) {
		return false;
	}

	m_scrobblingThread = thread(&GravifonClient::backgroundScrobbling, ref(*this));

	m_started = true;
	return true;
}

bool GravifonClient::stop()
{
	{ lock_guard<mutex> lock(m_mutex);
		if (!m_started) {
			// This GravifonClient is not started or is already stopped.
			return false;
		}

		m_started = false;

		// Waking up the scrobbing thread to let it finish quickly.
		m_cv.notify_one();
	}

	m_scrobblingThread.join();
	logDebug("[GravifonClient] The scrobbling thread is stopped.");

	{ lock_guard<mutex> lock(m_mutex);
		return storePendingScrobbles();
	}
}

// TODO Do not load all scrobbles to memory. Use mmap for this?
bool GravifonClient::loadPendingScrobbles()
{
	logDebug("[GravifonClient] Loading pending scrobbles...");
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

	logDebug("[GravifonClient] Pending scrobbles loaded: " + to_string(m_pendingScrobbles.size()));
	return result;
}

bool GravifonClient::storePendingScrobbles()
{
	logDebug("[GravifonClient] Storing pending scrobbles...");
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

	logDebug("[GravifonClient] Pending scrobbles stored: " + to_string(m_pendingScrobbles.size()));
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
	if (!isType(object, objectValue)) {
		return false;
	}
	if (!parseDateTime(object["scrobble_start_datetime"], dest.scrobbleStartTimestamp) ||
			!parseDateTime(object["scrobble_end_datetime"], dest.scrobbleEndTimestamp) ||
			!parseDuration(object["scrobble_duration"], dest.scrobbleDuration)) {
		return false;
	}

	Track &track = dest.track;

	const Value trackObject = object["track"];
	if (!isType(trackObject, objectValue)) {
		return false;
	}
	const Value &trackTitle = trackObject["title"];
	if (!isType(trackTitle, stringValue)) {
		return false;
	}
	track.setTitle(trackTitle.asString());

	const Value &trackAlbum = trackObject["album"];
	const ValueType trackAlbumObjType = trackAlbum.type();
	if (trackAlbumObjType != nullValue) {
		if (trackAlbumObjType != objectValue) {
			return false;
		}
		const Value &trackAlbumTitle = trackAlbum["title"];
		if (!isType(trackAlbumTitle, stringValue)) {
			return false;
		}
		track.setAlbumTitle(trackAlbumTitle.asString());

		if (!parseArtists(trackAlbum["artists"], false,
				[&](string &&artistName) { track.addAlbumArtist(artistName); })) {
			return false;
		}
	}

	long trackDuration;
	if (!parseDuration(trackObject["length"], trackDuration)) {
		return false;
	}
	track.setDurationMillis(trackDuration);

	if (!parseArtists(trackObject["artists"], true,
			[&](string &&artistName) { track.addArtist(artistName); })) {
		return false;
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
	str.pop_back(); // removing the last redundant comma.
	str.append(u8"],");
	if (track.m_albumSet) {
		str.append(u8R"("album":{"title":")");
		writeJsonString(track.m_album, str);
		str.append(u8"\"");
		if (track.m_albumArtistSet) {
			str.append(u8R"(,"artists":[)");
			for (const string &artist : track.m_albumArtists) {
				str.append(u8R"({"name":")");
				writeJsonString(artist, str);
				str.append(u8"\"},");
			}
			str.back() = u8"]"[0]; // removing the last redundant comma as well.
		}
		str.append(u8R"(},)");
	}
	str.append(u8R"("length":{"amount":)");
	writeJsonLong(track.m_duration, str);
	str.append(u8R"(,"unit":"ms"}})");
	return str;
}
