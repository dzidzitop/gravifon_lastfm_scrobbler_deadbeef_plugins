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
#include <deadbeef.h>
#include <cstdint>
#include <memory>
#include "GravifonClient.hpp"
#include <chrono>
#include <mutex>
#include <cstring>
#include "logger.hpp"
#include <afc/utils.h>
#include <atomic>

using namespace std;
using namespace afc;
using std::chrono::system_clock;

namespace
{
	// The character 'Line Feed' in UTF-8.
	static const char UTF8_LF = 0x0a;

	// TODO Ensure that this code is thread-safe.
	static GravifonClient gravifonClient;

	static DB_misc_t plugin = {};
	static DB_functions_t *deadbeef;
	static atomic<double> scrobbleThreshold(0.d);

	static mutex pluginMutex;

	struct ConfLock
	{
		ConfLock() { deadbeef->conf_lock(); }
		~ConfLock() { deadbeef->conf_unlock(); }
	};

	struct PlaylistLock
	{
		PlaylistLock() { deadbeef->pl_lock(); }
		~PlaylistLock() { deadbeef->pl_unlock(); }
	};

	inline long toLongMillis(const float seconds)
	{
		return static_cast<long>(seconds * 1000);
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
	{
		{ PlaylistLock lock;
			DB_playItem_t * const track = trackChangeEvent->from;

			if (track == nullptr) {
				// Nothing to scrobble.
				return nullptr;
			}

			const double trackPlayDuration = trackChangeEvent->playtime; // in seconds
			const double trackDuration = deadbeef->pl_get_item_duration(track); // in seconds

			if (trackDuration <= 0.f || trackPlayDuration < (scrobbleThreshold * trackDuration)) {
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
			// TODO use monotonic micro/nano clock to calculate exact duration. These measurements are inaccurate.
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
}

int gravifonScrobblerStart()
{
	logDebug("[gravifon_scrobbler] Starting...");
	// TODO move initialisation phase to here (now C++ unit initialisation is used).
	// TODO Ensure this code is thread-safe.
	if (!gravifonClient.start()) {
		return 1;
	}
	return 0;
}

int gravifonScrobblerStop()
{
	logDebug("[gravifon_scrobbler] Stopping...");
	int result = 0;
	if (!gravifonClient.stop()) {
		result = 1;
	}
	// TODO Discard other resources properly.
	return result;
}

bool initClient()
{ ConfLock lock;
	const bool enabled = deadbeef->conf_get_int("gravifonScrobbler.enabled", 0);
	if (!enabled) {
		return false;
	}

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
	scrobbleThreshold.store(threshold / 100.d, memory_order_relaxed);

	// TODO do not re-configure is settings are the same.
	gravifonClient.configure(convertFromUtf8(gravifonUrlInUtf8, systemCharset().c_str()).c_str(),
			usernameInAscii.c_str(), passwordInAscii.c_str());

	return true;
}

int gravifonScrobblerMessage(const uint32_t id, const uintptr_t ctx, const uint32_t p1, const uint32_t p2)
{
	if (id != DB_EV_SONGCHANGED) {
		return 0;
	}

	{ lock_guard<mutex> lock(pluginMutex);
		// TODO distinguish disabled scrobbling and gravifon client init errors
		if (!initClient()) {
			return 0;
		}

		unique_ptr<ScrobbleInfo> scrobbleInfo = getScrobbleInfo(reinterpret_cast<ddb_event_trackchange_t *>(ctx));

		if (scrobbleInfo != nullptr) {
			gravifonClient.scrobble(*scrobbleInfo);
		}
		return 0;
	}
}

extern "C" DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api)
{
	deadbeef = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 4;
	plugin.plugin.version_major = 1;
	plugin.plugin.version_minor = 0;
	plugin.plugin.type = DB_PLUGIN_MISC;
	plugin.plugin.name = u8"gravifon scrobbler";
	plugin.plugin.descr = u8"An audio track scrobbler to Gravifon.";
	plugin.plugin.copyright =
		u8"Copyright (C) 2013 Dźmitry Laŭčuk\n"
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
		R"_(property "Scrobble threshold (%)" entry gravifonScrobbler.threshold "0.0";)_";

	plugin.plugin.message = gravifonScrobblerMessage;

	return DB_PLUGIN(&plugin);
}
