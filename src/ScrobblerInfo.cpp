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
#include <utility>
#include <time.h>
#include <jsoncpp/json/value.h>
#include <jsoncpp/json/reader.h>
#include <afc/ensure_ascii.hpp>
#include <afc/utils.h>
#include "jsonutil.hpp"

using namespace std;
using namespace afc;
using Json::Value;
using Json::ValueType;

namespace
{
	inline void writeJsonString(const string &src, string &dest)
	{
		for (const char c : src) {
			switch (c) {
			case '"':
			case '\\':
				dest.push_back('\\');
				dest.push_back(c);
				break;
			case '\b':
				dest.append(u8"\\b");
				break;
			case '\f':
				dest.append(u8"\\f");
				break;
			case '\n':
				dest.append(u8"\\n");
				break;
			case '\r':
				dest.append(u8"\\r");
				break;
			case '\t':
				dest.append(u8"\\t");
				break;
			default:
				dest.push_back(c);
				break;
			}
		}
	}

	inline void writeJsonTimestamp(const afc::TimestampTZ &timestamp, string &dest)
	{
		/* The datetime format as required by https://github.com/gravidence/gravifon/wiki/Date-Time
		 * Milliseconds are not supported.
		 */
		::tm dateTime = static_cast< ::tm >(timestamp);

		/* Twenty five characters (including the terminating character) are really used
		 * to store an ISO-8601-formatted date for a single-byte encoding. For multi-byte
		 * encodings this is not the case, and a larger buffer could be needed.
		 */
		for (size_t outputSize = 25;; outputSize *= 2) {
			char buf[outputSize];
			if (::strftime(buf, outputSize, "%Y-%m-%dT%H:%M:%S%z", &dateTime) != 0) {
				// The date is formatted successfully.
				dest.append(u8"\"").append(convertToUtf8(buf, systemCharset().c_str())).append(u8"\"");
				return;
			}
			// The size of the destination buffer is too small. Repeating with a larger buffer.
		}
	}

	// writes value to out in utf-8
	inline void writeJsonLong(const long value, string &dest)
	{
		dest.append(convertToUtf8(to_string(value), systemCharset().c_str()));
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

bool ScrobbleInfo::parse(const string &str, ScrobbleInfo &dest)
{
	Json::Reader jsonReader;
	Value object;
	if (!jsonReader.parse(str, object, false)) {
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

void ScrobbleInfo::appendAsJsonTo(string &str) const
{
	str.append(u8R"({"scrobble_start_datetime":)");
	writeJsonTimestamp(scrobbleStartTimestamp, str);
	str.append(u8R"(,"scrobble_end_datetime":)");
	writeJsonTimestamp(scrobbleEndTimestamp, str);
	str.append(u8R"(,"scrobble_duration":{"amount":)");
	writeJsonLong(scrobbleDuration, str);
	str.append(u8R"(,"unit":"ms"},"track":)");
	track.appendAsJsonTo(str);
	str.append(u8"}");
}

void Track::appendAsJsonTo(string &str) const
{
	str.append(u8R"({"title":")");
	writeJsonString(m_title, str);
	// A single artist is currently supported.
	str.append(u8R"(","artists":[)");
	for (const string &artist : m_artists) {
		str.append(u8R"({"name":")");
		writeJsonString(artist, str);
		str.append(u8"\"},");
	}
	str.pop_back(); // removing the last redundant comma.
	str.append(u8"],");
	if (m_albumSet) {
		str.append(u8R"("album":{"title":")");
		writeJsonString(m_album, str);
		str.append(u8"\"");
		if (hasAlbumArtist()) {
			str.append(u8R"(,"artists":[)");
			for (const string &artist : m_albumArtists) {
				str.append(u8R"({"name":")");
				writeJsonString(artist, str);
				str.append(u8"\"},");
			}
			str.back() = u8"]"[0]; // removing the last redundant comma as well.
		}
		str.append(u8R"(},)");
	}
	str.append(u8R"("length":{"amount":)");
	writeJsonLong(m_duration, str);
	str.append(u8R"(,"unit":"ms"}})");
}
