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
#include <cstdlib>

CPPUNIT_TEST_SUITE_REGISTRATION(TrackTest);

#include <GravifonClient.hpp>

using namespace std;

void TrackTest::testSerialiseTrack_WithAllFields()
{
	Track track;
	track.setTitle(u8"'39");
	track.setAlbumTitle(u8"A Night at the Opera");
	track.addArtist(u8"Queen");
	track.addAlbumArtist(u8"Scorpions");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"Scorpions"}]},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_WithAllFields_StringsContainNonASCIICharacters()
{
	Track track;
	track.setTitle(u8"Dzie\u0144");
	track.setAlbumTitle(u8"Vie\u010dar");
	track.addArtist(u8"No\u010d");
	track.addAlbumArtist(u8"Nadvia\u010dorak");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8"{\"title\":\"Dzie\u0144\",\"artists\":[{\"name\":\"No\u010d\"}],"
			u8"\"album\":{\"title\":\"Vie\u010dar\",\"artists\":[{\"name\":\"Nadvia\u010dorak\"}]},"
			u8"\"length\":{\"amount\":210000,\"unit\":\"ms\"}}"),
			result);
}

void TrackTest::testSerialiseTrack_TrackNameWithEscapeCharacters()
{
	Track track;
	track.setTitle(u8"A\"'\\\b\f\n\r\tbc");
	track.setAlbumTitle(u8"Test album");
	track.addArtist(u8"Test artist");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":")" u8"A\\\"'\\\\\\b\\f\\n\\r\\tbc" "\","
			u8R"("artists":[{"name":"Test artist"}],"album":{"title":"Test album"},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_AlbumNameWithEscapeCharacters()
{
	Track track;
	track.setTitle(u8"Test track");
	track.setAlbumTitle(u8"\"'\\\b\f\n\r\t++");
	track.addArtist(u8"Test artist");
	track.setDurationMillis(1234);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"Test track",)"
			u8R"("artists":[{"name":"Test artist"}],"album":{"title":")" u8"\\\"'\\\\\\b\\f\\n\\r\\t++" R"("},)"
			u8R"("length":{"amount":1234,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_ArtistNameWithEscapeCharacters()
{
	Track track;
	track.setTitle(u8"Test track");
	track.setAlbumTitle(u8"Test album");
	track.addArtist(u8"_\"'\\\b\f\n\r\t_");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"Test track",)"
			u8R"("artists":[{"name":")" u8"_\\\"'\\\\\\b\\f\\n\\r\\t_" R"("}],"album":{"title":"Test album"},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_AlbumArtistNameWithEscapeCharacters()
{
	Track track;
	track.setTitle(u8"Test track");
	track.addArtist(u8"Queen");
	track.setAlbumTitle(u8"Test album");
	track.addAlbumArtist(u8"_\"'\\\b\f\n\r\t_");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"Test track",)"
			u8R"("artists":[{"name":"Queen"}],"album":{"title":"Test album",)"
			u8R"("artists":[{"name":")" u8"_\\\"'\\\\\\b\\f\\n\\r\\t_" R"("}]},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_WithNoAlbum()
{
	Track track;
	track.setTitle(u8"Test track");
	track.addArtist(u8"Test artist");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"Test track","artists":[{"name":"Test artist"}],)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_WithNoAlbumArtists()
{
	Track track;
	track.setTitle(u8"'39");
	track.setAlbumTitle(u8"A Night at the Opera");
	track.addArtist(u8"Queen");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_MultipleArtists()
{
	Track track;
	track.setTitle(u8"'39");
	track.setAlbumTitle(u8"A Night at the Opera");
	track.addArtist(u8"Queen");
	track.addArtist(u8"Scorpions");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"'39","artists":[{"name":"Queen"},{"name":"Scorpions"}],)"
			u8R"("album":{"title":"A Night at the Opera"},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}

void TrackTest::testSerialiseTrack_MultipleAlbumArtists()
{
	Track track;
	track.setTitle(u8"'39");
	track.setAlbumTitle(u8"A Night at the Opera");
	track.addArtist(u8"Queen");
	track.addAlbumArtist(u8"Scorpions");
	track.addAlbumArtist(u8"ABBA");
	track.setDurationMillis(210000);

	string result;
	track.appendTo(result);

	CPPUNIT_ASSERT_EQUAL(string(u8R"({"title":"'39","artists":[{"name":"Queen"}],)"
			u8R"("album":{"title":"A Night at the Opera","artists":[{"name":"Scorpions"},{"name":"ABBA"}]},)"
			u8R"("length":{"amount":210000,"unit":"ms"}})"), result);
}
