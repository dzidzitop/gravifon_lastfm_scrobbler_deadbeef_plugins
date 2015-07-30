/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2014-2015 Dźmitry Laŭčuk

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
#ifndef GRAVIFONSCROBBLER_HPP_
#define GRAVIFONSCROBBLER_HPP_

#include "ScrobbleInfo.hpp"
#include "Scrobbler.hpp"
#include <cstddef>
#include <list>
#include <mutex>
#include <utility>

#include <afc/SimpleString.hpp>

// TODO think of choosing a better in-memory container for pending scrobbles (issue #16).
class GravifonScrobbler : public Scrobbler<std::list<ScrobbleInfo>>
{
public:
	GravifonScrobbler() : Scrobbler(), m_scrobblerUrl(), m_authHeader(), m_dataFilePath()
	{ std::lock_guard<std::mutex> lock(m_mutex); // synchronising memory
		/* This instance is partially initialised here. It will be initialised completely
		 * when ::start() is invoked successfully.
		 */
	}

	virtual ~GravifonScrobbler()
	{
		// Synchronising memory before destructing the member fields of this GravifonScrobbler.
		std::lock_guard<std::mutex> lock(m_mutex);
	}

	/* - serverUrl must be a valid ASCII-compatible string
	 * - username should conform to https://gist.github.com/bassstorm/bae655c72a1449f7e6ab
	 * - password should conform to https://gist.github.com/bassstorm/a53fa95806650648fdda
	 * - both username and password are expected to be in ISO-8859-1.
	 */
	void configure(const char *serverUrl, std::size_t serverUrlSize, const char *username, std::size_t usernameSize,
			const char *password, std::size_t passwordSize);

	void setDataFilePath(const afc::SimpleString &dataFilePath)
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_dataFilePath = dataFilePath;
	}

	void setDataFilePath(afc::SimpleString &&dataFilePath)
	{ std::lock_guard<std::mutex> lock(m_mutex);
		m_dataFilePath = std::move(dataFilePath);
	}
protected:
	virtual std::size_t doScrobbling() override;

	virtual const afc::SimpleString &getDataFilePath() const override { return m_dataFilePath; }

	virtual void stopExtra() override;
private:
	afc::SimpleString m_scrobblerUrl;
	// The authentication header encoded in the basic charset.
	afc::SimpleString m_authHeader;

	afc::SimpleString m_dataFilePath;
};

#endif /* GRAVIFONSCROBBLER_HPP_ */
