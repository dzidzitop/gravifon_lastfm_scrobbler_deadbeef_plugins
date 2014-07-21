/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

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
#include "GravifonScrobbler.hpp"
#include <cassert>
#include <afc/base64.hpp>
#include "HttpClient.hpp"
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include "logger.hpp"
#include <utility>
#include "jsonutil.hpp"
#include "pathutil.hpp"
#include <afc/FastStringBuffer.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/utils.h>

using namespace std;
using namespace afc;
using Json::Value;
using StatusCode = HttpClient::StatusCode;

namespace
{
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
		std::fprintf(stderr, "[GravifonScrobbler] Unable to send the scrobble message: %s\n", message);
	}

	inline bool isRecoverableError(const unsigned long errorCode)
	{
		return errorCode < 10000 || errorCode == 10003 || errorCode > 10006;
	}
}

void GravifonScrobbler::stopExtra()
{
	m_scrobblerUrl.clear();
	m_authHeader.clear();
}

void GravifonScrobbler::configure(const char * const serverUrl, const std::size_t serverUrlSize,
		const char * const username, const std::size_t usernameSize,
		const char * const password, const std::size_t passwordSize)
{ lock_guard<mutex> lock(m_mutex);
	string tmpUrl(serverUrl, serverUrlSize);
	if (serverUrlSize > 0) {
		appendToPath(tmpUrl, "scrobbles");
	}

	// Curl expects the basic charset in headers.
	string tmpAuthHeader("Authorization: Basic "); // HTTP Basic authentication is used.

	/* Colon (':') is not allowed to be in a username by Gravifon. This concatenation is safe.
	 * In addition, the character 'colon' in UTF-8 is equivalent to those in ISO-8859-1.
	 */
	// TODO replace with FastStringBuffer.
	string token;
	token.reserve(usernameSize + passwordSize + 1);
	token.append(username, usernameSize);
	token += ':';
	token.append(password, passwordSize);

	tmpAuthHeader += encodeBase64(token);

	const bool sameScrobblerUrl = m_scrobblerUrl == tmpUrl;
	const bool sameAuthHeader = m_authHeader == tmpAuthHeader;

	if (!sameScrobblerUrl || !sameAuthHeader) {
		// The configuration has changed. Updating it as well as resetting the 'scrobbles to wait' counter.
		if (!sameScrobblerUrl) {
			m_scrobblerUrl = std::move(tmpUrl);
		}
		if (!sameAuthHeader) {
			m_authHeader = std::move(tmpAuthHeader);
		}
		m_scrobblesToWait = minScrobblesToWait();
	}

	m_configured = true;
}

size_t GravifonScrobbler::doScrobbling()
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

	HttpEntity request;
	string &body = request.body;
	body += u8"[";

	// Adding up to 20 scrobbles to the request.
	// 20 is the max number of scrobbles in a single request.
	unsigned submittedCount = 0;
	for (auto it = m_pendingScrobbles.begin(), end = m_pendingScrobbles.end();
			submittedCount < 20 && it != end; ++submittedCount, ++it) {
		it->appendAsJsonTo(body);
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

	// Making a copy of shared data to pass outside the critical section.
	const string scrobblerUrlCopy = m_scrobblerUrl;

#ifndef NDEBUG
	const size_t pendingScrobbleCount = m_pendingScrobbles.size();
#endif

	StatusCode result;
	HttpResponseEntity response;

	/* Each HTTP call is performed outside the critical section so that other threads can:
	 * - add scrobbles without waiting for this call to finish
	 * - stop this Scrobbler by invoking Scrobbler::stop(). In this case this HTTP call
	 * is aborted and the scrobbles involved are left in the list of pending scrobbles
	 * so that they can be stored to the data file and be completed later.
	 *
	 * It is safe to unlock the mutex because:
	 * - no shared data is accessed outside the critical section
	 * - other threads cannot delete scrobbles in the meantime (because the scrobbling thread
	 * assumes that the scrobbles being submitted are the first {submittedCount} elements in
	 * the list of pending scrobbles.
	 *
	 * In addition, it is safe to use m_finishScrobblingFlag outside the critical section
	 * because it is atomic.
	 */
	{ UnlockGuard unlockGuard(m_mutex);
		logDebug(string("[GravifonScrobbler] Request body: ") + request.body);

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().post(scrobblerUrlCopy.c_str(), request, response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	/* Ensure that no scrobbles are deleted by other threads during the HTTP call.
	 * Only the scrobbling thread and ::stop() can do this, and ::stop() must wait for
	 * the scrobbling thread to finish in order to do this.
	 */
	assert(pendingScrobbleCount <= m_pendingScrobbles.size());

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebug("[GravifonScrobbler] An HTTP call is aborted.");
		return 0;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return 0;
	}

	logDebug(string("[GravifonScrobbler] Response status code: ") + to_string(response.statusCode));

	const string &responseBody = response.body;

	Value rs;
	if (!Json::Reader().parse(responseBody, rs, false)) {
		fprintf(stderr, "[GravifonScrobbler] Invalid response: '%s'.\n", responseBody.c_str());
		return 0;
	}

	if (response.statusCode == 200) {
		// An array of status entities is expected for a 200 response, one per scrobble submitted.
		if (!isType(rs, arrayValue) || rs.size() != submittedCount) {
			fprintf(stderr, "[GravifonScrobbler] Invalid response: '%s'.\n", response.body.c_str());
			return 0;
		}

		size_t completedCount = 0;
		auto it = m_pendingScrobbles.begin();
		for (auto i = 0u, n = rs.size(); i < n; ++i) {
			processStatus(rs[i],
					// Successful status: if the track is scrobbled successfully then it is removed from the list.
					[&it, &completedCount, this]()
					{
						it = m_pendingScrobbles.erase(it);
						++completedCount;
					},

					/* Error status. If the error is unprocessable then the scrobble is removed from the list;
					 * otherwise another attempt will be done to submit it.
					 */
					[&responseBody, &it, &completedCount, this](
							const unsigned long errorCode, const string &errorDescription)
					{
						string scrobbleAsStr;
						it->appendAsJsonTo(scrobbleAsStr);
						if (isRecoverableError(errorCode)) {
							fprintf(stderr, "[GravifonScrobbler] Scrobble '%s' is not processed. "
									"Error: '%s' (%lu). It will be re-submitted later.\n",
									scrobbleAsStr.c_str(), errorDescription.c_str(), errorCode);
							++it;
						} else {
							fprintf(stderr, "[GravifonScrobbler] Scrobble '%s' cannot be processed. "
									"Error: '%s' (%lu). It is removed as non-processable.\n",
									scrobbleAsStr.c_str(), errorDescription.c_str(), errorCode);
							it = m_pendingScrobbles.erase(it);
							++completedCount;
						}
					},

					// Invalid status: report an error and leave the scrobble in the list of pending scrobbles.
					[&responseBody, &it]()
					{
						fprintf(stderr, "[GravifonScrobbler] Invalid response: '%s'.\n", responseBody.c_str());
						++it;
					});
		}

		if (completedCount == submittedCount) {
			logDebug(string("[GravifonScrobbler] Successful response: ") + responseBody);
		}

		return completedCount;
	} else {
		// A global status entity is expected for a non-200 response.
		processStatus(rs,
				// Success status. It is not expected. Report an error and finish processing.
				[&responseBody]()
				{
					fprintf(stderr, "[GravifonScrobbler] Unexpected 'ok' global status response: '%s'.\n",
							responseBody.c_str());
				},

				// Error status: report an error and finish processing.
				[&responseBody](const unsigned long errorCode, const string &errorDescription)
				{
					fprintf(stderr, "[GravifonScrobbler] Error global status response: '%s'. Error: '%s' (%lu).\n",
							responseBody.c_str(), errorDescription.c_str(), errorCode);
				},

				// Invalid status: report an error and finish processing.
				[&responseBody]()
				{
					fprintf(stderr, "[GravifonScrobbler] Invalid response: '%s'.\n", responseBody.c_str());
				});

		return 0;
	}
}
