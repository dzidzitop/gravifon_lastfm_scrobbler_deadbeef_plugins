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
#include <afc/utils.h>

using namespace afc;

void UrlBuilder::appendUrlEncoded(const char c, std::string &dest)
{
	if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
			c == '-' || c == '_' || c == '.' || c == '~') {
		// An unreserved character. No escaping is needed.
		dest += c;
	} else {
		/* A non-unreserved character. Escaping it to percent-encoded representation.
		 * The reserved characters are escaped, too, for simplicity. */

		/* Casting to unsigned since bitwise operators are defined well for them
		 * in terms of values.
		 */
		const unsigned char ac = c;

		// 0xff is applied just in case non-octet bytes are used.
		const char high = toHex((ac & 0xff) >> 4);
		const char low = toHex(ac & 0xf);
		dest.append({'%', high, low});
	}
}
