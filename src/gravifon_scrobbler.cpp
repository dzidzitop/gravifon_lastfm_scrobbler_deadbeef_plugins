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
#include "GravifonScrobbler.hpp"
#include <chrono>
#include <mutex>
#include <cstring>
#include "logger.hpp"
#include <afc/utils.h>
#include <utility>
#include "pathutil.hpp"

using namespace std;
using namespace afc;
using std::chrono::system_clock;

namespace
{
	// The character 'Line Feed' in UTF-8.
	static const char UTF8_LF = 0x0a;

	static GravifonScrobbler gravifonClient;

	// These variables must be accessed within the critical section against pluginMutex.
	static DB_misc_t plugin = {};
	static DB_functions_t *deadbeef;
	static double scrobbleThreshold = 0.d;

	static mutex pluginMutex;

	struct ConfLock
	{
		ConfLock(DB_functions_t &deadbeef) : m_deadbeef(deadbeef) { m_deadbeef.conf_lock(); }
		~ConfLock() { m_deadbeef.conf_unlock(); }
	private:
		DB_functions_t &m_deadbeef;
	};

	struct PlaylistLock
	{
		PlaylistLock(DB_functions_t &deadbeef) : m_deadbeef(deadbeef) { m_deadbeef.pl_lock(); }
		~PlaylistLock() { m_deadbeef.pl_unlock(); }
	private:
		DB_functions_t &m_deadbeef;
	};

	constexpr inline long toLongMillis(const double seconds)
	{
		return static_cast<long>(seconds * 1000.d);
	}

	template<typename AddTagOp>
	inline void addMultiTag(const char * const multiTag, AddTagOp addTagOp)
	{
		/* Adding tags one by one. DeaDBeeF returns them as
		 * '\n'-separated values within a single string.
		 */
		const char *start = multiTag, *end;
		while ((end = strchr(start, UTF8_LF)) != nullptr) {
			addTagOp(string(start, end));
			start = end + 1;
		}
		// Adding the last tag.
		addTagOp(start);
	}

	unique_ptr<ScrobbleInfo> getScrobbleInfo(ddb_event_trackchange_t * const trackChangeEvent)
	{ PlaylistLock lock(*deadbeef);
		DB_playItem_t * const track = trackChangeEvent->from;

		if (track == nullptr) {
			// Nothing to scrobble.
			return nullptr;
		}

		/* Note: as of DeaDBeeF 0.5.6 track duration and play time values are approximate.
		 * Moreover, if the track is played from start to end without rewinding
		 * then the play time could be different from the track duration.
		 */
		const double trackPlayDuration = double(trackChangeEvent->playtime); // in seconds
		const double trackDuration = double(deadbeef->pl_get_item_duration(track)); // in seconds

		if (trackDuration <= 0.d || trackPlayDuration < (scrobbleThreshold * trackDuration)) {
			// The track was not played long enough to be scrobbled or its duration is zero or negative.
			logDebug(string("The track is played not long enough to be scrobbled (play duration: ") +
					to_string(trackPlayDuration) + "s; track duration: " + to_string(trackDuration) + "s).");
			return nullptr;
		}

		// DeaDBeeF track metadata are returned in UTF-8. No additional conversion is needed.
		const char * const title = deadbeef->pl_find_meta(track, "title");
		if (title == nullptr) {
			// Track title is a required field.
			return nullptr;
		}

		const char *albumArtist = deadbeef->pl_find_meta(track, "album artist");
		if (albumArtist == nullptr) {
			albumArtist = deadbeef->pl_find_meta(track, "albumartist");
			if (albumArtist == nullptr) {
				albumArtist = deadbeef->pl_find_meta(track, "band");
			}
		}

		const char *artist = deadbeef->pl_find_meta(track, "artist");
		if (artist == nullptr) {
			artist = albumArtist;
			if (artist == nullptr) {
				// Track artist is a required field.
				return nullptr;
			}
		}
		const char * const album = deadbeef->pl_find_meta(track, "album");

		unique_ptr<ScrobbleInfo> scrobbleInfo(new ScrobbleInfo());
		scrobbleInfo->scrobbleStartTimestamp = trackChangeEvent->started_timestamp;
		scrobbleInfo->scrobbleEndTimestamp = system_clock::to_time_t(system_clock::now());
		scrobbleInfo->scrobbleDuration = toLongMillis(trackPlayDuration);
		Track &trackInfo = scrobbleInfo->track;
		trackInfo.setTitle(title);
		if (album != nullptr) {
			trackInfo.setAlbumTitle(album);
		}
		trackInfo.setDurationMillis(toLongMillis(trackDuration));

		addMultiTag(artist, [&](string &&artistName) { trackInfo.addArtist(artistName); });

		if (albumArtist != nullptr) {
			addMultiTag(albumArtist, [&](string &&artistName) { trackInfo.addAlbumArtist(artistName); });
		}

		return scrobbleInfo;
	}

	inline bool utf8ToAscii(const char * const src, string &dest)
	{
		const char *ptr = src;
		for (;;) {
			const unsigned char c = *ptr++;
			if (c >= 128) {
				return false;
			}
			if (c == 0) {
				return true;
			}
			dest.push_back(c);
		}
	}

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
					logError("[gravifon_scrobbler] unable to stop Gravifon client.");
				}
			}
			return false;
		} else if (!clientStarted) {
			if (!gravifonClient.start()) {
				logError("[gravifon_scrobbler] unable to start Gravifon client.");
				return false;
			}
		}

		safeScrobbling = deadbeef->conf_get_int("gravifonScrobbler.safeScrobbling", 0);

		// DeaDBeeF configuration records are returned in UTF-8.
		const char * const gravifonUrlInUtf8 = deadbeef->conf_get_str_fast(
				"gravifonScrobbler.gravifonUrl", u8"http://api.gravifon.org/v1");

		// Only ASCII subset of ISO-8859-1 is valid to be used in username and password.
		const char * const usernameInUtf8 = deadbeef->conf_get_str_fast("gravifonScrobbler.username", "");
		string usernameInAscii;
		if (!utf8ToAscii(usernameInUtf8, usernameInAscii)) {
			logError("[gravifon_scrobbler] Non-ASCII characters are present in the username.");
			gravifonClient.invalidateConfiguration();
			// Scrobbles are still to be recorded though not submitted.
			return true;
		}

		const char * const passwordInUtf8 = deadbeef->conf_get_str_fast("gravifonScrobbler.password", "");
		string passwordInAscii;
		if (!utf8ToAscii(passwordInUtf8, passwordInAscii)) {
			logError("[gravifon_scrobbler] Non-ASCII characters are present in the password.");
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
		gravifonClient.configure(convertFromUtf8(gravifonUrlInUtf8, systemCharset().c_str()).c_str(),
				usernameInAscii.c_str(), passwordInAscii.c_str());

		return true;
	}

	int gravifonScrobblerStart()
	{ lock_guard<mutex> lock(pluginMutex);
		logDebug("[gravifon_scrobbler] Starting...");

		// TODO think of making it configurable.
		string dataFilePath;
		if (::getDataFilePath("deadbeef/gravifon_scrobbler_data", dataFilePath) != 0) {
			return 1;
		}

		/* must be invoked before gravifonClient.start() to let pending scrobbles
		 * be loaded from the data file.
		 */
		gravifonClient.setDataFilePath(move(dataFilePath));

		const bool enabled = deadbeef->conf_get_int("gravifonScrobbler.enabled", 0);
		if (enabled && !gravifonClient.start()) {
			return 1;
		}

		return 0;
	}

	int gravifonScrobblerStop()
	{
		logDebug("[gravifon_scrobbler] Stopping...");
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
			const unique_ptr<ScrobbleInfo> scrobbleInfo = getScrobbleInfo(event);

			if (scrobbleInfo != nullptr) {
				gravifonClient.scrobble(*scrobbleInfo, safeScrobbling);
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
