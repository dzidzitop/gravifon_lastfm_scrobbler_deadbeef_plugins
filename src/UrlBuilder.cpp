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

namespace
{
	inline void appendUrlEncoded(const char c, FastStringBuffer<char> &dest)
	{
		/* Casting to unsigned since bitwise operators are defined well for them
		 * in terms of values.
		 *
		 * Only the lowest octet matters for URL encoding, even if unsigned char is larger.
		 */
		const unsigned char uc = static_cast<unsigned char>(c) & 0xff;

		if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9') ||
				uc == '-' || uc == '_' || uc == '.' || uc == '~') {
			// An unreserved character. No escaping is needed.
			dest += c;
		} else {
			/* A non-unreserved character. Escaping it to percent-encoded representation.
			 * The reserved characters are escaped, too, for simplicity. */
			const char high = toHex((uc) >> 4);
			const char low = toHex(uc & 0xf);
			dest.append({'%', high, low});
		}
	}
}

void UrlBuilder::appendUrlEncoded(const char *str, const std::size_t n)
{
	for (std::size_t i = 0; i < n; ++i) {
		::appendUrlEncoded(str[i], m_buf);
	}
}
