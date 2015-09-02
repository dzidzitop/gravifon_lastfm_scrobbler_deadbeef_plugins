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
#include "ScrobbleInfo.hpp"
#include <algorithm>
#include <bitset>
#include <cstddef>
#include <utility>
#include <afc/builtin.hpp>
#include <afc/dateutil.hpp>
#include <afc/ensure_ascii.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/json.hpp>
#include <afc/logger.hpp>
#include <afc/number.h>
#include <afc/SimpleString.hpp>
#include <afc/StringRef.hpp>
#include <afc/utils.h>

using afc::operator"" _s;

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
	Iterator writeJsonString(const char * const srcBegin, const char * const srcEnd, register Iterator dest)
	{
		for (auto p = srcBegin; p < srcEnd; ++p) {
			const char c = *p;
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

	template<typename Iterator>
	inline Iterator writeJsonString(const afc::String &src, register Iterator dest)
	{
		return writeJsonString(src.begin(), src.end(), dest);
	}

	inline std::size_t artistCount(const afc::String &artists)
	{
		return std::count_if(artists.begin(), artists.end(),
				[](const char c) -> bool { return c == Track::multiTagSeparator(); }) + 1;
	}

	template<typename Iterator>
	inline Iterator writeArtists(const afc::String &artists, register Iterator dest)
	{
		// Artists are '\n'-separated values within a single string.
		const char *start = artists.begin(), *end = artists.end();
		for (const char *p = start; p != end; ++p) {
			if (*p == Track::multiTagSeparator()) {
				dest = afc::copy(R"({"name":")"_s, dest);
				dest = writeJsonString(start, p, dest);
				dest = afc::copy(R"("},)"_s, dest);
				start = p + 1;
			}
		}
		// Adding the last tag.
		dest = afc::copy(R"({"name":")"_s, dest);
		dest = writeJsonString(start, artists.end(), dest);
		dest = afc::copy(R"("})"_s, dest);
		return dest;
	}

	inline std::size_t maxJsonSize(const ScrobbleInfo &scrobbleInfo) noexcept
	{
		const Track &track = scrobbleInfo.track;

		assert(track.hasTitle());
		assert(track.getArtists().size() > 0);

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
			const afc::String &artists = track.getArtists();
			const std::size_t count= artistCount(artists);
			maxSize += R"("artists":[])"_s.size();
			maxSize += R"({"name":""},)"_s.size() * count - 1; // With commas; the last comma is removed.
			maxSize += maxPrintedCharSize * (artists.size() - count);
			maxSize += 1; // ,
		}

		if (track.hasAlbumTitle()) {
			maxSize += R"("album":{"title":""})"_s.size();
			maxSize += maxPrintedCharSize * track.getAlbumTitle().size();
			if (track.getAlbumArtists().size() > 0) {
				const afc::String &albumArtists = track.getAlbumArtists();
				const std::size_t count= artistCount(albumArtists);
				maxSize += 1; // ,
				maxSize += R"("artists":[])"_s.size();
				maxSize += R"({"name":""},)"_s.size() * count - 1; // With commas; the last comma is removed.
				maxSize += maxPrintedCharSize * (albumArtists.size() - count);
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
		dest = writeArtists(track.getArtists(), dest);
		if (track.hasAlbumTitle()) {
			dest = afc::copy(R"(],"album":{"title":")"_s, dest);
			dest = writeJsonString(track.getAlbumTitle(), dest);
			if (track.getAlbumArtists().size() > 0) {
				dest = afc::copy(R"(","artists":[)"_s, dest);
				dest = writeArtists(track.getAlbumArtists(), dest);
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

	template<typename ErrorHandler>
	inline const char *parseText(const char * const begin, const char * const end, afc::FastStringBuffer<char> &dest, ErrorHandler &errorHandler)
	{
		auto textParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			return afc::json::parseCharsToUTF8(begin, end, [&](const char c) { dest.reserveForOne(); dest.append(c); }, errorHandler);
		};

		return afc::json::parseString<const char *, decltype(textParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, textParser, errorHandler);
	}

	template<typename ErrorHandler>
	inline const char *parseDateTime(const char * const begin, const char * const end, afc::TimestampTZ &dest, ErrorHandler &errorHandler) noexcept
	{
		auto timestampParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char * const valueEnd = std::find(begin, end, u8"\""[0]);
			if (valueEnd == end) {
				errorHandler.prematureEnd();
				return end;
			}
			if (likely(parseISODateTime(afc::String(begin, valueEnd).c_str(), dest))) {
				return valueEnd;
			} else {
				errorHandler.malformedJson(begin);
				return end;
			}
		};

		return afc::json::parseString<const char *, decltype(timestampParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, timestampParser, errorHandler);
	}

	// TOOD defined noexcept
	template<typename ErrorHandler>
	inline const char *parseDuration(const char * const begin, const char * const end, long &dest, ErrorHandler &errorHandler)
	{
		unsigned multiplier = 0;
		auto durationParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char *p = begin;
			for (std::size_t fieldsToParse = 1 | (1 << 1);;) { // All fields are required.
				// TODO optimise property name matching.
				const char *propNameBegin;
				std::size_t propNameSize;

				auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
				{
					propNameBegin = begin;
					const char * const propNameEnd = std::find(begin, end, u8"\""[0]);
					propNameSize = propNameEnd - propNameBegin;
					return propNameEnd;
				};

				p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
						(p, end, propNameParser, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				if (afc::equal("amount", "amount"_s.size(), propNameBegin, propNameSize)) {
					fieldsToParse &= ~std::size_t(1);

					auto durationAmountParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler)
					{
						const char * const p = afc::parseNumber<10, afc::ParseMode::scan>(
								begin, end, dest, [&](const char * const pos) { errorHandler.malformedJson(pos); });
						if (unlikely(!errorHandler.valid())) {
							return end;
						}
						return p;
					};

					p = afc::json::parseNumber<const char *, decltype(durationAmountParser) &, ErrorHandler, afc::json::noSpaces>
							(p, end, durationAmountParser, errorHandler);
					if (!errorHandler.valid()) {
						return end;
					}
				} else if (afc::equal("unit", "unit"_s.size(), propNameBegin, propNameSize)) {
					fieldsToParse &= ~std::size_t(1 << 1);

					auto unitValueParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
					{
						const char * const unitEnd = std::find(begin, end, u8"\""[0]);
						if (unitEnd == end) {
							return end;
						}
						switch (unitEnd - begin) {
						case 1:
							if (likely(begin[0] == u8"s"[0])) {
								multiplier = 1000;
								return unitEnd;
							} else {
								errorHandler.malformedJson(begin);
								return end;
							}
						case 2:
							if (unlikely(begin[0] != u8"m"[0] || begin[1] != u8"s"[0])) {
								errorHandler.malformedJson(begin);
								return end;
							}
							multiplier = 1;
							return unitEnd;
						default:
							errorHandler.malformedJson(begin);
							return end;
						}
					};

					p = afc::json::parseString<const char *, decltype(unitValueParser) &, ErrorHandler, afc::json::noSpaces>
							(p, end, unitValueParser, errorHandler);
					if (!errorHandler.valid()) {
						return end;
					}
				} else {
					errorHandler.malformedJson(p);
					return end;
				}

				if (fieldsToParse == 0) {
					return p;
				}

				if (unlikely(p == end)) {
					errorHandler.prematureEnd();
					return end;
				}
				const char c = *p;
				if (likely(c == u8","[0])) {
					++p;
				} else {
					errorHandler.malformedJson(p);
					return end;
				}
			}
		};

		return afc::json::parseObject<const char *, decltype(durationParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, durationParser, errorHandler);
	}

	template<typename ErrorHandler>
	inline const char *parseArtists(const char * const begin, const char * const end, afc::FastStringBuffer<char> &dest, ErrorHandler &errorHandler)
	{
		auto artistElementParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			auto artistParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler)
			{
				auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
				{
					if (unlikely(end - begin < 4 || begin[0] != u8"n"[0] || begin[1] != u8"a"[0] || begin[2] != u8"m"[0] || begin[3] != u8"e"[0])) {
						errorHandler.malformedJson(begin);
						return end;
					} else {
						return begin + 4;
					}
				};

				const char *p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
						(begin, end, propNameParser, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = parseText(p, end, dest, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				dest.reserveForOne();
				dest.append(Track::multiTagSeparator());

				return p;
			};

			return afc::json::parseObject<const char *, decltype(artistParser) &, ErrorHandler, afc::json::noSpaces>
					(begin, end, artistParser, errorHandler);
		};

		const char * const p = afc::json::parseArray<const char *, decltype(artistElementParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, artistElementParser, errorHandler);
		if (dest.size() > 0) { // removing extra separator
			dest.resize(dest.size() - 1);
		}
		return p;
	}

	template<typename ErrorHandler>
	inline const char *parseAlbum(const char * const begin, const char * const end, Track &dest, ErrorHandler &errorHandler)
	{
		auto albumParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char *p = begin;
			for (std::size_t fieldsToParse = 1;;) {
				// TODO optimise property name matching.
				const char *propNameBegin;
				std::size_t propNameSize;

				auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
				{
					propNameBegin = begin;
					const char * const propNameEnd = std::find(begin, end, u8"\""[0]);
					propNameSize = propNameEnd - propNameBegin;
					return propNameEnd;
				};

				p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
						(p, end, propNameParser, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				if (afc::equal("title", "title"_s.size(), propNameBegin, propNameSize)) {
					fieldsToParse &= ~std::size_t(1);

					afc::FastStringBuffer<char> buf(15); // Reasonable album title buffer size to keep balance between re-allocs and memory usage overhead.
					p = parseText(p, end, buf, errorHandler);

					if (unlikely(!errorHandler.valid())) {
						return end;
					}

					dest.setAlbumTitle(afc::String::move(buf));
				} else if (afc::equal("artists", "artists"_s.size(), propNameBegin, propNameSize)) {
					afc::FastStringBuffer<char> buf(15); // Reasonable artist name buffer size to keep balance between re-allocs and memory usage overhead.
					p = parseArtists(p, end, buf, errorHandler);

					if (unlikely(!errorHandler.valid())) {
						return end;
					}

					dest.setAlbumArtists(afc::String::move(buf));
				} else {
					errorHandler.malformedJson(p);
					return end;
				}

				if (p == end) {
					errorHandler.prematureEnd();
					return end;
				}
				const char c = *p;
				if (c == u8","[0]) {
					++p;
				} else if (c == u8"}"[0]) {
					if (unlikely(fieldsToParse != 0)) {
						errorHandler.malformedJson(p);
						return end;
					}

					return p;
				} else {
					errorHandler.malformedJson(p);
					return end;
				}
			}
		};

		return afc::json::parseObject<const char *, decltype(albumParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, albumParser, errorHandler);
	}

	template<typename ErrorHandler>
	inline const char *parseTrack(const char * const begin, const char * const end, Track &dest, ErrorHandler &errorHandler)
	{
		auto trackParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
		{
			const char *p = begin;
			for (std::size_t fieldsToParse = 1 | (1 << 1) | (1 << 2);;) {
				// TODO optimise property name matching.
				const char *propNameBegin;
				std::size_t propNameSize;

				auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
				{
					propNameBegin = begin;
					const char * const propNameEnd = std::find(begin, end, u8"\""[0]);
					propNameSize = propNameEnd - propNameBegin;
					return propNameEnd;
				};

				p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
						(p, end, propNameParser, errorHandler);

				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}

				if (afc::equal("title", "title"_s.size(), propNameBegin, propNameSize)) {
					fieldsToParse &= ~std::size_t(1);

					afc::FastStringBuffer<char> buf(15); // Reasonable track title buffer size to keep balance between re-allocs and memory usage overhead.
					p = parseText(p, end, buf, errorHandler);

					if (unlikely(!errorHandler.valid())) {
						return end;
					}

					dest.setTitle(afc::String::move(buf));
				} else if (afc::equal("artists", "artists"_s.size(), propNameBegin, propNameSize)) {
					fieldsToParse &= ~std::size_t(1 << 1);

					afc::FastStringBuffer<char> buf(15); // Reasonable artist name buffer size to keep balance between re-allocs and memory usage overhead.
					p = parseArtists(p, end, buf, errorHandler);

					if (unlikely(!errorHandler.valid())) {
						return end;
					}

					dest.setArtists(afc::String::move(buf));
				} else if (afc::equal("length", "length"_s.size(), propNameBegin, propNameSize)) {
					fieldsToParse &= ~std::size_t(1 << 2);

					long trackDuration;
					p = parseDuration(p, end, trackDuration, errorHandler);
					if (unlikely(!errorHandler.valid())) {
						return end;
					}

					dest.setDurationMillis(trackDuration);
				} else if (afc::equal("album", "album"_s.size(), propNameBegin, propNameSize)) {
					p = parseAlbum(p, end, dest, errorHandler);
					if (unlikely(!errorHandler.valid())) {
						return end;
					}
				} else {
					errorHandler.malformedJson(p);
					return end;
				}

				if (p == end) {
					errorHandler.prematureEnd();
					return end;
				}
				const char c = *p;
				if (c == u8","[0]) {
					++p;
				} else if (c == u8"}"[0]) {
					if (unlikely(fieldsToParse != 0)) {
						errorHandler.malformedJson(p);
						return end;
					}

					return p;
				} else {
					errorHandler.malformedJson(p);
					return end;
				}
			}
		};

		return afc::json::parseObject<const char *, decltype(trackParser) &, ErrorHandler, afc::json::noSpaces>
				(begin, end, trackParser, errorHandler);
	}
}

bool ScrobbleInfo::parse(const char * const begin, const char * const end, ScrobbleInfo &dest)
{
	class ErrorHandler
	{
	public:
		ErrorHandler(void) : m_valid(true) {}

		void malformedJson(const char *pos)
		{
			// TODO handle error;
			m_valid = false;
		}

		void prematureEnd()
		{
			// TODO handle error;
			m_valid = false;
		}

		bool valid() { return m_valid; }
	private:
		bool m_valid;
	} errorHandler;

	auto scrobbleParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
	{
		const char *p = begin;
		for (std::size_t fieldsToParse = 1 | (1 << 1) | (1 << 2) | (1 << 3);;) { // All fields are required.
			// TODO optimise property name matching.
			const char *propNameBegin;
			std::size_t propNameSize;

			auto propNameParser = [&](const char * const begin, const char * const end, ErrorHandler &errorHandler) -> const char *
			{
				propNameBegin = begin;
				const char * const propNameEnd = std::find(begin, end, u8"\""[0]);
				propNameSize = propNameEnd - propNameBegin;
				return propNameEnd;
			};

			p = afc::json::parseString<const char *, decltype(propNameParser) &, ErrorHandler, afc::json::noSpaces>
					(p, end, propNameParser, errorHandler);

			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			p = afc::json::parseColon<const char *, ErrorHandler, afc::json::noSpaces>(p, end, errorHandler);
			if (unlikely(!errorHandler.valid())) {
				return end;
			}

			if (afc::equal("scrobble_start_datetime", "scrobble_start_datetime"_s.size(), propNameBegin, propNameSize)) {
				fieldsToParse &= ~std::size_t(1);

				p = parseDateTime(p, end, dest.scrobbleStartTimestamp, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}
			} else if (afc::equal("scrobble_end_datetime", "scrobble_end_datetime"_s.size(), propNameBegin, propNameSize)) {
				fieldsToParse &= ~std::size_t(1 << 1);

				p = parseDateTime(p, end, dest.scrobbleEndTimestamp, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}
			} else if (afc::equal("scrobble_duration", "scrobble_duration"_s.size(), propNameBegin, propNameSize)) {
				fieldsToParse &= ~std::size_t(1 << 2);

				p = parseDuration<ErrorHandler &>(p, end, dest.scrobbleDuration, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}
			} else if (afc::equal("track", "track"_s.size(), propNameBegin, propNameSize)) {
				fieldsToParse &= ~std::size_t(1 << 3);

				p = parseTrack<ErrorHandler &>(p, end, dest.track, errorHandler);
				if (unlikely(!errorHandler.valid())) {
					return end;
				}
			}

			if (fieldsToParse == 0) {
				return p;
			}

			if (unlikely(p == end)) {
				errorHandler.prematureEnd();
				return end;
			}
			const char c = *p;
			if (likely(c == u8","[0])) {
				++p;
			} else {
				errorHandler.malformedJson(p);
				return end;
			}
		}
	};

	const char * const p = afc::json::parseObject<const char *, decltype(scrobbleParser) &, ErrorHandler, afc::json::noSpaces>
			(begin, end, scrobbleParser, errorHandler);

	if (unlikely(!errorHandler.valid() || p != end)) {
		// TODO handle error.
		return false;
	}

	return true;
}

afc::FastStringBuffer<char, afc::AllocMode::accurate> serialiseAsJson(const ScrobbleInfo &scrobbleInfo)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	afc::FastStringBuffer<char, afc::AllocMode::accurate> buf(maxSize);

	buf.returnTail(appendAsJsonImpl(scrobbleInfo, buf.borrowTail()));

	return buf;
}

void appendAsJson(const ScrobbleInfo &scrobbleInfo, afc::FastStringBuffer<char> &dest)
{
	const std::size_t maxSize = maxJsonSize(scrobbleInfo);
	dest.reserve(dest.size() + maxSize);

	dest.returnTail(appendAsJsonImpl(scrobbleInfo, dest.borrowTail()));
}
