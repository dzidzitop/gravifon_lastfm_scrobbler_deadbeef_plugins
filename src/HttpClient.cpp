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
#include "HttpClient.hpp"
#include <curl/curl.h>
#include <cassert>

using namespace std;

namespace
{
	class CurlInit
	{
		CurlInit(const CurlInit &) = delete;
		CurlInit(CurlInit &&) = delete;
		CurlInit &operator=(const CurlInit &) = delete;
		CurlInit &operator=(CurlInit &&) = delete;
	public:
		static CurlInit instance;

		const bool initialised;
	private:
		CurlInit() noexcept : initialised(curl_global_init(CURL_GLOBAL_ALL) == 0) {}
		~CurlInit() noexcept { curl_global_cleanup(); }
	};

	CurlInit CurlInit::instance;

	class CurlSession
	{
		CurlSession(const CurlSession &) = delete;
		CurlSession(CurlSession &&) = delete;
		CurlSession &operator=(const CurlSession &) = delete;
		CurlSession &operator=(CurlSession &&) = delete;
	public:
		CURL * const handler;

		CurlSession() : handler(curl_easy_init()) {}

		~CurlSession() noexcept { curl_easy_cleanup(handler); };

		operator CURL *() noexcept { return handler; }
	};

	class CurlHeaders
	{
		CurlHeaders(const CurlHeaders &) = delete;
		CurlHeaders(CurlHeaders &&) = delete;
		CurlHeaders &operator=(const CurlHeaders &) = delete;
		CurlHeaders &operator=(CurlHeaders &&) = delete;
	public:
		CurlHeaders() : m_headers(nullptr) {}
		~CurlHeaders() { curl_slist_free_all(m_headers); }

		operator curl_slist *() { return m_headers; }

		bool addHeader(const char * const header)
		{
			curl_slist * const tmp = curl_slist_append(m_headers, header);

			if (tmp == nullptr) {
				return false;
			}
			m_headers = tmp;
			return true;
		}
	private:
		curl_slist *m_headers;
	};

	size_t writeData(char * const data, const size_t size, const size_t nmemb, void * const userdata)
	{
		size_t dataSize = size * nmemb;
		string * const dest = reinterpret_cast<string *>(userdata);

		dest->append(data, dataSize);

		return dataSize;
	}

	HttpClient::StatusCode toStatusCode(CURLcode curlErrorCode)
	{
		switch (curlErrorCode) {
		case CURLE_COULDNT_CONNECT:
			return HttpClient::StatusCode::UNABLE_TO_CONNECT;
		default:
			return HttpClient::StatusCode::UNKNOWN_ERROR;
		}
	}
}

HttpClient::HttpClient()
{
	if (!::CurlInit::instance.initialised) {
		// TODO handle error
	}
}

HttpClient::StatusCode HttpClient::send(const string &url, const HttpEntity &request, HttpResponseEntity &response,
		const long connectionTimeoutMillis, const long socketTimeoutMillis)
{
	CurlSession curl;

	if (curl.handler == nullptr) {
		return StatusCode::UNKNOWN_ERROR;
	}

	CurlHeaders headers;
	for (const char * const header : request.headers) {
		if (!headers.addHeader(header)) {
			return StatusCode::UNKNOWN_ERROR;
		}
	}

	// TODO add response headers
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(request.body.size()));
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, static_cast<curl_slist *>(headers));
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeData);

	// Setting timeouts.
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connectionTimeoutMillis);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, socketTimeoutMillis);

	CURLcode status = curl_easy_perform(curl);
	if (status != 0) {
		return toStatusCode(status);
	}

	long statusCode;
	if (curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode) != 0) {
		return toStatusCode(status);
	}

	response.statusCode = static_cast<int>(statusCode);

	return StatusCode::SUCCESS;
}
