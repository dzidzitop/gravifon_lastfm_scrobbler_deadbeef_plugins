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
#ifndef DEADBEEFUTILTEST_HPP_
#define DEADBEEFUTILTEST_HPP_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class DeadbeefUtilTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(DeadbeefUtilTest);
	CPPUNIT_TEST(testConvertMultiTag_EmptyString);
	CPPUNIT_TEST(testConvertMultiTag_SingleValue);
	CPPUNIT_TEST(testConvertMultiTag_TwoValues);
	CPPUNIT_TEST(testConvertMultiTag_ThreeValues);
	CPPUNIT_TEST(testConvertMultiTag_TwoValues_NonEmptyBuffer);
	CPPUNIT_TEST_SUITE_END();
public:
	void testConvertMultiTag_EmptyString();
	void testConvertMultiTag_SingleValue();
	void testConvertMultiTag_TwoValues();
	void testConvertMultiTag_ThreeValues();
	void testConvertMultiTag_TwoValues_NonEmptyBuffer();
};

#endif /* DEADBEEFUTILTEST_HPP_ */
