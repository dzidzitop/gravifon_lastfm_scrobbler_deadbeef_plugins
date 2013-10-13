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

		tm * const dateTime = localtime(&t);
		dateTime->tm_year = year - 1900;
		dateTime->tm_mon = month - 1;
		dateTime->tm_mday = day;
		dateTime->tm_hour = hour;
		dateTime->tm_min = minute;
		dateTime->tm_sec = second;
		dateTime->tm_isdst = -1;

		return mktime(dateTime);
	}

	void getTimeZone(const time_t t, char dest[6])
	{
		const size_t count = std::strftime(dest, sizeof(dest), "%z", localtime(&t));
		CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(5), count);
	}
}

void ScrobbleInfoTest::testSerialiseScrobbleInfo_WithAllFields()
{
	time_t scrobbleStart = t(2000, 1, 1, 23, 12, 33);
	time_t scrobbleEnd = t(2001, 2, 3, 12, 10, 4);
	char scrobbleStartTimeZone[6];
	char scrobbleEndTimeZone[6];
	getTimeZone(scrobbleStart, scrobbleStartTimeZone);
	getTimeZone(scrobbleEnd, scrobbleEndTimeZone);

	ScrobbleInfo scrobbleInfo;
	scrobbleInfo.scrobbleStartTimestamp = scrobbleStart;
	scrobbleInfo.scrobbleEndTimestamp = scrobbleEnd;
	scrobbleInfo.scrobbleDuration = 1001;
	Track &track = scrobbleInfo.track;
	track.setTitle(u8"'39");
	track.setAlbumTitle(u8"A Night at the Opera");
	track.setArtist(u8"Queen");
	track.setDurationMillis(12);

	string result;
	result += scrobbleInfo;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"scrobble_start_datetime":"2000-01-01T23:12:33)") + scrobbleStartTimeZone +
			R"(","scrobble_end_datetime":"2001-02-03T12:10:04)" + scrobbleEndTimeZone +
			R"(","scrobble_duration":{"amount":1001,"unit":"ms"},"track":{"title":"\'39","artists":[{"name":"Queen"}],)"
			R"("album":{"title":"A Night at the Opera"},)"
			R"("length":{"amount":12,"unit":"ms"}}})", result);
}
