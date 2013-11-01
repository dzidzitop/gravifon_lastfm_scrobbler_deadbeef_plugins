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
#include <time.h>
#include <afc/utils.h>

using namespace std;
using namespace afc;

bool parseISODateTime(const string &str, time_t &dest)
{
	// Initialises the system time zone data.
	tzset();

	tm dateTime;
	// The input string is converted to the system default encoding to be interpreted correctly.
	const char * const parseResult =
			strptime(convertFromUtf8(str, systemCharset().c_str()).c_str(), "%FT%T%z", &dateTime);

	if (parseResult == nullptr || *parseResult != 0) {
		return false;
	}

	/* 1. Flush time zone offset into seconds to make a UTC time.
	 * 2. Make a local value out the tm structure with the UTC time. It is shifted towards future by
	 *    amount of negated ::timezone.
	 * 3. Make a time_t value out of the tm structure with the local time.
	 *
	 * Unfortunately, this code is not portable. It compiles in Debian Wheezy with GCC 4.7.
	 * In addition, this code cannot tolerate the system time zone changed in the middle of processing.
	 */
	dateTime.tm_sec -= dateTime.tm_gmtoff;
	dateTime.tm_sec -= timezone;
	dateTime.tm_gmtoff = -timezone;

	dest = mktime(&dateTime);
	return true;
}
