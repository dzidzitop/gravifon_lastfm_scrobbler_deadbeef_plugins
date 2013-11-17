/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013 Dźmitry Laŭčuk

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
#include "dateutil.hpp"
#include <afc/utils.h>

using namespace std;
using namespace afc;

bool parseISODateTime(const string &str, DateTime &dest)
{
	tm dateTime;
	// The input string is converted to the system default encoding to be interpreted correctly.
	const char * const parseResult =
			strptime(convertFromUtf8(str, systemCharset().c_str()).c_str(), "%FT%T%z", &dateTime);
	/* strptime() does not assign the field tm_isdst. If it appears to be negative
	 * then strftime with %z discards the time zone as non-determined. To avoid
	 * this effect, assigning it explicitly.
	 */
	dateTime.tm_isdst = 0;

	if (parseResult == nullptr || *parseResult != 0) {
		return false;
	}

	dest = dateTime;

	return true;
}
