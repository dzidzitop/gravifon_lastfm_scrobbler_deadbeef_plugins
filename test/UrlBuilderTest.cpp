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
#include <algorithm>
#include <cstddef>
#include <string>

using namespace std;

void UrlBuilderTest::testUrlWithNoQuery()
{
	UrlBuilder<webForm> builder("http://hello/world");

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_NoEscaping()
{
	UrlBuilder<webForm> builder("http://hello/world");
	builder.params(UrlPart<>("foo"), UrlPart<>("bar"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=bar"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_NameEscaped()
{
	UrlBuilder<webForm> builder("http://hello/world");
	builder.params(UrlPart<>("fo o"), UrlPart<>("bar"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?fo%20o=bar"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_SingleParam_ValueEscaped()
{
	UrlBuilder<webForm> builder("http://hello/world");
	builder.params(UrlPart<>("foo"), UrlPart<>("ba+r"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=ba%2br"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_MultipleParams_RepeatedNames()
{
	UrlBuilder<webForm> builder("http://hello/world");
	builder.params(UrlPart<>("foo"), UrlPart<>("ba+r"),
			UrlPart<>("ba&^z"), UrlPart<>("==quu_x"),
			UrlPart<>("foo"), UrlPart<>("123"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo=ba%2br&ba%26%5ez=%3d%3dquu_x&foo=123"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_FreeFormatNoEscaping()
{
	UrlBuilder<plain> builder("http://hello/world");
	builder.query(UrlPart<>("foo"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_FreeFormatEscaped()
{
	UrlBuilder<plain> builder("http://hello/world");
	builder.query(UrlPart<>("foo+++///"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo%2b%2b%2b%2f%2f%2f"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_FreeFormatRaw()
{
	UrlBuilder<plain> builder("http://hello/world");
	builder.query(UrlPart<raw>("foo+++///"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo+++///"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

struct CustomUrlPart
{
	constexpr std::size_t maxEncodedSize() const noexcept { return 9; }

	template<typename Iterator>
	Iterator appendTo(Iterator dest) const { return std::copy_n("foo+++///", 9, dest); }
};

void UrlBuilderTest::testUrlWithQuery_FreeFormatCustom()
{
	UrlBuilder<plain> builder("http://hello/world");
	builder.query(CustomUrlPart());

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo+++///"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_ConstructoBuilding_FreeFormatNoEscaping()
{
	UrlBuilder<plain> builder("http://hello/world", UrlPart<>("foo"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_ConstructoBuilding_FreeFormatEscaped()
{
	UrlBuilder<plain> builder("http://hello/world", UrlPart<>("foo+++///"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo%2b%2b%2b%2f%2f%2f"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_ConstructoBuilding_FreeFormatRaw()
{
	UrlBuilder<plain> builder("http://hello/world", UrlPart<raw>("foo+++///"));

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo+++///"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testUrlWithQuery_ConstructoBuilding_FreeFormatCustom()
{
	UrlBuilder<plain> builder("http://hello/world", CustomUrlPart());

	const string result(builder.c_str());

	CPPUNIT_ASSERT_EQUAL(string("http://hello/world?foo+++///"), result);
	CPPUNIT_ASSERT_EQUAL(builder.size(), result.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_NoParams()
{
	UrlBuilder<webForm> builder(queryOnly);

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string(), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(0), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_NoEscaping()
{
	UrlBuilder<webForm> builder(queryOnly, UrlPart<>("hello"), UrlPart<>("world"));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("hello=world"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(11), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_ParamNameEscaped()
{
	UrlBuilder<webForm> builder(queryOnly, UrlPart<>("he ll,o~"), UrlPart<>("world"));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("he%20ll%2co%7e=world"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(20), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_ParamValueEscaped()
{
	UrlBuilder<webForm> builder(queryOnly, UrlPart<>("hello"), UrlPart<>("**w*rld  "));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("hello=%2a%2aw%2arld%20%20"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(25), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_RawParamName()
{
	UrlBuilder<webForm> builder(queryOnly, UrlPart<raw>("he ll,o~"), UrlPart<ordinary>("**w*rld  "));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("he ll,o~=%2a%2aw%2arld%20%20"), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(28), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_SingleParam_RawParamValue()
{
	UrlBuilder<webForm> builder(queryOnly, UrlPart<>("he ll,o~"), UrlPart<raw>("**w*rld  "));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(string("he%20ll%2co%7e=**w*rld  "), string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(24), builder.size());
}

void UrlBuilderTest::testQueryOnly_ConstructorBuilding_MultipleParams()
{
	UrlBuilder<webForm> builder(queryOnly,
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

void UrlBuilderTest::testCapacityComputation_QueryOnly_OrdinaryParams_NoEscaping()
{
	/* Total size exceeds 127 (another capacity growth step for the internal buffer)
	 * while the size of param parts is less than 127.
	 */
	UrlBuilder<webForm> builder(queryOnly,
			UrlPart<>(string(21, 'a')), UrlPart<>(string(10, 'b')),
			UrlPart<>(string(6, 'c')), UrlPart<>(string(5, 'd')));

	const string expectedResult(string(21, 'a') + "=" + string(10, 'b') + "&" + string(6, 'c') + "=" + string(5, 'd'));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(expectedResult, string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(45), builder.size());
}

void UrlBuilderTest::testCapacityComputation_QueryOnly_OrdinaryParams_AllEscaped()
{
	/* Total size exceeds 127 (another capacity growth step for the internal buffer)
	 * while the size of param parts is less than 127.
	 */
	UrlBuilder<webForm> builder(queryOnly,
			UrlPart<>(string(21, '+')), UrlPart<>(string(10, '$')),
			UrlPart<>(string(6, '#')), UrlPart<>(string(5, '@')));

	string expectedResult;
	for (int i = 0; i < 21; ++i) {
		expectedResult += "%2b";
	}
	expectedResult += '=';
	for (int i = 0; i < 10; ++i) {
		expectedResult += "%24";
	}
	expectedResult += '&';
	for (int i = 0; i < 6; ++i) {
		expectedResult += "%23";
	}
	expectedResult += '=';
	for (int i = 0; i < 5; ++i) {
		expectedResult += "%40";
	}

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(expectedResult, string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(129), builder.size());
}

void UrlBuilderTest::testCapacityComputation_QueryOnly_RawParams()
{
	/* Total size exceeds 127 (another capacity growth step for the internal buffer)
	 * while the size of param parts is less than 127.
	 */
	UrlBuilder<webForm> builder(queryOnly,
			UrlPart<raw>(string(40, 'a')), UrlPart<raw>(string(40, 'b')),
			UrlPart<raw>(string(20, 'c')), UrlPart<raw>(string(5, 'd')),
			UrlPart<raw>(string(10, 'e')), UrlPart<raw>(string(10, 'f')));

	const string expectedResult(string(40, 'a') + "=" + string(40, 'b') + "&" +
			string(20, 'c') + "=" + string(5, 'd') + "&" + string(10, 'e') + "=" + string(10, 'f'));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(expectedResult, string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(130), builder.size());
}

void UrlBuilderTest::testCapacityComputation_UrlWithQuery()
{
	/* Total size exceeds 127 (another capacity growth step for the internal buffer)
	 * while the size of param parts is less than 127.
	 */
	UrlBuilder<webForm> builder("abcde",
			UrlPart<>(string(19, 'a')), UrlPart<>(string(10, 'b')),
			UrlPart<>(string(6, 'c')), UrlPart<>(string(5, 'd')));

	const string expectedResult(string("abcde") + "?" + string(19, 'a') + "=" + string(10, 'b') + "&" +
			string(6, 'c') + "=" + string(5, 'd'));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(expectedResult, string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(49), builder.size());
}

void UrlBuilderTest::testCapacityComputation_ParamsAppended()
{
	/* Total size exceeds 127 (another capacity growth step for the internal buffer)
	 * while the size of param parts is less than 127.
	 */
	UrlBuilder<webForm> builder(queryOnly,
			UrlPart<raw>(string(40, 'a')), UrlPart<raw>(string(40, 'b')));
	builder.params(
			UrlPart<raw>(string(20, 'c')), UrlPart<raw>(string(5, 'd')),
			UrlPart<raw>(string(10, 'e')), UrlPart<raw>(string(10, 'f')));

	const string expectedResult(string(40, 'a') + "=" + string(40, 'b') + "&" +
			string(20, 'c') + "=" + string(5, 'd') + "&" + string(10, 'e') + "=" + string(10, 'f'));

	const char * const result = builder.c_str();

	CPPUNIT_ASSERT(result != nullptr);
	CPPUNIT_ASSERT_EQUAL(expectedResult, string(result));
	CPPUNIT_ASSERT_EQUAL(size_t(130), builder.size());
}
