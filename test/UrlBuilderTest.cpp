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
#include "UrlBuilderTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(UrlBuilderTest);

#include <UrlBuilder.hpp>
#include <string>
#include <cstddef>

using namespace std;

void UrlBuilderTest::testUrlWithNoQuery()
{
	UrlBuilder builder("http://hello/world");

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_NoEscaping()
{
	UrlBuilder builder("http://hello/world");
	builder.param("foo", "bar");

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=bar"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_NameEscaped()
{
	UrlBuilder builder("http://hello/world");
	builder.param("fo o", "bar");

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?fo%20o=bar"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_ValueEscaped()
{
	UrlBuilder builder("http://hello/world");
	builder.param("foo", "ba+r");

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=ba%2br"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_MultipleParams_RepeatedNames()
{
	UrlBuilder builder("http://hello/world");
	builder.param("foo", "ba+r").param("ba&^z", "==quu_x").param("foo", "123");

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=ba%2br&ba%26%5ez=%3d%3dquu_x&foo=123"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_NoParams()
{
	UrlBuilder builder(queryOnly);

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string(), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(0), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_NoEscaping()
{
	UrlBuilder builder(queryOnly, UrlPart<>("hello"), UrlPart<>("world"));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("hello=world"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(11), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_ParamNameEscaped()
{
	UrlBuilder builder(queryOnly, UrlPart<>("he ll,o~"), UrlPart<>("world"));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("he%20ll%2co~=world"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(18), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_ParamValueEscaped()
{
	UrlBuilder builder(queryOnly, UrlPart<>("hello"), UrlPart<>("**w*rld  "));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("hello=%2a%2aw%2arld%20%20"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(25), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_RawParamName()
{
	UrlBuilder builder(queryOnly, UrlPart<raw>("he ll,o~"), UrlPart<ordinary>("**w*rld  "));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("he ll,o~=%2a%2aw%2arld%20%20"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(28), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_RawParamValue()
{
	UrlBuilder builder(queryOnly, UrlPart<>("he ll,o~"), UrlPart<raw>("**w*rld  "));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("he%20ll%2co~=**w*rld  "), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(22), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_MultipleParams()
{
	UrlBuilder builder(queryOnly,
			UrlPart<>("abc"), UrlPart<>("c e"),
			UrlPart<>("f*hijk"), UrlPart<>(""),
			UrlPart<raw>("l+no"), UrlPart<ordinary>("p%*rs"),
			UrlPart<raw>(""), UrlPart<>("tuvwxyz!"),
			UrlPart<raw>("abc"), UrlPart<>("1234"),
			UrlPart<ordinary>("abc"), UrlPart<>("www222"));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("abc=c%20e&f%2ahijk=&l+no=p%25%2ars&=tuvwxyz%21&abc=1234&abc=www222"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(66), builder.size());
}
