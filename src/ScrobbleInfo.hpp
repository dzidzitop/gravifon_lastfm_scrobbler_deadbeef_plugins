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
#ifndef SCROBBLER_INFO_HPP_
#define SCROBBLER_INFO_HPP_

#include <cassert>
#include <cstddef>
#include <cstring>
#include <utility>
#include <afc/dateutil.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/utils.h>

class ScrobbleInfo;
class TrackInfoBuilder;

// All strings are utf8-encoded.
class Track
{
	friend class ScrobbleInfo;
	friend class TrackInfoBuilder;
private:
	// Copying scrobbles is expensive. Move semantics is forced for them.
	Track(const Track &) = delete;
	Track &operator=(const Track &) = default;
public:
	Track() : m_data(31) {} // Reasonable buffer size to keep balance between re-allocs and memory usage overhead.
	Track(Track &&) = default;

	~Track() = default;

	Track &operator=(Track &&) = default;

	const char *getTitleBegin() const noexcept { return m_data.data(); }
	const char *getTitleEnd() const noexcept { return m_data.data() + m_artistsBegin; }

	const char *getArtistsBegin() const noexcept { return m_data.data() + m_artistsBegin; }
	const char *getArtistsEnd() const noexcept { return m_data.data() + m_albumTitleBegin; }

	const char *getAlbumTitleBegin() const noexcept { return m_data.data() + m_albumTitleBegin; }
	const char *getAlbumTitleEnd() const noexcept { return m_data.data() + m_albumArtistsBegin; }

	const char *getAlbumArtistsBegin() const noexcept { return m_data.data() + m_albumArtistsBegin; }
	const char *getAlbumArtistsEnd() const noexcept { return m_data.data() + m_data.size(); }

	void setDurationMillis(const long duration) { m_durationMillis = duration; }
	long getDurationMillis() const noexcept { return m_durationMillis; }

	constexpr static char multiTagSeparator() noexcept { return u8"\0"[0]; }
// TODO make raw data private.
public:
	// TODO replace with afc::String which is size_t more compact by sizeof(std::size_t).
	afc::FastStringBuffer<char> m_data;
	std::size_t m_artistsBegin;
	std::size_t m_albumTitleBegin;
	std::size_t m_albumArtistsBegin;
	// Track duration in milliseconds.
	long m_durationMillis;
};

class ScrobbleInfo
{
private:
	// Copying scrobbles is expensive. Move semantics is forced for them.
	ScrobbleInfo(const ScrobbleInfo &) = delete;
	ScrobbleInfo &operator=(const ScrobbleInfo &) = default;
public:
	ScrobbleInfo() = default;
	ScrobbleInfo(ScrobbleInfo &&) = default;

	~ScrobbleInfo() = default;

	ScrobbleInfo &operator=(ScrobbleInfo &&) = default;

	static afc::Optional<ScrobbleInfo> parse(const char * const begin, const char * const end)
	{
		ScrobbleInfo result;
		if (parse(begin, end, result)) {
			return afc::Optional<ScrobbleInfo>(std::move(result));
		} else {
			return afc::Optional<ScrobbleInfo>::none();
		}
	}

	static bool parse(const char *begin, const char *end, ScrobbleInfo &dest);

	// Date and time when scrobble event was initiated.
	afc::TimestampTZ scrobbleStartTimestamp;
	// Date and time when scrobble event was finished.
	afc::TimestampTZ scrobbleEndTimestamp;
	// Scrobble length in milliseconds.
	long scrobbleDuration;
	// Track to scrobble.
	Track track;
};

class TrackInfoBuilder {
public:
	TrackInfoBuilder(Track &dest) : m_dest(dest) {}

	void setTitle(const char * const begin)
	{
#ifndef NDEBUG
		assert(m_state == trackTitle);
		m_state = trackArtists;
#endif
		const std::size_t size = std::strlen(begin);
		m_dest.m_data.reserve(m_dest.m_data.size() + size);
		m_dest.m_data.append(begin, size);
		m_dest.m_artistsBegin = size;
	}

	void artistsProcessed()
	{
#ifndef NDEBUG
		assert(m_state == trackArtists);
		m_state = albumTitle;
#endif
		m_dest.m_albumTitleBegin = m_dest.m_data.size();
	}

	void setAlbumTitle(const char * const begin)
	{
#ifndef NDEBUG
		assert(m_state == albumTitle);
		m_state = albumArtists;
#endif
		const std::size_t size = std::strlen(begin);
		m_dest.m_data.reserve(m_dest.m_data.size() + size);
		m_dest.m_data.append(begin, size);
		m_dest.m_albumArtistsBegin = m_dest.m_data.size();
	}

	void noAlbumTitle()
	{
#ifndef NDEBUG
		assert(m_state == albumTitle);
		m_state = albumArtists;
#endif
		m_dest.m_albumArtistsBegin = m_dest.m_data.size();
	}

	// TODO replace with more memory-efficient addAlbumArtist
	void albumArtistsProcessed()
	{
#ifndef NDEBUG
		assert(m_state == albumArtists);
		m_state = noAction;
#endif
	}

	void noAlbumArtists() noexcept
	{
#ifndef NDEBUG
		assert(m_state == albumArtists);
		m_state = noAction;
#endif
	}

	void build()
	{
		assert(m_state == noAction);
	}

	afc::FastStringBuffer<char> &getBuf() noexcept { return m_dest.m_data; }
private:
	Track &m_dest;
#ifndef NDEBUG
	enum state { trackTitle, trackArtists, albumTitle, albumArtists, noAction };
	state m_state = trackTitle;
#endif
};

// Writes this ScrobbleInfo in the JSON format to a buffer and returns the latter.
afc::FastStringBuffer<char, afc::AllocMode::accurate> serialiseAsJson(const ScrobbleInfo &scrobbleInfo);
void appendAsJson(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &dest);

#endif /* SCROBBLER_INFO_HPP_ */
