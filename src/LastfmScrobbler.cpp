/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2014 Dźmitry Laŭčuk

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

#include "LastfmScrobbler.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include "HttpClient.hpp"
#include "UrlBuilder.hpp"
#include <afc/number.h>
#include <afc/StringRef.hpp>
#include "logger.hpp"
#include <afc/md5.hpp>
#include <afc/Tokeniser.hpp>
#include <afc/ensure_ascii.hpp>
#include <utility>
#include <afc/dateutil.hpp>
#include <afc/utils.h>

using namespace std;
using namespace afc;
using StatusCode = HttpClient::StatusCode;

namespace
{
	inline UrlBuilder buildAuthUrl(const string &scrobblerUrl, const string &username, const string &password)
	{
		typedef decltype(currentUTCTimeSeconds()) TimestampType;
		constexpr size_t maxTimestampSize = afc::maxPrintedSize<TimestampType, 10>();

		/* Auth token is generated as md5(md5(password) + timestamp),
		 * where md5 is a lowercase hex-encoded ASCII MD5 hash and + is concatenation.
		 */
		constexpr size_t digestSize = 32; // Each octet is represented as two characters.
		char tmp[digestSize + maxTimestampSize];
		// md5(password). In addition, the timestamp is stored right after the end of this digest.
		// TODO check that this reinterpret_cast conforms to the C++11 standard.
		char * const timestampStart = md5String(reinterpret_cast<const unsigned char *>(password.c_str()), password.size(), tmp);
		// md5(password) + timestamp
		char * const timestampEnd = printNumber<TimestampType, 10>(currentUTCTimeSeconds(), timestampStart);
		// Finally, generating authToken.
		char authToken[digestSize];
		md5String(reinterpret_cast<const unsigned char *>(tmp), timestampEnd - tmp, authToken);

		// TODO set real client ID and version.
		return UrlBuilder(scrobblerUrl,
				UrlPart<raw>("hs"_s), UrlPart<raw>("true"_s),
				UrlPart<raw>("p"_s), UrlPart<raw>("1.2.1"_s),
				UrlPart<raw>("c"_s), UrlPart<raw>("tst"_s),
				UrlPart<raw>("v"_s), UrlPart<raw>("1.0"_s),
				UrlPart<raw>("u"_s), UrlPart<>(username),
				/* Neither timestamp nor authToken need to be URL-encoded since they are
				 * decimal and hex numbers, respectively.
				 */
				UrlPart<raw>("t"_s), UrlPart<raw>(timestampStart, timestampEnd - timestampStart),
				UrlPart<raw>("a"_s), UrlPart<raw>(authToken, digestSize));
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
			message = "sending the request has timed out";
			break;
		default:
			message = "unknown error";
		}
		std::fprintf(stderr, "[LastfmScrobbler] Unable to send the request: %s\n", message);
	}
}

void LastfmScrobbler::stopExtra()
{
	m_scrobblerUrl.clear();
}

void LastfmScrobbler::configure(const char * const serverUrl, const string &username, const string &password)
{ lock_guard<mutex> lock(m_mutex);
	assert(serverUrl != nullptr);

	bool reconfigured = false;

	if (m_scrobblerUrl != serverUrl) {
		m_scrobblerUrl = serverUrl;
		reconfigured = true;
	}
	if (m_username != username) {
		m_username = username;
		reconfigured = true;
	}
	if (m_password != password) {
		m_password = password;
		reconfigured = true;
	}

	if (reconfigured) {
		// The configuration has changed. Updating it as well as resetting the 'scrobbles to wait' counter.
		m_scrobblesToWait = MIN_SCROBBLES_TO_WAIT;
		m_authenticated = false;
	}

	m_configured = true;
}

std::size_t LastfmScrobbler::doScrobbling()
{
	assertLocked();
	assert(!m_pendingScrobbles.empty());

	if (!m_configured) {
		fputs("Scrobbler is not configured properly.", stderr);
		return 0;
	}

	if (m_scrobblerUrl.empty()) {
		/* There is no sense to try to send a request because the URL to the scrobbling
		 * service (e.g. Gravifon API) is undefined. The scrobble is added to the list of
		 * pending scrobbles (see above) to be submitted (among other scrobbles) when
		 * the URL is configured to point to a scrobbling server.
		 */
		fputs("URL to the scrobbling server is undefined.", stderr);
		return 0;
	}

	if (ensureAuthenticated()) {
		return 0;
	}

	// TODO implement scrobbling.
	return 0;
}

inline bool LastfmScrobbler::ensureAuthenticated()
{
	assertLocked();

	if (m_authenticated) {
		return true;
	}

	logDebug("[LastfmScrobbler] Authenticating the user...");

	// TODO set real client ID and version.
	const UrlBuilder url = buildAuthUrl(m_scrobblerUrl, m_username, m_password);

	StatusCode result;
	HttpResponseEntity response;

	/* Each HTTP call is performed outside the critical section so that other threads can:
	 * - add scrobbles without waiting for this call to finish
	 * - stop this Scrobbler by invoking Scrobbler::stop(). In this case this HTTP call
	 * is aborted and the scrobbles involved are left in the list of pending scrobbles
	 * so that they can be stored to the data file and be completed later.
	 *
	 * It is safe to unlock the mutex because no shared data is accessed outside the critical section.
	 *
	 * In addition, it is safe to use m_finishScrobblingFlag outside the critical section
	 * because it is atomic.
	 */
	{ UnlockGuard unlockGuard(m_mutex);
		logDebug(string("[LastfmScrobbler] Authentication URL: ") + string(url.c_str(), url.size()));

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().get(url.c_str(), HttpEntity(), response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebug("[LastfmScrobbler] An HTTP call is aborted.");
		return false;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return false;
	}

	string &responseBody = response.body;

	logDebug(string("[LastfmScrobbler] Authentication response status code: ") + to_string(response.statusCode));
	logDebug(string("[LastfmScrobbler] Authentication response body: ") + responseBody);

	if (response.statusCode != 200) {
		fputs("[LastfmScrobbler] An error is encountered while authenticating the user to Last.fm.", stderr);
		return false;
	}

	/* Adding tags one by one. DeaDBeeF returns them as
	 * '\n'-separated values within a single string.
	 */
	string::iterator start, end;
	Tokeniser<> t(responseBody, '\n');
	t.next(start, end);
	if (end - start == 2 && *start == 'O' && *(start + 1) == 'K') {
		if (!t.hasNext()) { // Session ID.
			fprintf(stderr, "[LastfmScrobbler] Invalid response body: '%s'.\n", responseBody.c_str());
			return false;
		}
		m_sessionId = std::move(t.next());

		if (!t.hasNext()) { // Now-playing URL. It is ignored for now.
			fprintf(stderr, "[LastfmScrobbler] Invalid response body: '%s'.\n", responseBody.c_str());
			return false;
		}
		if (!t.hasNext()) { // Submission URL.
			fprintf(stderr, "[LastfmScrobbler] Invalid response body: '%s'.\n", responseBody.c_str());
			return false;
		}
		m_submissionUrl = std::move(t.next());

		logDebug("[LastfmScrobbler] The user is authenticated...");
		m_authenticated = true;
		return true;
	} else {
		// TODO handle non-OK responses differently (e.g. if BANNED then disable the plugin).
		fprintf(stderr, "[LastfmScrobbler] Unable to authenticate the user to Last.fm. Reason code: '%s'.\n",
				string(start, end).c_str());
		return false;
	}
}
