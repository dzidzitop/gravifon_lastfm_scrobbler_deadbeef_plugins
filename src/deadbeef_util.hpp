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
#ifndef DEADBEEF_UTIL_HPP_
#define DEADBEEF_UTIL_HPP_

#include <algorithm>
#include <cstddef>
#include <deadbeef.h>
#include <chrono>
#include <cstring>
#include <utility>

#include <afc/logger.hpp>
#include <afc/SimpleString.hpp>
#include <afc/StringRef.hpp>
#include <afc/utils.h>

#include "HttpClient.hpp"
#include "Scrobbler.hpp"

using afc::operator"" _s;

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

inline afc::String convertMultiTag(const char * const multiTag)
{
	/* Converts \n to \0. The former is used by DeaDBeeF as the value separator,
	 * the latter is used by ScrobbleInfo.
	 */
	std::size_t n = std::strlen(multiTag);
	afc::FastStringBuffer<char, afc::AllocMode::accurate> buf(n);
	auto p = buf.borrowTail();
	const char *start = multiTag, *end;
	while ((end = std::strchr(start, Track::multiTagSeparator())) != nullptr) {
		p = std::copy(start, end, p);
		*p = u8"\0"[0];
		++p;
		start = end + 1;
	}
	// Adding the last tag.
	p = std::copy_n(start, &(*p) - buf.data(), p);
	buf.returnTail(p);
	return afc::String::move(buf);
}

inline afc::Optional<Track> getTrackInfo(DB_playItem_t * const track, DB_functions_t &deadbeef)
{
	if (track == nullptr) {
		// No new track is started.
		return afc::Optional<Track>::none();
	}

	// DeaDBeeF track metadata are returned in UTF-8. No additional conversion is needed.
	const char * const title = deadbeef.pl_find_meta(track, "title");
	if (title == nullptr) {
		// Track title is a required field.
		return afc::Optional<Track>::none();
	}

	const char *albumArtist = deadbeef.pl_find_meta(track, "album artist");
	if (albumArtist == nullptr) {
		albumArtist = deadbeef.pl_find_meta(track, "albumartist");
		if (albumArtist == nullptr) {
			albumArtist = deadbeef.pl_find_meta(track, "band");
		}
	}

	const char *artist = deadbeef.pl_find_meta(track, "artist");
	if (artist == nullptr) {
		artist = albumArtist;
		if (artist == nullptr) {
			// Track artist is a required field.
			return afc::Optional<Track>::none();
		}
	}
	const char * const album = deadbeef.pl_find_meta(track, "album");

	afc::Optional<Track> result;
	Track &trackInfo = result.value();

	trackInfo.setTitle(title);
	if (album != nullptr) {
		trackInfo.setAlbumTitle(album);
	}
	/* Note: as of DeaDBeeF 0.5.6 track duration and play time values are approximate.
	 * Moreover, if the track is played from start to end without rewinding
	 * then the play time could be different from the track duration.
	 */
	const double trackDuration = double(deadbeef.pl_get_item_duration(track)); // in seconds
	trackInfo.setDurationMillis(toLongMillis(trackDuration));

	/* Adding artists. DeaDBeeF returns them as '\n'-separated values within a single string.
	 * This is the format used by Track so no conversion is required.
	 */
	trackInfo.setArtists(convertMultiTag(artist));

	if (albumArtist != nullptr) {
		trackInfo.setAlbumArtists(convertMultiTag(albumArtist));
	}

	// TODO avoid unnecessary moving.
	return result;
}

inline afc::Optional<ScrobbleInfo> getScrobbleInfo(ddb_event_trackchange_t * const trackChangeEvent,
		DB_functions_t &deadbeef, const double scrobbleThreshold)
{ PlaylistLock lock(deadbeef);
	using std::chrono::system_clock;

	DB_playItem_t * const track = trackChangeEvent->from;

	if (track == nullptr) {
		// Nothing to scrobble.
		return afc::Optional<ScrobbleInfo>::none();
	}

	/* Note: as of DeaDBeeF 0.5.6 track duration and play time values are approximate.
	 * Moreover, if the track is played from start to end without rewinding
	 * then the play time could be different from the track duration.
	 */
	const double trackPlayDuration = double(trackChangeEvent->playtime); // in seconds
	const double trackDuration = double(deadbeef.pl_get_item_duration(track)); // in seconds

	if (trackDuration <= 0.d || trackPlayDuration < (scrobbleThreshold * trackDuration)) {
		// The track was not played long enough to be scrobbled or its duration is zero or negative.
		afc::logger::logDebug(
				"The track is played not long enough to be scrobbled (play duration: "_s,
				trackPlayDuration, "s; track duration: s)."_s, trackDuration);
		return afc::Optional<ScrobbleInfo>::none();
	}

	// DeaDBeeF track metadata are returned in UTF-8. No additional conversion is needed.
	const char * const title = deadbeef.pl_find_meta(track, "title");
	if (title == nullptr) {
		// Track title is a required field.
		return afc::Optional<ScrobbleInfo>::none();
	}

	const char *albumArtist = deadbeef.pl_find_meta(track, "album artist");
	if (albumArtist == nullptr) {
		albumArtist = deadbeef.pl_find_meta(track, "albumartist");
		if (albumArtist == nullptr) {
			albumArtist = deadbeef.pl_find_meta(track, "band");
		}
	}

	const char *artist = deadbeef.pl_find_meta(track, "artist");
	if (artist == nullptr) {
		artist = albumArtist;
		if (artist == nullptr) {
			// Track artist is a required field.
			return afc::Optional<ScrobbleInfo>::none();
		}
	}
	const char * const album = deadbeef.pl_find_meta(track, "album");

	afc::Optional<ScrobbleInfo> result;
	ScrobbleInfo &scrobbleInfo = result.value();
	scrobbleInfo.scrobbleStartTimestamp = trackChangeEvent->started_timestamp;
	scrobbleInfo.scrobbleEndTimestamp = system_clock::to_time_t(system_clock::now());
	scrobbleInfo.scrobbleDuration = toLongMillis(trackPlayDuration);
	Track &trackInfo = scrobbleInfo.track;
	trackInfo.setTitle(title);
	if (album != nullptr) {
		trackInfo.setAlbumTitle(album);
	}
	trackInfo.setDurationMillis(toLongMillis(trackDuration));

	/* Adding artists. DeaDBeeF returns them as '\n'-separated values within a single string.
	 * This is the format used by Track so no conversion is required.
	 */
	trackInfo.setArtists(convertMultiTag(artist));

	if (albumArtist != nullptr) {
		trackInfo.setAlbumArtists(convertMultiTag(albumArtist));
	}

	// TODO avoid unnecessary moving.
	return result;
}

inline bool isAscii(const char * const str, const std::size_t n) noexcept
{
	for (std::size_t i = 0; i < n; ++i) {
		const unsigned char c = static_cast<unsigned char>(str[i]);
		if (c >= 128) {
			return false;
		}
	}
	return true;
}

class FastStringBufferAppender : public HttpResponse::BodyAppender
{
	FastStringBufferAppender(const FastStringBufferAppender &) = delete;
	FastStringBufferAppender(FastStringBufferAppender &&) = delete;
	FastStringBufferAppender &operator=(const FastStringBufferAppender &) = delete;
	FastStringBufferAppender &operator=(FastStringBufferAppender &&) = delete;
public:
	FastStringBufferAppender(afc::FastStringBuffer<char> &dest) : m_dest(dest) {}

	void operator()(const char * const data, const std::size_t n) override
	{
		m_dest.reserve(m_dest.size() + n);
		m_dest.append(data, n);
	}
private:
	afc::FastStringBuffer<char> &m_dest;
};

#endif /* DEADBEEF_UTIL_HPP_ */
