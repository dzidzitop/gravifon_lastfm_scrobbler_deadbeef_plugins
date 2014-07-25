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
#include "Scrobbler.hpp"
#include <bitset>
#include <cstddef>
#include <utility>
#include <afc/builtin.hpp>
#include <afc/dateutil.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/number.h>
#include <afc/StringRef.hpp>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include "jsonutil.hpp"

using afc::operator"" _s;
using namespace std;
using Json::Value;
using Json::ValueType;

namespace
{
	bitset<256> initJsonCharsToEscape()
	{
		bitset<256> result;
		for (auto i = 0; i < 0x20; ++i) {
			result.set(i);
		}
		result.set('"');
		result.set('\\');
		result.set('/');
		result.set(0x7f);
		return result;
	}

	static const bitset<256> jsonCharsToEscape = initJsonCharsToEscape();

	inline void writeJsonString(const string &src, afc::FastStringBuffer<char> &dest)
	{
		register afc::FastStringBuffer<char>::Tail p = dest.borrowTail();
		for (const char c : src) {
			// Truncated to an octet just in case non-octet bytes are used.
			const unsigned char uc = static_cast<unsigned char>(c) & 0xff;
			if (likely(!jsonCharsToEscape[uc])) {
				*p++ = c;
			} else {
				switch (c) {
				case '"':
				case '\\':
				case '/':
					*p++ = '\\';
					*p++ = c;
					break;
				case '\b':
					p = std::copy_n("\\b", 2, p);
					break;
				case '\f':
					p = std::copy_n("\\f", 2, p);
					break;
				case '\n':
					p = std::copy_n("\\n", 2, p);
					break;
				case '\r':
					p = std::copy_n("\\r", 2, p);
					break;
				case '\t':
					p = std::copy_n("\\t", 2, p);
					break;
				default:
					p = std::copy_n("\\u00", 4, p);
					p = afc::printTwoDigits(c, p);
					break;
				}
			}
		}
		dest.returnTail(p);
	}

	inline void writeJsonTimestamp(const afc::TimestampTZ &timestamp, afc::FastStringBuffer<char> &dest)
	{
		dest.returnTail(afc::formatISODateTime(timestamp, dest.borrowTail()));
	}

	inline std::size_t maxJsonSize(const ScrobbleInfo &scrobbleInfo) noexcept
	{
		const Track &track = scrobbleInfo.track;

		assert(track.hasTitle());
		assert(track.hasArtist());

		/* Each free-text label can be escaped so it is multiplied by six to cover the case
		 * when each character is unicode-escaped.
		 */
		constexpr auto maxPrintedCharSize = 6;

		std::size_t maxSize = 0;
		maxSize += 2; // {}
		maxSize += R"("scrobble_start_datetime":"")"_s.size() + afc::maxISODateTimeSize();
		maxSize += 1; // ,
		maxSize += R"("scrobble_end_datetime":"")"_s.size() + afc::maxISODateTimeSize();
		maxSize += 1; // ,
		maxSize += R"("scrobble_duration":{"amount":,"unit":"ms"})"_s.size() +
				afc::maxPrintedSize<decltype(scrobbleInfo.scrobbleDuration), 10>();
		maxSize += 1; // ,
		maxSize += R"("track":{})"_s.size();
		maxSize += R"("title":"")"_s.size() + maxPrintedCharSize * track.getTitle().size();
		maxSize += 1; // ,
		maxSize += R"("artists":[]");
		maxSize += R"({"name":""},)"_s.size() * track.getArtists().size() - 1; // With commas; the last comma is removed.
		// TODO move loop invariant.
		for (const string &artist : track.getArtists()) {
			maxSize += maxPrintedCharSize * artist.size();
		}
		maxSize += 1; // ,
		if (track.hasAlbumTitle()) {
			maxSize += R"("album":{"title":""})"_s.size();
			maxSize += maxPrintedCharSize * track.getAlbumTitle().size();
			if (track.hasAlbumArtist()) {
				maxSize += 1; // ,
				maxSize += R"("artists":[]")_s.size();
				maxSize += R"({"name":""},)"_s.size() * track.getArtists().size() - 1; // With commas; the last comma is removed.
				// TODO move loop invariant.
				for (const string &artist : track.getArtists()) {
					maxSize += maxPrintedCharSize * artist.size();
				}
			}
			maxSize += 1; // ,
		}
		maxSize += R"("length":{"amount":,"unit":"ms"})"_s.size() +
				afc::maxPrintedSize<decltype(track.getDurationMillis()), 10>();

		return maxSize;
	}

	inline void appendAsJsonImpl(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &buf) noexcept
	{
		const Track &track = scrobbleInfo.track;

		buf.append(R"({"scrobble_start_datetime":")"_s);
		writeJsonTimestamp(scrobbleInfo.scrobbleStartTimestamp, buf);
		buf.append(R"(","scrobble_end_datetime":")"_s);
		writeJsonTimestamp(scrobbleInfo.scrobbleEndTimestamp, buf);
		buf.append(R"(","scrobble_duration":{"amount":)"_s);
		buf.returnTail(afc::printNumber<long, 10>(scrobbleInfo.scrobbleDuration, buf.borrowTail()));
		buf.append(R"(,"unit":"ms"},"track":{"title":")"_s);
		writeJsonString(track.getTitle(), buf);
		// At least single artist is expected.
		buf.append(R"(","artists":[)"_s);
		for (const string &artist : track.getArtists()) {
			// TODO improve performance by merging appends.
			buf.append(R"({"name":")"_s);
			writeJsonString(artist, buf);
			buf.append(R"("},)"_s);
		}
		buf.returnTail(buf.borrowTail() - 1); // removing the last redundant comma.
		if (track.hasAlbumTitle()) {
			buf.append(R"(],"album":{"title":")"_s);
			writeJsonString(track.getAlbumTitle(), buf);
			if (track.hasAlbumArtist()) {
				buf.append(R"(","artists":[)"_s);
				for (const string &albumArtist : track.getAlbumArtists()) {
					// TODO improve performance by merging appends.
					buf.append(R"({"name":")"_s);
					writeJsonString(albumArtist, buf);
					buf.append(R"("},)"_s);
				}
				buf.returnTail(buf.borrowTail() - 1); // removing the last redundant comma.
				buf.append(R"(]},"length":{"amount":)"_s);
			} else {
				buf.append(R"("},"length":{"amount":)"_s);
			}
		} else {
			buf.append(R"(],"length":{"amount":)"_s);
		}
		buf.returnTail(afc::printNumber<long, 10>(track.getDurationMillis(), buf.borrowTail()));
		buf.append(R"(,"unit":"ms"}}})"_s);
	}

	inline bool parseDateTime(const Value &dateTimeObject, afc::TimestampTZ &dest) noexcept
	{
		return isType(dateTimeObject, stringValue) && parseISODateTime(dateTimeObject.asString(), dest);
	}

	inline bool parseDuration(const Value &durationObject, long &dest)
	{
		if (!isType(durationObject, objectValue)) {
			return false;
		}
		const Value &amount = durationObject["amount"];
		const Value &unit = durationObject["unit"];
		if (!isType(amount, intValue) || !isType(unit, stringValue)) {
			return false;
		}
		const long val = amount.asInt();
		const string unitValue = unit.asString();
		if (unitValue == "ms") {
			dest = val;
			return true;
		} else if (unitValue == "s") {
			dest = val * 1000;
			return true;
		}
		return false;
	}

	template<typename AddArtistOp>
	inline bool parseArtists(const Value &artists, const bool artistsRequired, AddArtistOp addArtistOp)
	{
		if (!isType(artists, arrayValue) || artists.empty()) {
			return !artistsRequired;
		}
		for (auto i = 0u, n = artists.size(); i < n; ++i) {
			const Value &artist = artists[i];
			if (!isType(artist, objectValue)) {
				return false;
			}
			const Value &artistName = artist["name"];
			if (!isType(artistName, stringValue)) {
				return false;
			}
			addArtistOp(std::move(artistName.asString()));
		}
		return true;
	}
}

bool ScrobbleInfo::parse(const char * const begin, const char * const end, ScrobbleInfo &dest)
{
	Json::Reader jsonReader;
	Value object;

	if (!jsonReader.parse(begin, end, object, false)) {
		logError(string("[Scrobbler] Unable to parse the scrobble JSON object: ") +
				jsonReader.getFormatedErrorMessages());
		return false;
	}
	if (!isType(object, objectValue)) {
		return false;
	}
	if (!parseDateTime(object["scrobble_start_datetime"], dest.scrobbleStartTimestamp) ||
			!parseDateTime(object["scrobble_end_datetime"], dest.scrobbleEndTimestamp) ||
			!parseDuration(object["scrobble_duration"], dest.scrobbleDuration)) {
		return false;
	}

	Track &track = dest.track;

	const Value trackObject = object["track"];
	if (!isType(trackObject, objectValue)) {
		return false;
	}
	const Value &trackTitle = trackObject["title"];
	if (!isType(trackTitle, stringValue)) {
		return false;
	}
	track.setTitle(trackTitle.asCString());

	const Value &trackAlbum = trackObject["album"];
	const ValueType trackAlbumObjType = trackAlbum.type();
	if (trackAlbumObjType != nullValue) {
		if (trackAlbumObjType != objectValue) {
			return false;
		}
		const Value &trackAlbumTitle = trackAlbum["title"];
		if (!isType(trackAlbumTitle, stringValue)) {
			return false;
		}
		track.setAlbumTitle(trackAlbumTitle.asCString());

		if (!parseArtists(trackAlbum["artists"], false,
				[&](string &&artistName) { track.addAlbumArtist(std::move(artistName)); })) {
			return false;
		}
	}

	long trackDuration;
	if (!parseDuration(trackObject["length"], trackDuration)) {
		return false;
	}
	track.setDurationMillis(trackDuration);

	if (!parseArtists(trackObject["artists"], true,
			[&](string &&artistName) { track.addArtist(std::move(artistName)); })) {
		return false;
	}

	return true;
}

afc::FastStringBuffer<char> serialiseAsJson(const ScrobbleInfo &scrobbleInfo)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	afc::FastStringBuffer<char> buf(maxSize);

	appendAsJsonImpl(scrobbleInfo, buf);

	return buf;
}

void appendAsJson(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &dest)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	dest.reserve(dest.size() + maxSize);

	appendAsJsonImpl(scrobbleInfo, dest);
}
