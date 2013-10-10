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
#include <ctime>

using namespace std;

namespace
{
	static unique_ptr<GravifonClient> gravifonClient;

	static DB_misc_t plugin = {};
	static DB_functions_t *deadbeef;

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

	unique_ptr<ScrobbleInfo> getScrobbleInfo(ddb_event_trackchange_t * const trackChangeEvent)
	{
		{ PlaylistLock lock;
			DB_playItem_t * const track = trackChangeEvent->from;
			const float trackPlayDuration = trackChangeEvent->playtime; // in seconds

			if (track == nullptr || trackPlayDuration == 0.f) {
				// Nothing to scrobble.
				return nullptr;
			}

			const char * const title = deadbeef->pl_find_meta(track, "title");
			if (title == nullptr) {
				// Track title is a required field.
				return nullptr;
			}
			const char * artist = deadbeef->pl_find_meta(track, "artist");
			if (artist == nullptr) {
				artist = deadbeef->pl_find_meta(track, "band");
				if (artist == nullptr) {
					artist = deadbeef->pl_find_meta(track, "album artist");
					if (artist == nullptr) {
						artist = deadbeef->pl_find_meta(track, "albumartist");
						if (artist == nullptr) {
							// Track artist is a required field.
							return nullptr;
						}
					}
				}
			}
			const char * const album = deadbeef->pl_find_meta(track, "album");
			const float trackDuration = deadbeef->pl_get_item_duration(track); // in seconds

			unique_ptr<ScrobbleInfo> scrobbleInfo(new ScrobbleInfo());
			scrobbleInfo->scrobbleStartTimestamp = trackChangeEvent->started_timestamp;
			scrobbleInfo->scrobbleEndTimestamp = time(nullptr);
			scrobbleInfo->scrobbleDuration = toLongMillis(trackPlayDuration);
			Track &trackInfo = scrobbleInfo->track;
			trackInfo.setTitle(title);
			trackInfo.setArtist(artist);
			if (album != nullptr) {
				trackInfo.setAlbumTitle(album);
			}
			trackInfo.setDurationMillis(toLongMillis(trackDuration));
			return scrobbleInfo;
		}
	}
}

int gravifonScrobblerStart()
{
	return 0;
}

int gravifonScrobblerStop()
{
	return 0;
}

bool initClient()
{
	{ ConfLock lock;
		const bool enabled = deadbeef->conf_get_int("gravifonScrobbler.enabled", 0);
		if (!enabled) {
			return false;
		}

		const char * const scrobblerUrl = deadbeef->conf_get_str_fast("gravifonScrobbler.scrobblerUrl", "");
		const char * const username = deadbeef->conf_get_str_fast("gravifonScrobbler.username", "");
		const char * const password = deadbeef->conf_get_str_fast("gravifonScrobbler.password", "");

		// TODO support changed configuration
		if (gravifonClient.get() == nullptr) {
			gravifonClient.reset(new GravifonClient(scrobblerUrl, username, password));
		}
		return true;
	}
}

int gravifonScrobblerMessage(const uint32_t id, const uintptr_t ctx, const uint32_t p1, const uint32_t p2)
{
	if (id != DB_EV_SONGCHANGED) {
		return 0;
	}

	// TODO add plugin-level mutex
	try {
		// TODO distinguish disabled scrobbling and gravifon client init errors
		if (!initClient()) {
			return 0;
		}

		unique_ptr<ScrobbleInfo> scrobbleInfo = getScrobbleInfo(reinterpret_cast<ddb_event_trackchange_t *>(ctx));

		if (scrobbleInfo != nullptr) {
			gravifonClient->scrobble(*scrobbleInfo);
		}
		return 0;
	}
	catch (...) {
		// TODO handle errors
		return 1;
	}
}

extern "C"
{
	DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api);
}

DB_plugin_t *gravifon_scrobbler_load(DB_functions_t * const api)
{
	deadbeef = api;

	plugin.plugin.api_vmajor = 1;
	plugin.plugin.api_vminor = 4;
	plugin.plugin.version_major = 1;
	plugin.plugin.version_minor = 0;
	plugin.plugin.type = DB_PLUGIN_MISC;
	plugin.plugin.name = "gravifon scrobbler";
	plugin.plugin.descr = "An audio track scrobbler to Gravifon.";
	plugin.plugin.copyright =
		"Copyright (C) 2013 Dźmitry Laŭčuk\n"
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

	plugin.plugin.website = "https://github.com/dzidzitop/gravifon_scrobbler_deadbeef_plugin";
	plugin.plugin.start = gravifonScrobblerStart;
	plugin.plugin.stop = gravifonScrobblerStop;
	plugin.plugin.configdialog =
		"property \"Enable scrobbler\" checkbox gravifonScrobbler.enabled 0;"
		"property \"Username\" entry gravifonScrobbler.username \"\";"
		"property \"Password\" password gravifonScrobbler.password \"\";"
		"property \"Scrobbler URL\" entry gravifonScrobbler.scrobblerUrl \"\";";

	plugin.plugin.message = gravifonScrobblerMessage;

	return DB_PLUGIN(&plugin);
}
