/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2015 Dźmitry Laŭčuk

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
#include <cstring>
#include <memory>
#include <mutex>
#include <utility>
#include <afc/ensure_ascii.hpp>
#include <afc/logger.hpp>
#include <afc/StringRef.hpp>
#include "deadbeef_util.hpp"
#include "GravifonScrobbler.hpp"
#include "pathutil.hpp"
#include "ScrobbleInfo.hpp"
#include "Scrobbler.hpp"

using namespace std;

using afc::operator"" _s;
using afc::logger::logDebug;
using afc::logger::logDebugMsg;
using afc::logger::logError;
using afc::logger::logErrorMsg;

namespace
{
	static GravifonScrobbler gravifonClient;

	// These variables must be accessed within the critical section against pluginMutex.
	static DB_misc_t plugin = {};
	static DB_functions_t *deadbeef;
	static double scrobbleThreshold = 0.d;

	static mutex pluginMutex;

	/**
	 * Starts (if needed) the Gravifon client and configures it according to the
	 * Gravifon scrobbler plugin settings. If the settings are updated then the
	 * Gravifon client is re-configured. If scrobbling to Gravifon is disabled
	 * then the Gravifon client is stopped (if needed).
	 *
	 * @param safeScrobbling assigned to true if failure-safe scrobbling is enabled;
	 *         assigned to false otherwise.
	 *
	 * @return true if the Gravifon client is started and able to accept scrobbles;
	 *         false is returned otherwise.
	 */
	inline bool initClient(bool &safeScrobbling)
	{ ConfLock lock(*deadbeef);
		const bool enabled = deadbeef->conf_get_int("gravifonScrobbler.enabled", 0);
		const bool clientStarted = gravifonClient.started();
		if (!enabled) {
			if (clientStarted) {
				if (!gravifonClient.stop()) {
					logErrorMsg("[gravifon_scrobbler] unable to stop Gravifon client."_s);
				}
			}
			return false;
		} else if (!clientStarted) {
			if (!gravifonClient.start()) {
				logErrorMsg("[gravifon_scrobbler] unable to start Gravifon client."_s);
				return false;
			}
		}

		safeScrobbling = deadbeef->conf_get_int("gravifonScrobbler.safeScrobbling", 0);

		// DeaDBeeF configuration records are returned in UTF-8.
		const char * const gravifonUrl = deadbeef->conf_get_str_fast(
				"gravifonScrobbler.gravifonUrl", u8"http://api.gravifon.org/v1");
		const std::size_t gravifonUrlSize = std::strlen(gravifonUrl);
		if (!isAscii(gravifonUrl, gravifonUrlSize)) {
			logErrorMsg("[gravifon_scrobbler] Non-ASCII characters are present in the URL to Gravifon."_s);
			gravifonClient.invalidateConfiguration();
			// Scrobbles are still to be recorded though not submitted.
			return true;
		}

		/* Non-ASCII characters in username and password are definitely disallowed by Gravifon.
		 * UTF-8 strings are returned from the configuration. To avoid conversion, just checking
		 * that the username and password are from the ASCII subset.
		 */
		const char * const username = deadbeef->conf_get_str_fast("gravifonScrobbler.username", "");
		const std::size_t usernameSize = std::strlen(username);
		if (!isAscii(username, usernameSize)) {
			logErrorMsg("[gravifon_scrobbler] Non-ASCII characters are present in the username."_s);
			gravifonClient.invalidateConfiguration();
			// Scrobbles are still to be recorded though not submitted.
			return true;
		}

		const char * const password = deadbeef->conf_get_str_fast("gravifonScrobbler.password", "");
		const std::size_t passwordSize = std::strlen(password);
		if (!isAscii(password, passwordSize)) {
			logErrorMsg("[gravifon_scrobbler] Non-ASCII characters are present in the password."_s);
			gravifonClient.invalidateConfiguration();
			// Scrobbles are still to be recorded though not submitted.
			return true;
		}

		double threshold = deadbeef->conf_get_float("gravifonScrobbler.threshold", 0.f);
		if (threshold < 0.d || threshold > 100.d) {
			threshold = 0.d;
		}
		scrobbleThreshold = threshold / 100.d;

		// TODO do not re-configure if settings are the same.
		gravifonClient.configure(gravifonUrl, gravifonUrlSize, username, usernameSize, password, passwordSize);

		return true;
	}

	int gravifonScrobblerStart()
	{ lock_guard<mutex> lock(pluginMutex);
		logDebugMsg("[gravifon_scrobbler] Starting...");

		// TODO think of making it configurable.
		afc::FastStringBuffer<char, afc::AllocMode::accurate> dataFilePath;
		if (!::getDataFilePath("deadbeef/gravifon_scrobbler_data"_s, dataFilePath)) {
			return 1;
		}
		const std::size_t dataFilePathSize = dataFilePath.size();
		/* must be invoked before gravifonClient.start() to let pending scrobbles
		 * be loaded from the data file.
		 */
		gravifonClient.setDataFilePath(std::move(afc::SimpleString().attach(dataFilePath.detach(), dataFilePathSize)));

		const bool enabled = deadbeef->conf_get_int("gravifonScrobbler.enabled", 0);
		if (enabled && !gravifonClient.start()) {
			return 1;
		}

		return 0;
	}

	int gravifonScrobblerStop()
	{
		logDebugMsg("[gravifon_scrobbler] Stopping...");
		return gravifonClient.stop() ? 0 : 1;
	}

	int gravifonScrobblerMessage(const uint32_t id, const uintptr_t ctx, const uint32_t p1, const uint32_t p2)
	{
		if (id != DB_EV_SONGCHANGED) {
			return 0;
		}

		{ lock_guard<mutex> lock(pluginMutex);
			bool safeScrobbling;

			// TODO distinguish disabled scrobbling and gravifon client init errors
			if (!initClient(safeScrobbling)) {
				return 0;
			}

			ddb_event_trackchange_t * const event = reinterpret_cast<ddb_event_trackchange_t *>(ctx);
			const unique_ptr<ScrobbleInfo> scrobbleInfo = getScrobbleInfo(event, *deadbeef, scrobbleThreshold);

			if (scrobbleInfo != nullptr) {
				gravifonClient.scrobble(std::move(*scrobbleInfo), safeScrobbling);
			}
			return 0;
		}
	}
}

extern "C" DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api)
{ lock_guard<mutex> lock(pluginMutex);
	deadbeef = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 4;
	plugin.plugin.version_major = 1;
	plugin.plugin.version_minor = 0;
	plugin.plugin.type = DB_PLUGIN_MISC;
	plugin.plugin.name = u8"gravifon scrobbler";
	plugin.plugin.descr = u8"An audio track scrobbler to Gravifon.";
	plugin.plugin.copyright =
		u8"Copyright (C) 2013-2015 Dźmitry Laŭčuk\n"
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
	plugin.plugin.start = gravifonScrobblerStart;
	plugin.plugin.stop = gravifonScrobblerStop;
	plugin.plugin.configdialog =
		R"(property "Enable scrobbler" checkbox gravifonScrobbler.enabled 0;)"
		R"(property "Username" entry gravifonScrobbler.username "";)"
		R"(property "Password" password gravifonScrobbler.password "";)"
		R"(property "URL to Gravifon API" entry gravifonScrobbler.gravifonUrl ")" u8"http://api.gravifon.org/v1" "\";"
		R"_(property "Scrobble threshold (%)" entry gravifonScrobbler.threshold "0.0";)_"
		R"(property "Failure-safe scrobbling" checkbox gravifonScrobbler.safeScrobbling 0;)";

	plugin.plugin.message = gravifonScrobblerMessage;

	return DB_PLUGIN(&plugin);
}
