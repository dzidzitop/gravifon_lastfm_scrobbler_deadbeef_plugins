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
#include <afc/FastStringBuffer.hpp>

void DeadbeefUtilTest::testConvertMultiTag_EmptyString()
{
	afc::FastStringBuffer<char> result;
	convertMultiTag("", result);

	CPPUNIT_ASSERT_EQUAL(result.size(), std::size_t(0));
}

void DeadbeefUtilTest::testConvertMultiTag_SingleValue()
{
	afc::FastStringBuffer<char> result;
	convertMultiTag("Perry Como", result);

	CPPUNIT_ASSERT_EQUAL(std::string(result.c_str()), std::string("Perry Como"));
}

void DeadbeefUtilTest::testConvertMultiTag_TwoValues()
{
	afc::FastStringBuffer<char> result;
	convertMultiTag("Perry Como\nTom Jones", result);

	CPPUNIT_ASSERT_EQUAL(std::string(result.begin(), result.end()),
			(std::string("Perry Como") += u8"\0"[0]).append("Tom Jones"));
}

void DeadbeefUtilTest::testConvertMultiTag_ThreeValues()
{
	afc::FastStringBuffer<char> result;
	convertMultiTag("Perry Como\nTom Jones\nAndy Williams", result);

	CPPUNIT_ASSERT_EQUAL(std::string(result.begin(), result.end()),
			((std::string("Perry Como") += u8"\0"[0]).append("Tom Jones") += u8"\0"[0]).append("Andy Williams"));
}

void DeadbeefUtilTest::testConvertMultiTag_TwoValues_NonEmptyBuffer()
{
	afc::FastStringBuffer<char> result(15);
	result.append("Elton John", 10);

	convertMultiTag("Perry Como\nTom Jones", result);

	CPPUNIT_ASSERT_EQUAL(std::string(result.begin(), result.end()),
			(std::string("Elton JohnPerry Como") += u8"\0"[0]).append("Tom Jones"));
}
