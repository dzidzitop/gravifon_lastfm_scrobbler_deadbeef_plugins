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
#include <deadbeef.h>
#include <cstdint>
#include <memory>
#include "Scrobbler.hpp"
#include "ScrobblerInfo.hpp"
#include "LastfmScrobbler.hpp"
#include <mutex>
#include "logger.hpp"
#include <afc/utils.h>
#include <utility>
#include "pathutil.hpp"
#include "deadbeef_util.hpp"

using namespace std;
using namespace afc;

namespace
{
	static LastfmScrobbler lastfmClient;

	// These variables must be accessed within the critical section against pluginMutex.
	static DB_misc_t plugin = {};
	static DB_functions_t *deadbeef;
	static double scrobbleThreshold = 0.d;

	static mutex pluginMutex;

	/**
	 * Starts (if needed) the Lastfm client and configures it according to the
	 * Lastfm scrobbler plugin settings. If the settings are updated then the
	 * Lastfm client is re-configured. If scrobbling to Lastfm is disabled
	 * then the Lastfm client is stopped (if needed).
	 *
	 * @param safeScrobbling assigned to true if failure-safe scrobbling is enabled;
	 *         assigned to false otherwise.
	 *
	 * @return true if the Lastfm client is started and able to accept scrobbles;
	 *         false is returned otherwise.
	 */
	inline bool initClient(bool &safeScrobbling)
	{ ConfLock lock(*deadbeef);
		const bool enabled = deadbeef->conf_get_int("lastfmScrobbler.enabled", 0);
		const bool clientStarted = lastfmClient.started();
		if (!enabled) {
			if (clientStarted) {
				if (!lastfmClient.stop()) {
					logError("[lastfm_scrobbler] unable to stop Last.fm client.");
				}
			}
			return false;
		} else if (!clientStarted) {
			if (!lastfmClient.start()) {
				logError("[lastfm_scrobbler] unable to start Last.fm client.");
				return false;
			}
		}

		safeScrobbling = deadbeef->conf_get_int("lastfmScrobbler.safeScrobbling", 0);

		// DeaDBeeF configuration records are returned in UTF-8.
		const char * const lastfmUrlInUtf8 = deadbeef->conf_get_str_fast(
				"lastfmScrobbler.lastfmUrl", u8"http://post.audioscrobbler.com");

		// Only ASCII subset of ISO-8859-1 is valid to be used in username and password.
		const char * const usernameInUtf8 = deadbeef->conf_get_str_fast("lastfmScrobbler.username", "");
		string usernameInAscii;
		if (!utf8ToAscii(usernameInUtf8, usernameInAscii)) {
			logError("[lastfm_scrobbler] Non-ASCII characters are present in the username.");
			lastfmClient.invalidateConfiguration();
			// Scrobbles are still to be recorded though not submitted.
			return true;
		}

		const char * const passwordInUtf8 = deadbeef->conf_get_str_fast("lastfmScrobbler.password", "");
		string passwordInAscii;
		if (!utf8ToAscii(passwordInUtf8, passwordInAscii)) {
			logError("[lastfm_scrobbler] Non-ASCII characters are present in the password.");
			lastfmClient.invalidateConfiguration();
			// Scrobbles are still to be recorded though not submitted.
			return true;
		}

		double threshold = deadbeef->conf_get_float("lastfmScrobbler.threshold", 0.f);
		if (threshold < 0.d || threshold > 100.d) {
			threshold = 0.d;
		}
		scrobbleThreshold = threshold / 100.d;

		// TODO do not re-configure if settings are the same.
		lastfmClient.configure(convertFromUtf8(lastfmUrlInUtf8, systemCharset().c_str()).c_str(),
				usernameInAscii.c_str(), passwordInAscii.c_str());

		return true;
	}

	int lastfmScrobblerStart()
	{ lock_guard<mutex> lock(pluginMutex);
		logDebug("[lastfm_scrobbler] Starting...");

		// TODO think of making it configurable.
		string dataFilePath;
		if (::getDataFilePath("deadbeef/lastfm_scrobbler_data", dataFilePath) != 0) {
			return 1;
		}

		/* must be invoked before lastfmClient.start() to let pending scrobbles
		 * be loaded from the data file.
		 */
		lastfmClient.setDataFilePath(move(dataFilePath));

		const bool enabled = deadbeef->conf_get_int("lastfmScrobbler.enabled", 0);
		if (enabled && !lastfmClient.start()) {
			return 1;
		}

		return 0;
	}

	int lastfmScrobblerStop()
	{
		logDebug("[lastfm_scrobbler] Stopping...");
		return lastfmClient.stop() ? 0 : 1;
	}

	int lastfmScrobblerMessage(const uint32_t id, const uintptr_t ctx, const uint32_t p1, const uint32_t p2)
	{
		if (id != DB_EV_SONGCHANGED) {
			return 0;
		}

		{ lock_guard<mutex> lock(pluginMutex);
			bool safeScrobbling;

			// TODO distinguish disabled scrobbling and Lastfm client init errors
			if (!initClient(safeScrobbling)) {
				return 0;
			}

			ddb_event_trackchange_t * const event = reinterpret_cast<ddb_event_trackchange_t *>(ctx);
			const unique_ptr<ScrobbleInfo> scrobbleInfo = getScrobbleInfo(event, *deadbeef, scrobbleThreshold);

			if (scrobbleInfo != nullptr) {
				lastfmClient.scrobble(*scrobbleInfo, safeScrobbling);
			}
			return 0;
		}
	}
}

extern "C" DB_plugin_t *lastfm_scrobbler_load(DB_functions_t * const api)
{ lock_guard<mutex> lock(pluginMutex);
	deadbeef = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 4;
	plugin.plugin.version_major = 1;
	plugin.plugin.version_minor = 0;
	plugin.plugin.type = DB_PLUGIN_MISC;
	plugin.plugin.name = u8"lastfm scrobbler";
	plugin.plugin.descr = u8"An audio track scrobbler to Last.fm.";
	plugin.plugin.copyright =
		u8"Copyright (C) 2013-2014 Dźmitry Laŭčuk\n"
		"\n"
		"This program is free software: you can redistribute it and/or modify\n"
		"it under the terms of the GNU General Public License as published by\n"
		"the Free Software Foundation, either version 3 of the License, or\n"
		"(at your option) any later version.\n"
		"\n"
		"This program is distributed in the hope that it will be useful,\n"
		"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"GNU General Public License for more details.\n"
		"\n"
		"You should have received a copy of the GNU General Public License\n"
		"along with this program.  If not, see <http://www.gnu.org/licenses/>.\n";

	plugin.plugin.website = u8"https://github.com/dzidzitop/gravifon_scrobbler_deadbeef_plugin";
	plugin.plugin.start = lastfmScrobblerStart;
	plugin.plugin.stop = lastfmScrobblerStop;
	plugin.plugin.configdialog =
		R"(property "Enable scrobbler" checkbox lastfmScrobbler.enabled 0;)"
		R"(property "Username" entry lastfmScrobbler.username "";)"
		R"(property "Password" password lastfmScrobbler.password "";)"
		R"(property "URL to Last.fm API" entry lastfmScrobbler.lastfmUrl ")" u8"http://post.audioscrobbler.com" "\";"
		R"_(property "Scrobble threshold (%)" entry lastfmScrobbler.threshold "0.0";)_"
		R"(property "Failure-safe scrobbling" checkbox lastfmScrobbler.safeScrobbling 0;)";

	plugin.plugin.message = lastfmScrobblerMessage;

	return DB_PLUGIN(&plugin);
}
