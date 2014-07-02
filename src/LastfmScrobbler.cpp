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
#include "HttpClient.hpp"
#include "UrlBuilder.hpp"
#include <afc/StringRef.hpp>
#include "logger.hpp"
#include "dateutil.hpp"
#include <afc/md5.hpp>
#include <afc/string_util.hpp>
#include <afc/ensure_ascii.hpp>
#include <utility>
#include <afc/utils.h>

using namespace std;
using namespace afc;
using StatusCode = HttpClient::StatusCode;

namespace
{
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
		logError("Scrobbler is not configured properly.");
		return 0;
	}

	if (m_scrobblerUrl.empty()) {
		/* There is no sense to try to send a request because the URL to the scrobbling
		 * service (e.g. Gravifon API) is undefined. The scrobble is added to the list of
		 * pending scrobbles (see above) to be submitted (among other scrobbles) when
		 * the URL is configured to point to a scrobbling server.
		 */
		logError("URL to the scrobbling server is undefined.");
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
	UrlBuilder url(m_scrobblerUrl);
	url.param("hs", "true"_s).
			param("p", "1.2.1"_s).
			param("c", "tst"_s).
			param("v", "1.0"_s).
			paramName("u").paramValue(m_username);

	const string timestamp(to_string(currentUTCTimeSeconds()));

	// Calculating the authentication token.
	const size_t md5LengthOctets = 32; // Each octet is represented as two characters.
	string authToken, tmp;
	authToken.reserve(md5LengthOctets);
	tmp.reserve(md5LengthOctets + timestamp.size());
	// TODO check that this reinterpret_cast conforms to the C++11 standard.
	md5String(reinterpret_cast<const unsigned char *>(m_password.c_str()), m_password.size(), tmp);
	tmp += timestamp;
	md5String(reinterpret_cast<const unsigned char *>(tmp.c_str()), tmp.size(), authToken);

	url.paramName("t").paramValue(timestamp);
	url.paramName("a").paramValue(authToken);

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
		logDebug(string("[LastfmScrobbler] Authentication URL: ") + url.toString());

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().get(url.toString(), HttpEntity(), response,
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

	logDebug(string("[LastfmScrobbler] Authentication response status code: ") + to_string(response.statusCode));
	logDebug(string("[LastfmScrobbler] Authentication response body: ") + response.body);

	if (response.statusCode != 200) {
		logError("[LastfmScrobbler] An error is encountered while authenticating the user to Last.fm.");
		return false;
	}

	/* Adding tags one by one. DeaDBeeF returns them as
	 * '\n'-separated values within a single string.
	 */
	char *start, *end;
	Tokeniser<char> t(response.body, '\n');
	t.next(start, end);
	if (end - start == 2 && start[0] == 'O' && start[1] == 'K') {
		if (!t.hasNext()) { // Session ID.
			logError(string("[LastfmScrobbler] Invalid response body: ") + response.body);
			return false;
		}
		m_sessionId = std::move(t.next());

		if (!t.hasNext()) { // Now-playing URL. It is ignored for now.
			logError(string("[LastfmScrobbler] Invalid response body: ") + response.body);
			return false;
		}
		if (!t.hasNext()) { // Submission URL.
			logError(string("[LastfmScrobbler] Invalid response body: ") + response.body);
			return false;
		}
		m_submissionUrl = std::move(t.next());

		logDebug("[LastfmScrobbler] The user is authenticated...");
		m_authenticated = true;
		return true;
	} else {
		// TODO handle non-OK responses differently (e.g. if BANNED then disable the plugin).
		logError(string("[LastfmScrobbler] Unable to authenticate the user to Last.fm. Reason code: ") +
				string(start, end - start));
		return false;
	}
}
