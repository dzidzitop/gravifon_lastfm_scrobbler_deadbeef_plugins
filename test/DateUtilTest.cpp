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
#include "DateUtilTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(DateUtilTest);

#include <dateutil.hpp>
#include <time.h>

using namespace std;

void DateUtilTest::testParseValidISODateTime_PositiveUTCTimeZone()
{
	string input("2013-10-16T20:02:26+0000");
	time_t dest;

	const bool result = parseISODateTime(input, dest);

	CPPUNIT_ASSERT(result);

	tm dateTime;
	gmtime_r(&dest, &dateTime);

	char buf[100];
	const size_t count = std::strftime(buf, 100, "%Y-%m-%dT%H:%M:%S%z", &dateTime);

	CPPUNIT_ASSERT(count != 0);
	CPPUNIT_ASSERT_EQUAL(string("2013-10-16T20:02:26+0000"), string(buf));
}
