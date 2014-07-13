/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

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
#ifndef SCROBBLEINFOTEST_HPP_
#define SCROBBLEINFOTEST_HPP_

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <memory>
#include <string>

class ScrobbleInfoTest : public CppUnit::TestFixture
{
	CPPUNIT_TEST_SUITE(ScrobbleInfoTest);
	CPPUNIT_TEST(testSerialiseScrobbleInfo_WithAllFields);
	CPPUNIT_TEST(testDeserialiseScrobbleInfo_WithAllFields_SingleArtist);
	CPPUNIT_TEST(testDeserialiseScrobbleInfo_WithAllFields_MultipleArtists);
	CPPUNIT_TEST(testDeserialiseScrobbleInfo_WithAllFields_MultipleAlbumArtists);
	CPPUNIT_TEST(testDeserialiseScrobbleInfo_NoAlbum);
	CPPUNIT_TEST(testDeserialiseScrobbleInfo_NoAlbumArtists);
	CPPUNIT_TEST(testDeserialiseScrobbleInfo_MalformedJson);
	CPPUNIT_TEST_SUITE_END();

	std::unique_ptr<std::string> m_timeZoneBackup;
public:
	void setUp() override;
	void tearDown() override;

	void testSerialiseScrobbleInfo_WithAllFields();
	void testDeserialiseScrobbleInfo_WithAllFields_SingleArtist();
	void testDeserialiseScrobbleInfo_WithAllFields_MultipleArtists();
	void testDeserialiseScrobbleInfo_WithAllFields_MultipleAlbumArtists();
	void testDeserialiseScrobbleInfo_NoAlbum();
	void testDeserialiseScrobbleInfo_NoAlbumArtists();
	void testDeserialiseScrobbleInfo_MalformedJson();
};

#endif /* SCROBBLEINFOTEST_HPP_ */
