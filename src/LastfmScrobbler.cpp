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
#include "HttpClient.hpp"
#include "logger.hpp"

using namespace std;

void LastfmScrobbler::stopExtra()
{
	m_scrobblerUrl.clear();
}

void LastfmScrobbler::configure(const char * const serverUrl, const string &username, const string &password)
{ lock_guard<mutex> lock(m_mutex);
	assert(serverUrl != nullptr);

	if (m_scrobblerUrl != serverUrl) {
		// The configuration has changed. Updating it as well as resetting the 'scrobbles to wait' counter.
		m_scrobblerUrl = serverUrl;
		m_scrobblesToWait = MIN_SCROBBLES_TO_WAIT;
	}

	m_configured = true;
}

size_t LastfmScrobbler::doScrobbling()
{
	assert(!m_pendingScrobbles.empty());
	/* Ensures that this function is executed within the critical section against m_mutex.
	 * Even though mutex::try_lock() has side effects it is fine to acquire the lock m_mutex
	 * since the application is terminated immediately in this case.
	 */
	assert(!m_mutex.try_lock());

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

	// TODO implement scrobbling.
}
