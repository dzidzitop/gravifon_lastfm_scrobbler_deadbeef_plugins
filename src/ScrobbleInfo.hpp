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
#include <utility>
#include <vector>
#include <afc/dateutil.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/SimpleString.hpp>
#include <afc/utils.h>

// All strings are utf8-encoded.
class Track
{
private:
	// Copying scrobbles is expensive. Move semantics is forced for them.
	Track(const Track &) = delete;
	Track &operator=(const Track &) = default;
public:
	Track() = default;
	Track(Track &&) = default;

	~Track() = default;

	Track &operator=(Track &&) = default;

	void setTitle(afc::SimpleString &&trackTitle) { m_title = std::move(trackTitle); m_titleSet = true; }
	void setTitle(const char * const trackTitle) { m_title = trackTitle; m_titleSet = true; }
	afc::SimpleString &getTitle() noexcept { assert(m_titleSet); return m_title; }
	const afc::SimpleString &getTitle() const noexcept { assert(m_titleSet); return m_title; }
	bool hasTitle() const noexcept { return m_titleSet; }

	void addArtist(afc::SimpleString &&artist) { m_artists.emplace_back(std::move(artist)); }
	void addArtist(const char * const artist) { m_artists.emplace_back(artist); }
	std::vector<afc::SimpleString> &getArtists() noexcept { return m_artists; }
	const std::vector<afc::SimpleString> &getArtists() const noexcept { return m_artists; }
	bool hasArtist() const noexcept { return !m_artists.empty(); }

	void addAlbumArtist(afc::SimpleString &&artist) { m_albumArtists.emplace_back(std::move(artist)); }
	void addAlbumArtist(const char * const artist) { m_albumArtists.emplace_back(artist); }
	std::vector<afc::SimpleString> &getAlbumArtists() noexcept { return m_albumArtists; }
	const std::vector<afc::SimpleString> &getAlbumArtists() const noexcept { return m_albumArtists; }
	bool hasAlbumArtist() const noexcept { return !m_albumArtists.empty(); }

	void setAlbumTitle(afc::SimpleString &&albumTitle) { m_album = std::move(albumTitle); m_albumSet = true; }
	void setAlbumTitle(const char * const albumTitle) { m_album = albumTitle; m_albumSet = true; }
	afc::SimpleString &getAlbumTitle() noexcept { assert(m_albumSet); return m_album; }
	const afc::SimpleString &getAlbumTitle() const noexcept { assert(m_albumSet); return m_album; }
	bool hasAlbumTitle() const noexcept { return m_albumSet; }

	void setDurationMillis(const long duration) { m_duration = duration; m_durationSet = true; }
	long getDurationMillis() const noexcept { assert(m_durationSet); return m_duration; }
	bool hasDurationMillis() const noexcept { return m_durationSet; }
private:
	afc::SimpleString m_title;
	std::vector<afc::SimpleString> m_artists;
	std::vector<afc::SimpleString> m_albumArtists;
	afc::SimpleString m_album;
	// Track duration in milliseconds.
	long m_duration;
	bool m_titleSet = false;
	bool m_albumSet = false;
	bool m_durationSet = false;
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

// Writes this ScrobbleInfo in the JSON format to a buffer and returns the latter.
afc::FastStringBuffer<char, afc::AllocMode::accurate> serialiseAsJson(const ScrobbleInfo &scrobbleInfo);
void appendAsJson(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &dest);

#endif /* SCROBBLER_INFO_HPP_ */
