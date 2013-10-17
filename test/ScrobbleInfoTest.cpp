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
#include "ScrobbleInfoTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(ScrobbleInfoTest);

#include <GravifonClient.hpp>
#include <ctime>

using namespace std;

namespace
{
	time_t t(const int year, const int month, const int day, const int hour, const int minute, const int second)
	{
		time_t t;
		time(&t);

		tm * const dateTime = gmtime(&t);
		dateTime->tm_year = year - 1900;
		dateTime->tm_mon = month - 1;
		dateTime->tm_mday = day;
		dateTime->tm_hour = hour;
		dateTime->tm_min = minute;
		dateTime->tm_sec = second;
		dateTime->tm_isdst = -1;

		return timegm(dateTime);
	}
}

void ScrobbleInfoTest::testSerialiseScrobbleInfo_WithAllFields()
{
	time_t scrobbleStart = t(2000, 1, 1, 23, 12, 33);
	time_t scrobbleEnd = t(2001, 2, 3, 12, 10, 4);

	ScrobbleInfo scrobbleInfo;
	scrobbleInfo.scrobbleStartTimestamp = scrobbleStart;
	scrobbleInfo.scrobbleEndTimestamp = scrobbleEnd;
	scrobbleInfo.scrobbleDuration = 1001;
	Track &track = scrobbleInfo.track;
	track.setTitle(u8"'39");
	track.setAlbumTitle(u8"A Night at the Opera");
	track.addArtist(u8"Queen");
	track.setDurationMillis(12);

	string result;
	result += scrobbleInfo;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2000-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2001-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1001,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":12,"unit":"ms"}}})"), result);
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_WithAllFields_SingleArtist()
{
	string input(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})");

	ScrobbleInfo result;
	const bool status = ScrobbleInfo::parse(input, result);

	CPPUNIT_ASSERT(status);

	string serialisedScrobble;
	serialisedScrobble += result;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), serialisedScrobble);
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_WithAllFields_MultipleArtists()
{
	string input(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"},{"name":"Scorpions"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})");

	ScrobbleInfo result;
	const bool status = ScrobbleInfo::parse(input, result);

	CPPUNIT_ASSERT(status);

	string serialisedScrobble;
	serialisedScrobble += result;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"},{"name":"Scorpions"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), serialisedScrobble);
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_WithAllFields_NoAlbum()
{
	string input(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})");

	ScrobbleInfo result;
	const bool status = ScrobbleInfo::parse(input, result);

	CPPUNIT_ASSERT(status);

	string serialisedScrobble;
	serialisedScrobble += result;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("length":{"amount":207026,"unit":"ms"}}})"), serialisedScrobble);
}

void ScrobbleInfoTest::testDeserialiseScrobbleInfo_MalformedJson()
{
	string input(u8R"({"scrobble_start_datetime":"2002-01-01T23:12:33+0000",)"
			u8R"("scrobble_end_datetime":"2003-02-03T12:10:04+0000",)"
			u8R"("scrobble_duration":{"amount":1207,"unit":"ms"},)"
			u8R"("track":{"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("length":{"amount":207026,"unit":"ms")");

	ScrobbleInfo result;
	const bool status = ScrobbleInfo::parse(input, result);

	CPPUNIT_ASSERT(!status);
}
