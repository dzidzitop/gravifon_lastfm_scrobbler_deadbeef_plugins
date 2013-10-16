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
#ifndef DATEUTILTEST_HPP_
#define DATEUTILTEST_HPP_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class DateUtilTest : public CppUnit::TestFixture
{
CPPUNIT_TEST_SUITE(DateUtilTest);
CPPUNIT_TEST(testParseValidISODateTime_PositiveUTCTimeZone);
CPPUNIT_TEST_SUITE_END();

	void testParseValidISODateTime_PositiveUTCTimeZone();
};

#endif /* DATEUTILTEST_HPP_ */
