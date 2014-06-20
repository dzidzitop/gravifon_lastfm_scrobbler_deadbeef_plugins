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

using namespace std;

void UrlBuilderTest::testUrlWithNoQuery()
{
	UrlBuilder builder("http://hello/world");

	const string result(builder.toString());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world"), result);
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_NoEscaping()
{
	UrlBuilder builder("http://hello/world");
	builder.param("foo", "bar");

	const string result(builder.toString());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=bar"), result);
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_NameEscaped()
{
	UrlBuilder builder("http://hello/world");
	builder.param("fo o", "bar");

	const string result(builder.toString());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?fo%20o=bar"), result);
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_ValueEscaped()
{
	UrlBuilder builder("http://hello/world");
	builder.param("foo", "ba+r");

	const string result(builder.toString());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=ba%2br"), result);
}

void UrlBuilderTest::testUrlWithQuery_MultipleParams_RepeatedNames()
{
	UrlBuilder builder("http://hello/world");
	builder.param("foo", "ba+r").param("ba&^z", "==quu_x").param("foo", "123");

	const string result(builder.toString());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=ba%2br&ba%26%5ez=%3d%3dquu_x&foo=123"), result);
}
