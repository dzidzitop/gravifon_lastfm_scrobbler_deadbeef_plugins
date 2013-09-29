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
	track.trackName = u8"'39";
	track.album = u8"A Night at the Opera";
	track.artist = u8"Queen";
	track.duration = 210000;
	track.trackNumber = 5;

	stringstream buf;
	buf << track;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"\'39","artists":["Queen"],"album":{"title":"A Night at the Opera"},)"
			R"("length":{"amount":210000,"unit":"ms"},"number":5})"), buf.str());
}

void TrackTest::testSerialiseTrack_WithAllFields_StringsContainNonASCIICharacters()
{
	Track track;
	track.trackName = u8"Dzie\u0144";
	track.album = u8"Vie\u010dar";
	track.artist = u8"No\u010d";
	track.duration = 210000;
	track.trackNumber = 5;

	stringstream buf;
	buf << track;

	CPPUNIT_ASSERT_EQUAL(string(u8"{\"title\":\"Dzie\u0144\",\"artists\":[\"No\u010d\"],"
			"\"album\":{\"title\":\"Vie\u010dar\"},\"length\":{\"amount\":210000,\"unit\":\"ms\"},\"number\":5}"),
			buf.str());
}

void TrackTest::testSerialiseTrack_WithAllFields_TrackNameWithEscapeCharacters()
{
	Track track;
	track.trackName = u8"A\"'\\\b\f\n\r\tbc";
	track.album = u8"Test album";
	track.artist = u8"Test artist";
	track.duration = 210000;
	track.trackNumber = 5;

	stringstream buf;
	buf << track;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":")" "A\\\"\\'\\\\\\b\\f\\n\\r\\tbc" "\","
			R"("artists":["Test artist"],"album":{"title":"Test album"},)"
			R"("length":{"amount":210000,"unit":"ms"},"number":5})"), buf.str());
}

void TrackTest::testSerialiseTrack_WithAllFields_AlbumNameWithEscapeCharacters()
{
	Track track;
	track.trackName = u8"A\"'\\\b\f\n\r\tbc";
	track.album = u8"\"'\\\b\f\n\r\t++";
	track.artist = u8"Test artist";
	track.duration = 210000;
	track.trackNumber = 5;

	stringstream buf;
	buf << track;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":")" "A\\\"\\'\\\\\\b\\f\\n\\r\\tbc" "\","
			R"("artists":["Test artist"],"album":{"title":")" "\\\"\\'\\\\\\b\\f\\n\\r\\t++" R"("},)"
			R"("length":{"amount":210000,"unit":"ms"},"number":5})"), buf.str());
}

void TrackTest::testSerialiseTrack_WithAllFields_ArtistNameWithEscapeCharacters()
{
	Track track;
	track.trackName = u8"Test track";
	track.album = u8"Test album";
	track.artist = u8"_\"'\\\b\f\n\r\t_";
	track.duration = 210000;
	track.trackNumber = 5;

	stringstream buf;
	buf << track;

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"Test track",)"
			R"("artists":[")" "_\\\"\\'\\\\\\b\\f\\n\\r\\t_" R"("],"album":{"title":"Test album"},)"
			R"("length":{"amount":210000,"unit":"ms"},"number":5})"), buf.str());
}
