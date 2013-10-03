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
#include <sys/select.h>

using std::string;

namespace
{
	struct CurlInit
	{
		const bool initialised;

		CurlInit() noexcept : initialised (curl_global_init(CURL_GLOBAL_ALL) == 0) {};
		~CurlInit() noexcept { curl_global_cleanup(); };
	};

	static const CurlInit curlInit;

	void waitForSocketData(const int socketfd, const long timeoutMillis)
	{
		timeval timeout;
		timeout.tv_sec = timeoutMillis / 1000;
		timeout.tv_usec = (timeoutMillis % 1000) * 1000;

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(socketfd, &readfds);

		fd_set errfds;
		FD_ZERO(&errfds);
		FD_SET(socketfd, &errfds);

		const int status = select(socketfd + 1, &readfds, nullptr, &errfds, &timeout);
		if (status < 0) {
			// TODO error
		} else if (status == 0) {
			// TODO timeout expired
		}
	}
}

HttpClient::HttpClient()
{
	if (!::curlInit.initialised) {
		// TODO handle error
	}
}

string HttpClient::send(const string &url, const string &data)
{
	CURL * const curl = curl_easy_init();
	if (curl == nullptr) {
		throw HttpClientException("Unable to establish connection.");
	}

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 1L);
	CURLcode status = curl_easy_perform(curl);
	if (status != 0) {
		throw HttpClientException("Unable to establish connection.");
	}

	size_t bytesSent;

	status = curl_easy_send(curl, data.c_str(), data.size(), &bytesSent);
	if (status != CURLE_OK) {
		throw HttpClientException("Unable to send data.");
	}

	curl_socket_t socketfd;

	status = curl_easy_getinfo(curl, CURLINFO_LASTSOCKET, &socketfd);
	if (status != CURLE_OK) {
		throw HttpClientException("Unable to process request.");
	}

	size_t bytesRead;
	string response;
	for (;;) {
		char buf[1024];
		status = curl_easy_recv(curl, buf, sizeof(buf), &bytesRead);

		if (status == CURLE_OK) {
			if (bytesRead == 0) {
				break;
			} else {
				// TODO support re-coding into a correct charset
				response.append(buf, bytesRead);
			}
		} else if (status == CURLE_AGAIN) {
			waitForSocketData(socketfd, 1000);
		} else if (status == CURLE_UNSUPPORTED_PROTOCOL) {
			break;
		} else {
			throw HttpClientException("Unable to read response data.");
		}
	}

	curl_easy_cleanup(curl);
	return response;
}
