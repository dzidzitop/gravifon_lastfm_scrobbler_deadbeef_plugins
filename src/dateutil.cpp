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
#include <chrono>
#include <time.h>
#include <afc/utils.h>

using namespace std;
using namespace afc;
using std::chrono::system_clock;

bool parseISODateTime(const string &str, time_t &dest)
{
	tm dateTime;
	const char * const parseResult =
			strptime(convertToUtf8(str, systemCharset().c_str()).c_str(), "%FT%T%z", &dateTime);

	if (parseResult == nullptr || *parseResult != 0) {
		return false;
	}

	dest = timegm(&dateTime);
	return true;
}
