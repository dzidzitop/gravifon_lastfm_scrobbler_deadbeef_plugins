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

	CPPUNIT_TEST(testUrlWithQuery_FreeFormatNoEscaping);
	CPPUNIT_TEST(testUrlWithQuery_FreeFormatEscaped);
	CPPUNIT_TEST(testUrlWithQuery_FreeFormatRaw);
	CPPUNIT_TEST(testUrlWithQuery_FreeFormatCustom);

	CPPUNIT_TEST(testUrlWithQuery_ConstructoBuilding_FreeFormatNoEscaping);
	CPPUNIT_TEST(testUrlWithQuery_ConstructoBuilding_FreeFormatEscaped);
	CPPUNIT_TEST(testUrlWithQuery_ConstructoBuilding_FreeFormatRaw);
	CPPUNIT_TEST(testUrlWithQuery_ConstructoBuilding_FreeFormatCustom);

	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_NoParams);
	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_SingleParam_NoEscaping);
	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_SingleParam_ParamNameEscaped);
	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_SingleParam_ParamValueEscaped);
	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_SingleParam_RawParamName);
	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_SingleParam_RawParamValue);
	CPPUNIT_TEST(testQueryOnly_ConstructorBuilding_MultipleParams);

	CPPUNIT_TEST(testCapacityComputation_QueryOnly_OrdinaryParams_NoEscaping);
	CPPUNIT_TEST(testCapacityComputation_QueryOnly_OrdinaryParams_AllEscaped);
	CPPUNIT_TEST(testCapacityComputation_QueryOnly_RawParams);
	CPPUNIT_TEST(testCapacityComputation_UrlWithQuery);
	CPPUNIT_TEST(testCapacityComputation_ParamsAppended);

	CPPUNIT_TEST_SUITE_END();
public:
	void testUrlWithNoQuery();

	void testUrlWithQuery_SingleParam_NoEscaping();
	void testUrlWithQuery_SingleParam_NameEscaped();
	void testUrlWithQuery_SingleParam_ValueEscaped();

	void testUrlWithQuery_MultipleParams_RepeatedNames();

	void testUrlWithQuery_FreeFormatNoEscaping();
	void testUrlWithQuery_FreeFormatEscaped();
	void testUrlWithQuery_FreeFormatRaw();
	void testUrlWithQuery_FreeFormatCustom();

	void testUrlWithQuery_ConstructoBuilding_FreeFormatNoEscaping();
	void testUrlWithQuery_ConstructoBuilding_FreeFormatEscaped();
	void testUrlWithQuery_ConstructoBuilding_FreeFormatRaw();
	void testUrlWithQuery_ConstructoBuilding_FreeFormatCustom();

	void testQueryOnly_ConstructorBuilding_NoParams();
	void testQueryOnly_ConstructorBuilding_SingleParam_NoEscaping();
	void testQueryOnly_ConstructorBuilding_SingleParam_ParamNameEscaped();
	void testQueryOnly_ConstructorBuilding_SingleParam_ParamValueEscaped();
	void testQueryOnly_ConstructorBuilding_SingleParam_RawParamName();
	void testQueryOnly_ConstructorBuilding_SingleParam_RawParamValue();
	void testQueryOnly_ConstructorBuilding_MultipleParams();

	void testCapacityComputation_QueryOnly_OrdinaryParams_NoEscaping();
	void testCapacityComputation_QueryOnly_OrdinaryParams_AllEscaped();
	void testCapacityComputation_QueryOnly_RawParams();
	void testCapacityComputation_UrlWithQuery();
	void testCapacityComputation_ParamsAppended();
};

#endif /* URLBUILDERTEST_HPP_ */
