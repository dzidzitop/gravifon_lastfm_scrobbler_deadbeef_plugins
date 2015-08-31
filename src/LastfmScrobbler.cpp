/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2014-2015 Dźmitry Laŭčuk

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

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <utility>

#include "HttpClient.hpp"

#include <afc/builtin.hpp>
#include <afc/dateutil.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/logger.hpp>
#include <afc/number.h>
#include <afc/md5.hpp>
#include <afc/SimpleString.hpp>
#include <afc/StringRef.hpp>
#include <afc/UrlBuilder.hpp>
#include <afc/utils.h>

#include "deadbeef_util.hpp"

using namespace std;
using namespace afc;
using StatusCode = HttpClient::StatusCode;

using afc::logger::logDebugFmt;
using afc::logger::logDebugMsg;
using afc::logger::logErrorFmt;
using afc::logger::logErrorMsg;
using namespace afc::url;

namespace
{
	inline UrlBuilder<webForm> buildAuthUrl(const afc::String &scrobblerUrl, const afc::String &username,
			const afc::String &password)
	{
		constexpr size_t maxTimestampSize = afc::maxPrintedSize<long, 10>();

		/* Auth token is generated as md5(md5(password) + timestamp),
		 * where md5 is a lowercase hex-encoded ASCII MD5 hash and + is concatenation.
		 */
		constexpr size_t digestSize = 32; // Each octet is represented as two characters.
		char tmp[digestSize + maxTimestampSize];
		// md5(password). In addition, the timestamp is stored right after the end of this digest.
		// TODO check that this reinterpret_cast conforms to the C++11 standard.
		char * const timestampStart = md5String(reinterpret_cast<const unsigned char *>(password.data()), password.size(), tmp);
		// md5(password) + timestamp
		char * const timestampEnd = printNumber<10>(now().millis() / 1000, timestampStart);
		// Finally, generating authToken.
		char authToken[digestSize];
		md5String(reinterpret_cast<const unsigned char *>(tmp), timestampEnd - tmp, authToken);

		// TODO set real client ID and version.
		return UrlBuilder<webForm>(scrobblerUrl.data(), scrobblerUrl.size(),
				UrlPart<raw>("hs"_s), UrlPart<raw>("true"_s),
				UrlPart<raw>("p"_s), UrlPart<raw>("1.2.1"_s),
				UrlPart<raw>("c"_s), UrlPart<raw>("tst"_s),
				UrlPart<raw>("v"_s), UrlPart<raw>("1.0"_s),
				UrlPart<raw>("u"_s), UrlPart<>(username.data(), username.size()),
				/* Neither timestamp nor authToken need to be URL-encoded since they are
				 * decimal and hex numbers, respectively.
				 */
				UrlPart<raw>("t"_s), UrlPart<raw>(timestampStart, timestampEnd - timestampStart),
				UrlPart<raw>("a"_s), UrlPart<raw>(authToken, digestSize));
	}

	inline UrlPart<> getAlbumTitleUrlPart(const Track &track) noexcept
	{
		const char *albumTitleData;
		std::size_t albumTitleSize;
		if (track.hasAlbumTitle()) {
			const afc::String &str = track.getAlbumTitle();
			albumTitleData = str.data();
			albumTitleSize = str.size();
		} else {
			albumTitleData = nullptr;
			albumTitleSize = 0;
		}
		return UrlPart<>(albumTitleData, albumTitleSize);
	}

	class ScrobbleParamName
	{
		// The same instance must be used.
		ScrobbleParamName(const ScrobbleParamName &) = delete;
		ScrobbleParamName(ScrobbleParamName &&) = delete;
	public:
		explicit ScrobbleParamName(const unsigned char index) : m_step(0)
		{
			assert(index < 100); // one or two digits are expected and supported.

			if (index < 10) {
				m_value[0] = digitToChar<10>(index);
				m_longIndex = false;
			} else {
				afc::printTwoDigits(index, m_value);
				m_longIndex = true;
			}
		}

		// a[i] encoded can be a%5bxx%5d in the worst case.
		std::size_t maxEncodedSize() const noexcept { return 9; }

		template<typename Iterator>
		Iterator appendTo(Iterator dest)
		{
			assert(m_step < sizeof(namePrefixes));

			*dest++ = namePrefixes[m_step++];
			// Writing URL-encoded '['.
			dest = afc::copy("%5b"_s, dest);
			*dest++ = m_value[0];
			if (m_longIndex) {
				*dest++ = m_value[1];
			}
			// Writing URL-encoded ']'.
			dest = afc::copy("%5d"_s, dest);
			return dest;
		}
	private:
		static const char namePrefixes[9];

		std::size_t m_step;
		char m_value[2];
		bool m_longIndex;
	};

	// Prefixes are put in the order in which the parameters of a submission URL are processed.
	const char ScrobbleParamName::namePrefixes[9] = {'a', 't', 'i', 'o', 'r', 'l', 'b', 'n', 'm'};

	template<typename Number>
	class NumberUrlPart
	{
		// The same instance should be used.
		NumberUrlPart(const NumberUrlPart &) = delete;
		NumberUrlPart(NumberUrlPart &&) = delete;
	public:
		explicit constexpr NumberUrlPart(const Number number) : m_number(number) {}

		constexpr std::size_t maxEncodedSize() const noexcept { return afc::maxPrintedSize<Number, 10>(); }

		template<typename Iterator>
		Iterator appendTo(Iterator dest) const { return afc::printNumber<10>(m_number, dest); }
	private:
		const Number m_number;
	};

	void appendScrobbleInfo(UrlBuilder<webForm> &builder, const ScrobbleInfo &scrobbleInfo, const unsigned char index)
	{
		assert(index < 50); // max amount of scrobbles per request.

		const Track &track = scrobbleInfo.track;
		assert(track.hasTitle());
		assert(track.hasArtist());

		const NumberUrlPart<afc::Timestamp::time_type> scrobbleStartTs(
				scrobbleInfo.scrobbleStartTimestamp.millis() / 1000);
		const NumberUrlPart<long> trackDurationSeconds(track.getDurationMillis() / 1000);

		// Constructs params in form x[index] where x is changed each time ::appendTo() is invoked.
		ScrobbleParamName scrobbleParamName(index);

		// TODO optimise parameter passing
		builder.params(
				// The artist name. Required.
				scrobbleParamName, UrlPart<>(track.getArtists()[0].data(), track.getArtists()[0].size()),
				// The track title. Required.
				scrobbleParamName, UrlPart<>(track.getTitle().data(), track.getTitle().size()),
				// The time the track started playing, in UNIX timestamp format. Required.
				scrobbleParamName, scrobbleStartTs,
				// The source of the track. Required. 'Chosen by the user' in all cases.
				scrobbleParamName, UrlPart<raw>("P"_s),
				// TODO Support track ratings.
				// A single character denoting the rating of the track. Empty, since not applicable.
				scrobbleParamName, UrlPart<raw>(""_s),
				// The length of the track in seconds. Required for 'Chosen by the user'.
				scrobbleParamName, trackDurationSeconds,
				// The album title, or an empty string if not known.
				scrobbleParamName, getAlbumTitleUrlPart(track),
				// TODO Support track numbers.
				// The position of the track on the album, or an empty string if not known.
				scrobbleParamName, UrlPart<>(""_s),
				// TODO Support MusicBrainz Track IDs.
				// The MusicBrainz Track ID, or an empty string if not known.
				scrobbleParamName, UrlPart<>(""_s));
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
		logErrorFmt("[LastfmScrobbler] Unable to send the request: '#'.", message);
	}

	static constexpr auto lastfmResponseDelim = [](const char c) noexcept { return c == '\n'; };
}

void LastfmScrobbler::submitNowPlayingTrack()
{
	assertLocked();

	logDebugFmt("[LastfmScrobbler] Trying to submit the now-playing notification to the scrobbling server.");

	if (unlikely(!m_configured)) {
		logErrorMsg("[LastfmScrobbler] Scrobbler is not configured properly."_s);
		return;
	}

	if (!m_hasNowPlayingTrack) {
		logDebugFmt("[LastfmScrobbler] There is no now-playing track to submit.");
		return;
	}

	// Resetting the event despite of the result of the attempt to submit it to the scrobbling server.
	m_hasNowPlayingTrack = false;

	ensureAuthenticated();

	const Track &track = m_nowPlayingTrack;

	// TODO optimise parameter passing
	UrlBuilder<webForm> builder(queryOnly,
			// TODO URL-encode session ID right after it is obtained during the authentication process.
			UrlPart<raw>("s"_s), UrlPart<>(m_sessionId.data(), m_sessionId.size()),
			UrlPart<raw>("a"_s), UrlPart<>(track.getArtists()[0].data(), track.getArtists()[0].size()),
			UrlPart<raw>("t"_s), UrlPart<>(track.getTitle().data(), track.getTitle().size()),
			UrlPart<raw>("b"_s), getAlbumTitleUrlPart(track),
			UrlPart<raw>("l"_s), NumberUrlPart<long>(track.getDurationMillis() / 1000),
			// TODO Support track numbers.
			// The position of the track on the album, or an empty string if not known.
			UrlPart<raw>("n"_s), UrlPart<raw>(""_s),
			// TODO Support MusicBrainz Track IDs.
			// The MusicBrainz Track ID, or an empty string if not known.
			UrlPart<raw>("m"_s), UrlPart<>(""_s));

	HttpRequest request;
	request.setBody(builder.data(), builder.size());

	// Making a copy of shared data to pass outside the critical section.
	const afc::String nowPlayingUrlCopy(m_nowPlayingUrl);

	/* No conversion to the system encoding is used as the response body is assumed to be in
	 * an ASCII-compatible encoding. It contains status codes (in ASCII), and some reason
	 * messages that are safe to be used without conversion with hope they are in ASCII, too.
	 */
	afc::FastStringBuffer<char> responseBody;
	FastStringBufferAppender responseBodyAppender(responseBody);
	StatusCode result;
	HttpResponse response(responseBodyAppender);

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
		logDebugFmt("[LastfmScrobbler] Now-playing URL: '#'.", nowPlayingUrlCopy);
		logDebugFmt("[LastfmScrobbler] Now-playing request body: '#'.",
				std::make_pair(request.getBody(), request.getBody() + request.getBodySize()));

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().post(nowPlayingUrlCopy.c_str(), request, response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebugMsg("[LastfmScrobbler] An HTTP call is aborted."_s);
		return;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return;
	}

	logDebugFmt("[LastfmScrobbler] Now-playing response status code: '#'.", response.statusCode);
	logDebugFmt("[LastfmScrobbler] Now-playing response body: #", responseBody.c_str());

	if (response.statusCode == 200) {
		/* Using find_if to tokenise response instead of find since the former
		 * takes parameters by value which minimises memory reads.
		 */
		const char *seqBegin = responseBody.data(), *seqEnd, *end = responseBody.data() + responseBody.size();
		seqEnd = std::find_if(seqBegin, end, lastfmResponseDelim);
		if (unlikely(seqEnd == end)) {
			logErrorFmt("[LastfmScrobbler] Invalid response body (missing line feed): '#'.", responseBody.c_str());
			return;
		}
		const std::size_t tokenSize = seqEnd - seqBegin;
		if (tokenSize == 2 && *seqBegin == 'O' && *(seqBegin + 1) == 'K') {
			logDebugMsg("[LastfmScrobbler] The now-playing track is submitted successfully."_s);
			return;
		} else {
			constexpr ConstStringRef badSession = "BADSESSION"_s;
			if (tokenSize == badSession.size() && equal(badSession.begin(), badSession.end(), seqBegin)) {
				logDebugMsg("[LastfmScrobbler] The now-playing track is not submitted. "
						"The user is not authenticated to Last.fm."_s);

				deauthenticate();
				return;
			} else {
				// TODO think of counting hard failures, as the specification suggests.
				// A hard failure or an unknown status is reported.
				logErrorFmt("[LastfmScrobbler] Unable to submit the now-playing track to Last.fm. Reason: '#'.",
						std::make_pair(seqBegin, seqEnd));
				return;
			}
		}
	} else {
		logErrorMsg("[LastfmScrobbler] An error is encountered while submitting the now-playing track to Last.fm."_s);
		return;
	}
}

void LastfmScrobbler::stopExtra()
{
	m_sessionId.clear();
	m_scrobblerUrl.clear();
	m_nowPlayingUrl.clear();
	m_hasNowPlayingTrack = false;
}

void LastfmScrobbler::configure(const char * const serverUrl, const std::size_t serverUrlSize,
		const char * const username, const char * const password)
{ lock_guard<mutex> lock(m_mutex);
	assert(username != nullptr);
	assert(password != nullptr);

	logDebugMsg("[LastfmScrobbler] Configuring..."_s);

	bool reconfigured = false;

	if (!afc::equal(serverUrl, serverUrlSize, m_scrobblerUrl.begin(), m_scrobblerUrl.size())) {
		m_scrobblerUrl.assign(serverUrl, serverUrlSize);
		reconfigured = true;
	}

	const std::size_t usernameSize = std::strlen(username);
	if (!afc::equal(username, usernameSize, m_username.begin(), m_username.size())) {
		m_username.assign(username, usernameSize);
		reconfigured = true;
	}

	const std::size_t passwordSize = std::strlen(password);
	if (!afc::equal(password, passwordSize, m_password.begin(), m_password.size())) {
		m_password.assign(password, passwordSize);
		reconfigured = true;
	}

	if (reconfigured) {
		// The configuration has changed. Updating it as well as resetting the 'scrobbles to wait' counter.
		m_scrobblesToWait = minScrobblesToWait();
		m_authenticated = false;
	}

	m_configured = true;

	logDebugMsg("[LastfmScrobbler] Configuring completed."_s);
}

std::size_t LastfmScrobbler::doScrobbling()
{
	assertLocked();
	assert(!m_pendingScrobbles.empty());

	if (unlikely(!m_configured)) {
		logErrorMsg("Scrobbler is not configured properly."_s);
		return 0;
	}

	if (m_scrobblerUrl.empty()) {
		/* There is no sense to try to send a request because the URL to the scrobbling
		 * service is undefined. The scrobble is added to the list of pending scrobbles
		 * (see above) to be submitted (among other scrobbles) when the URL is configured
		 * to point to a scrobbling server.
		 */
		logErrorMsg("URL to the scrobbling server is undefined."_s);
		return 0;
	}

	if (!ensureAuthenticated()) {
		return 0;
	}

	// 50 is the max number of scrobbles that can be submitted within a single request.
	constexpr unsigned maxScrobblesPerRequest = 50;

	// Adding up to maxScrobblesPerRequest scrobbles to the request.
	UrlBuilder<webForm> builder(queryOnly,
			// TODO URL-encode session ID right after it is obtained during the authentication process.
			UrlPart<raw>("s"_s), UrlPart<>(m_sessionId.data(), m_sessionId.size()));

	/* Points to the position after the end of the chunk of scrobbles submitted and is used
	 * to remove these scrobbles from the queue if they are submitted successfully.
	 */
	auto chunkEnd = m_pendingScrobbles.begin();
	unsigned submittedCount = 0;
	for (auto end = m_pendingScrobbles.end(); chunkEnd != end && submittedCount < maxScrobblesPerRequest;
			++chunkEnd, ++submittedCount) {
		appendScrobbleInfo(builder, *chunkEnd, submittedCount);
	}

	HttpRequest request;
	request.setBody(builder.data(), builder.size());

	// Making a copy of shared data to pass outside the critical section.
	const afc::String submissionUrlCopy(m_submissionUrl);

#ifndef NDEBUG
	const size_t pendingScrobbleCount = m_pendingScrobbles.size();
#endif

	/* No conversion to the system encoding is used as the response body is assumed to be in
	 * an ASCII-compatible encoding. It contains status codes (in ASCII), and some reason
	 * messages that are safe to be used without conversion with hope they are in ASCII, too.
	 */
	afc::FastStringBuffer<char> responseBody;
	FastStringBufferAppender responseBodyAppender(responseBody);
	StatusCode result;
	HttpResponse response(responseBodyAppender);

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
		logDebugFmt("[LastfmScrobbler] Submission URL: '#'.", submissionUrlCopy);
		logDebugFmt("[LastfmScrobbler] Submission request body: '#'.",
				std::make_pair(request.getBody(), request.getBody() + request.getBodySize()));

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().post(submissionUrlCopy.c_str(), request, response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	/* Ensure that no scrobbles are deleted by other threads during the HTTP call.
	 * Only the scrobbling thread and ::stop() can do this, and ::stop() must wait for
	 * the scrobbling thread to finish in order to do this.
	 */
	assert(pendingScrobbleCount <= m_pendingScrobbles.size());

	if (result == StatusCode::ABORTED_BY_CLIENT) {
		logDebugMsg("[LastfmScrobbler] An HTTP call is aborted."_s);
		return 0;
	}
	if (result != StatusCode::SUCCESS) {
		reportHttpClientError(result);
		return 0;
	}

	logDebugFmt("[LastfmScrobbler] Submission response status code: '#'.", response.statusCode);
	logDebugFmt("[LastfmScrobbler] Submission response body: #", responseBody.c_str());

	if (response.statusCode == 200) {
		/* Using find_if to tokenise response instead of find since the former
		 * takes parameters by value which minimises memory reads.
		 */
		const char *seqBegin = responseBody.data(), *seqEnd, *end = responseBody.data() + responseBody.size();
		seqEnd = std::find_if(seqBegin, end, lastfmResponseDelim);
		if (unlikely(seqEnd == end)) {
			logErrorFmt("[LastfmScrobbler] Invalid response body (missing line feed): '#'.", responseBody.c_str());
			return 0;
		}
		const std::size_t tokenSize = seqEnd - seqBegin;
		if (tokenSize == 2 && *seqBegin == 'O' && *(seqBegin + 1) == 'K') {
			logDebugMsg("[LastfmScrobbler] The scrobbles are submitted successfully."_s);

			m_pendingScrobbles.erase(m_pendingScrobbles.begin(), chunkEnd);
			return submittedCount;
		} else {
			constexpr ConstStringRef badSession = "BADSESSION"_s;
			if (tokenSize == badSession.size() && equal(badSession.begin(), badSession.end(), seqBegin)) {
				logDebugMsg("[LastfmScrobbler] The scrobbles are not submitted. "
						"The user is not authenticated to Last.fm."_s);

				deauthenticate();
				return 0;
			} else {
				// TODO think of counting hard failures, as the specification suggests.
				// A hard failure or an unknown status is reported.
				logErrorFmt("[LastfmScrobbler] Unable to submit scrobbles to Last.fm. Reason: '#'.",
						std::make_pair(seqBegin, seqEnd));
				return 0;
			}
		}
	} else {
		logErrorMsg("[LastfmScrobbler] An error is encountered while submitting the scrobbles to Last.fm."_s);
		return 0;
	}

	return 0;
}

inline bool LastfmScrobbler::ensureAuthenticated()
{
	assertLocked();

	if (m_authenticated) {
		return true;
	}

	logDebugMsg("[LastfmScrobbler] Authenticating the user..."_s);

	// TODO set real client ID and version.
	const UrlBuilder<webForm> url = buildAuthUrl(m_scrobblerUrl, m_username, m_password);

	/* No conversion to the system encoding is used as the response body is assumed to be in
	 * an ASCII-compatible encoding. It contains status codes, URLs (both are in ASCII),
	 * and some reason messages that are safe to be used without conversion with hope
	 * they are in ASCII, too.
	 */
	afc::FastStringBuffer<char> responseBody;
	FastStringBufferAppender responseBodyAppender(responseBody);
	StatusCode result;
	HttpResponse response(responseBodyAppender);

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
		logDebugFmt("[LastfmScrobbler] Authentication URL: '#'.", url.c_str());

		// The timeouts are set to 'infinity' since this HTTP call is interruptible.
		result = HttpClient().get(url.c_str(), HttpRequest(), response,
				HttpClient::NO_TIMEOUT, HttpClient::NO_TIMEOUT, m_finishScrobblingFlag);
	}

	if (unlikely(result == StatusCode::ABORTED_BY_CLIENT)) {
		logDebugMsg("[LastfmScrobbler] An HTTP call is aborted."_s);
		return false;
	}
	if (unlikely(result != StatusCode::SUCCESS)) {
		reportHttpClientError(result);
		return false;
	}

	logDebugFmt("[LastfmScrobbler] Authentication response status code: '#'.", response.statusCode);
	logDebugFmt("[LastfmScrobbler] Authentication response body: #", responseBody.c_str());

	if (unlikely(response.statusCode != 200)) {
		logErrorMsg("[LastfmScrobbler] An error is encountered while authenticating the user to Last.fm."_s);
		return false;
	}

	/* Using find_if to tokenise response instead of find since the former
	 * takes parameters by value which minimises memory reads.
	 */
	const char *seqBegin = responseBody.data(), *seqEnd, *end = responseBody.data() + responseBody.size();

	// Status.
	seqEnd = std::find_if(seqBegin, end, lastfmResponseDelim);

	if (unlikely(seqEnd == end)) {
		logErrorFmt("[LastfmScrobbler] Invalid response body (missing line feed): '#'.", responseBody.c_str());
		return false;
	}
	if (unlikely(seqEnd - seqBegin != 2 || *seqBegin != 'O' || *(seqBegin + 1) != 'K')) {
		// TODO handle non-OK responses differently (e.g. if BANNED then disable the plugin).
		logErrorFmt("[LastfmScrobbler] Unable to authenticate the user to Last.fm. Reason: '#'.",
				std::make_pair(seqBegin, seqEnd));
		return false;
	}

	// Session ID.
	seqBegin = seqEnd + 1;
	seqEnd = std::find_if(seqBegin, end, lastfmResponseDelim);
	m_sessionId.assign(seqBegin, seqEnd);

	if (unlikely(seqEnd == end)) {
		logErrorFmt("[LastfmScrobbler] Invalid response body: '#'.", responseBody.c_str());
		return false;
	}

	// Now-playing URL. It is ignored for now.
	seqBegin = seqEnd + 1;
	seqEnd = std::find_if(seqEnd + 1, end, lastfmResponseDelim);
	m_nowPlayingUrl.assign(seqBegin, seqEnd);

	if (unlikely(seqEnd == end)) {
		logErrorFmt("[LastfmScrobbler] Invalid response body: '#'.", responseBody.c_str());
		return false;
	}

	// Submission URL.
	seqBegin = seqEnd + 1;
	seqEnd = std::find_if(seqBegin, end, lastfmResponseDelim);
	m_submissionUrl.assign(seqBegin, seqEnd);

	logDebugMsg("[LastfmScrobbler] The user is authenticated..."_s);
	m_authenticated = true;
	return true;
}

inline void LastfmScrobbler::deauthenticate() noexcept
{
	assertLocked();
	// Updated in the order they are declared in.
	m_sessionId.clear();
	m_submissionUrl.clear();
	m_authenticated = false;
}
