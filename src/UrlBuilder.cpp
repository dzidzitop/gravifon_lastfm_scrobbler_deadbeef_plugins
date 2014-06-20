/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2014 Dźmitry Laŭčuk

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
#include "UrlBuilder.hpp"
#include <afc/ensure_ascii.hpp>
#include <afc/utils.h>
#include <cstddef>

using namespace std;
using namespace afc;

namespace
{
	constexpr inline char toAscii(const char value)
	{
		return value; // ASCII-compatible basic encodings are supported for now only.
	}
}

void UrlBuilder::appendUrlEncoded(const char * const str)
{
	char c;
	size_t i = 0;
	while ((c = str[i++]) != '\0') {
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
				c == '-' || c == '_' || c == '.' || c == '~') {
			// An unreserved character. No escaping is needed.
			m_buf += c;
		} else {
			/* A non-unreserved character. Escaping it to percent-encoded representation.
			   The reserved characters are escaped, too, for simplicity. */

			/* ASCII representation is escaped. Converting the character to ASCII.
			 *
			 * Casting to unsigned since bitwise operators are defined well for them
			 * in terms of values.
			 */
			const unsigned char ac = toAscii(c);

			// 0xff is applied just in case non-octet bytes are used.
			const char high = toHex((ac & 0xff) >> 4);
			const char low = toHex(ac & 0xf);
			m_buf.append({'%', high, low}); // The percent-encoded ASCII character.
		}
	}
}
