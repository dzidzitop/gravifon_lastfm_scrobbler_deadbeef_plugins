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
#ifndef URLBUILDERTEST_HPP_
#define URLBUILDERTEST_HPP_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class UrlBuilderTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(UrlBuilderTest);
	CPPUNIT_TEST(testUrlWithNoQuery);

	CPPUNIT_TEST(testUrlWithQuery_SingleParam_NoEscaping);
	CPPUNIT_TEST(testUrlWithQuery_SingleParam_NameEscaped);
	CPPUNIT_TEST(testUrlWithQuery_SingleParam_ValueEscaped);

	CPPUNIT_TEST(testUrlWithQuery_MultipleParams_RepeatedNames);
	CPPUNIT_TEST_SUITE_END();
public:
	void testUrlWithNoQuery();

	void testUrlWithQuery_SingleParam_NoEscaping();
	void testUrlWithQuery_SingleParam_NameEscaped();
	void testUrlWithQuery_SingleParam_ValueEscaped();

	void testUrlWithQuery_MultipleParams_RepeatedNames();
};

#endif /* URLBUILDERTEST_HPP_ */
