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
#include "TrackTest.hpp"

CPPUNIT_TEST_SUITE_REGISTRATION(TrackTest);

#include <GravifonClient.hpp>
#include <sstream>

using namespace std;

void TrackTest::testSerialiseTrack_WithAllFields()
{
	Track track;
	track.trackName = "'39";
	track.album = "A Night at the Opera";
	track.artist = "Queen";
	track.duration = 210000;
	track.trackNumber = 5;

	stringstream buf;
	buf << track;

	CPPUNIT_ASSERT_EQUAL(string(R"({"title":"'39","artists":["Queen"],"album":{"title":"A Night at the Opera"},)"
			R"("length":{"amount":210000,"unit":"ms"},"number":5})"), buf.str());
}
