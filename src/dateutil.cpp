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
	tm dateTime;
	// The input string is converted to the system default encoding to be interpreted correctly.
	const char * const parseResult =
			strptime(convertFromUtf8(str, systemCharset().c_str()).c_str(), "%FT%T%z", &dateTime);

	if (parseResult == nullptr || *parseResult != 0) {
		return false;
	}

	// Unfortunately, this code is not portable. It compiles in Debian Wheezy with GCC 4.7.
	dateTime.tm_sec -= dateTime.tm_gmtoff;
	dateTime.tm_gmtoff = 0;

	dest = mktime(&dateTime) - timezone;
	return true;
}
