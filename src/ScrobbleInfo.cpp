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
#include "Scrobbler.hpp"
#include <algorithm>
#include <bitset>
#include <cstddef>
#include <afc/builtin.hpp>
#include <afc/dateutil.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/logger.hpp>
#include <afc/number.h>
#include <afc/SimpleString.hpp>
#include <afc/StringRef.hpp>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include "jsonutil.hpp"

using namespace std;

using afc::operator"" _s;

using Json::Value;
using Json::ValueType;

namespace
{
	std::bitset<256> initJsonCharsToEscape()
	{
		std::bitset<256> result;
		for (auto i = 0; i < 0x20; ++i) {
			result.set(i);
		}
		result.set('"');
		result.set('\\');
		result.set('/');
		result.set(0x7f);
		return result;
	}

	static const std::bitset<256> jsonCharsToEscape = initJsonCharsToEscape();

	template<typename Iterator>
	Iterator writeJsonString(const afc::SimpleString &src, register Iterator dest)
	{
		for (const char c : src) {
			// Truncated to an octet just in case non-octet bytes are used.
			const unsigned char uc = static_cast<unsigned char>(c) & 0xff;
			if (!jsonCharsToEscape[uc]) {
				*dest++ = c;
			} else {
				switch (c) {
				case '"':
				case '\\':
				case '/':
					*dest++ = '\\';
					*dest++ = c;
					break;
				case '\b':
					dest = afc::copy("\\b"_s, dest);
					break;
				case '\f':
					dest = afc::copy("\\f"_s, dest);
					break;
				case '\n':
					dest = afc::copy("\\n"_s, dest);
					break;
				case '\r':
					dest = afc::copy("\\r"_s, dest);
					break;
				case '\t':
					dest = afc::copy("\\t"_s, dest);
					break;
				default:
					dest = afc::copy("\\u00"_s, dest);
					dest = afc::octetToHex(c, dest);
					break;
				}
			}
		}
		return dest;
	}

	inline std::size_t totalStringSize(const std::vector<afc::SimpleString> &strings) noexcept
	{
		std::size_t totalSize = 0;
		for (const afc::SimpleString &s : strings) {
			totalSize += s.size();
		}
		return totalSize;
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

		{ // artists
			maxSize += R"("artists":[])"_s.size();
			const std::vector<afc::SimpleString> &artists = track.getArtists();
			maxSize += R"({"name":""},)"_s.size() * artists.size() - 1; // With commas; the last comma is removed.
			maxSize += maxPrintedCharSize * totalStringSize(artists);
			maxSize += 1; // ,
		}

		if (track.hasAlbumTitle()) {
			maxSize += R"("album":{"title":""})"_s.size();
			maxSize += maxPrintedCharSize * track.getAlbumTitle().size();
			if (track.hasAlbumArtist()) {
				maxSize += 1; // ,
				maxSize += R"("artists":[])"_s.size();
				const std::vector<afc::SimpleString> &albumArtists = track.getAlbumArtists();
				maxSize += R"({"name":""},)"_s.size() * albumArtists.size() - 1; // With commas; the last comma is removed.
				maxSize += maxPrintedCharSize * totalStringSize(albumArtists);
			}
			maxSize += 1; // ,
		}

		maxSize += R"("length":{"amount":,"unit":"ms"})"_s.size() +
				afc::maxPrintedSize<decltype(track.getDurationMillis()), 10>();

		return maxSize;
	}

	template<typename Iterator>
	inline Iterator appendAsJsonImpl(const ScrobbleInfo &scrobbleInfo, register Iterator dest) noexcept
	{
		const Track &track = scrobbleInfo.track;

		dest = afc::copy(R"({"scrobble_start_datetime":")"_s, dest);
		dest = afc::formatISODateTime(scrobbleInfo.scrobbleStartTimestamp, dest);
		dest = afc::copy(R"(","scrobble_end_datetime":")"_s, dest);
		dest = afc::formatISODateTime(scrobbleInfo.scrobbleEndTimestamp, dest);
		dest = afc::copy(R"(","scrobble_duration":{"amount":)"_s, dest);
		dest = afc::printNumber<10>(scrobbleInfo.scrobbleDuration, dest);
		dest = afc::copy(R"(,"unit":"ms"},"track":{"title":")"_s, dest);
		dest = writeJsonString(track.getTitle(), dest);
		// At least single artist is expected.
		dest = afc::copy(R"(","artists":[)"_s, dest);
		for (const afc::SimpleString &artist : track.getArtists()) {
			// TODO improve performance by merging appends.
			dest = afc::copy(R"({"name":")"_s, dest);
			dest = writeJsonString(artist, dest);
			dest = afc::copy(R"("},)"_s, dest);
		}
		--dest; // removing the last redundant comma.
		if (track.hasAlbumTitle()) {
			dest = afc::copy(R"(],"album":{"title":")"_s, dest);
			dest = writeJsonString(track.getAlbumTitle(), dest);
			if (track.hasAlbumArtist()) {
				dest = afc::copy(R"(","artists":[)"_s, dest);
				for (const afc::SimpleString &albumArtist : track.getAlbumArtists()) {
					// TODO improve performance by merging appends.
					dest = afc::copy(R"({"name":")"_s, dest);
					dest = writeJsonString(albumArtist, dest);
					dest = afc::copy(R"("},)"_s, dest);
				}
				--dest; // removing the last redundant comma.
				dest = afc::copy(R"(]},"length":{"amount":)"_s, dest);
			} else {
				dest = afc::copy(R"("},"length":{"amount":)"_s, dest);
			}
		} else {
			dest = afc::copy(R"(],"length":{"amount":)"_s, dest);
		}
		dest = afc::printNumber<10>(track.getDurationMillis(), dest);
		dest = afc::copy(R"(,"unit":"ms"}}})"_s, dest);

		return dest;
	}

	inline bool parseDateTime(const Value &dateTimeObject, afc::TimestampTZ &dest) noexcept
	{
		return likely(isType(dateTimeObject, stringValue)) &&
				likely(parseISODateTime(dateTimeObject.asCString(), dest));
	}

	inline bool parseDuration(const Value &durationObject, long &dest)
	{
		if (unlikely(!isType(durationObject, objectValue))) {
			return false;
		}
		const Value &amount = getField(durationObject, "amount");
		const Value &unit = getField(durationObject, "unit");
		if (unlikely(!isType(amount, intValue)) || unlikely(!isType(unit, stringValue))) {
			return false;
		}
		const long val = amount.asInt();
		const char * const unitValue = unit.asCString();
		if (unitValue[0] == 'm' && likely(unitValue[1] == 's' && unitValue[2] == '\0')) { // == "ms"
			dest = val;
			return true;
		} else if (unitValue[0] == 's' && likely(unitValue[1] == '\0')) { // == "s"
			dest = val * 1000;
			return true;
		}
		return false;
	}

	template<typename AddArtistOp>
	inline bool parseArtists(const Value &artists, const bool artistsRequired, AddArtistOp addArtistOp)
	{
		typedef decltype(Value::size()) artistSize_t;

		const ValueType valueType = artists.type();
		if (unlikely(valueType != nullValue && valueType != arrayValue)) {
			return false;
		}
		const artistSize_t artistCount = artists.size();
		if (artistCount == 0) {
			return !artistsRequired;
		}

		artistSize_t i = 0;
		do {
			const Value &artist = artists[i];
			if (unlikely(!isType(artist, objectValue))) {
				return false;
			}
			const Value &artistName = getField(artist, "name");
			if (unlikely(!isType(artistName, stringValue))) {
				return false;
			}
			addArtistOp(artistName.asCString());
		} while (++i < artistCount);

		return true;
	}
}

bool ScrobbleInfo::parse(const char * const begin, const char * const end, ScrobbleInfo &dest)
{
	Json::Reader jsonReader;
	Value object;

	if (unlikely(!jsonReader.parse(begin, end, object, false))) {
		afc::logger::logError("[Scrobbler] Unable to parse the scrobble JSON object: '#'.",
				jsonReader.getFormatedErrorMessages().c_str());
		return false;
	}
	if (unlikely(!isType(object, objectValue))) {
		return false;
	}
	if (unlikely(!parseDateTime(getField(object, "scrobble_start_datetime"), dest.scrobbleStartTimestamp)) ||
			unlikely(!parseDateTime(getField(object, "scrobble_end_datetime"), dest.scrobbleEndTimestamp)) ||
			unlikely(!parseDuration(getField(object, "scrobble_duration"), dest.scrobbleDuration))) {
		return false;
	}

	Track &track = dest.track;

	const Value &trackObject = getField(object, "track");
	if (unlikely(!isType(trackObject, objectValue))) {
		return false;
	}
	const Value &trackTitle = getField(trackObject, "title");
	if (unlikely(!isType(trackTitle, stringValue))) {
		return false;
	}
	track.setTitle(trackTitle.asCString());

	const Value &trackAlbum = getField(trackObject, "album");
	const ValueType trackAlbumObjType = trackAlbum.type();
	if (trackAlbumObjType != nullValue) {
		if (unlikely(trackAlbumObjType != objectValue)) {
			return false;
		}
		const Value &trackAlbumTitle = getField(trackAlbum, "title");
		if (unlikely(!isType(trackAlbumTitle, stringValue))) {
			return false;
		}
		track.setAlbumTitle(trackAlbumTitle.asCString());

		if (unlikely(!parseArtists(getField(trackAlbum, "artists"), false,
				[&](const char * const artistName) { track.addAlbumArtist(artistName); }))) {
			return false;
		}
	}

	long trackDuration;
	if (unlikely(!parseDuration(getField(trackObject, "length"), trackDuration))) {
		return false;
	}
	track.setDurationMillis(trackDuration);

	if (unlikely(!parseArtists(getField(trackObject, "artists"), true,
			[&](const char * const artistName) { track.addArtist(artistName); }))) {
		return false;
	}

	return true;
}

afc::FastStringBuffer<char, afc::AllocMode::accurate> serialiseAsJson(const ScrobbleInfo &scrobbleInfo)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	asm("nop");
	afc::FastStringBuffer<char, afc::AllocMode::accurate> buf(maxSize);
	asm("nop");

	buf.returnTail(appendAsJsonImpl(scrobbleInfo, buf.borrowTail()));

	return buf;
}

void appendAsJson(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &dest)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	dest.reserve(dest.size() + maxSize);

	dest.returnTail(appendAsJsonImpl(scrobbleInfo, dest.borrowTail()));
}
