/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2015 Dźmitry Laŭčuk

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
#include "DeadbeefUtilTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(DeadbeefUtilTest);

#include <cstddef>
#include <string>

#include <deadbeef_util.hpp>
#include <afc/SimpleString.hpp>

void DeadbeefUtilTest::testConvertMultiTag_EmptyString()
{
	afc::String result = convertMultiTag("");

	CPPUNIT_ASSERT_EQUAL(result.size(), std::size_t(0));
}

void DeadbeefUtilTest::testConvertMultiTag_SingleValue()
{
	afc::String result = convertMultiTag("Perry Como");

	CPPUNIT_ASSERT_EQUAL(std::string(result.c_str()), std::string("Perry Como"));
}

void DeadbeefUtilTest::testConvertMultiTag_TwoValues()
{
	afc::String result = convertMultiTag("Perry Como\nTom Jones");

	CPPUNIT_ASSERT_EQUAL(std::string(result.begin(), result.end()),
			(std::string("Perry Como") += u8"\0"[0]).append("Tom Jones"));
}

void DeadbeefUtilTest::testConvertMultiTag_ThreeValues()
{
	afc::String result = convertMultiTag("Perry Como\nTom Jones\nAndy Williams");

	CPPUNIT_ASSERT_EQUAL(std::string(result.begin(), result.end()),
			((std::string("Perry Como") += u8"\0"[0]).append("Tom Jones") += u8"\0"[0]).append("Andy Williams"));
}
