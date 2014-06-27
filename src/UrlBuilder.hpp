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
#ifndef URLBUILDER_HPP_
#define URLBUILDER_HPP_

#include <string>
#include <afc/ensure_ascii.hpp>
#include <cstddef>
#include <afc/StringRef.hpp>

class UrlBuilder
{
private:
	UrlBuilder(const UrlBuilder &) = delete;
	UrlBuilder(UrlBuilder &&) = delete;
	UrlBuilder &operator=(const UrlBuilder &) = delete;
	UrlBuilder &operator=(UrlBuilder &&) = delete;
public:
	UrlBuilder(const char * const urlBase) : m_buf(urlBase), m_hasParams(false) {}
	UrlBuilder(const std::string &urlBase) : m_buf(urlBase), m_hasParams(false) {}
	UrlBuilder(std::string &&urlBase) : m_buf(urlBase), m_hasParams(false) {}

	~UrlBuilder() = default;

	inline UrlBuilder &param(const char * const name, const char * const value)
	{
		return paramName(name).paramValue(value);
	}

	inline UrlBuilder &param(const char * const name, const size_t nameSize,
			const char * const value, const size_t valueSize)
	{
		return paramName(name, nameSize).paramValue(value, valueSize);
	}

	inline UrlBuilder &param(afc::ConstStringRef name, afc::ConstStringRef value)
	{
		return paramName(name).paramValue(value);
	}

	inline UrlBuilder &paramName(const char * const name)
	{
		if (m_hasParams) {
			m_buf += '&';
		} else {
			m_buf += '?';
			m_hasParams = true;
		}
		appendUrlEncoded(name);
		return *this;
	}

	inline UrlBuilder &paramName(const char * const name, const size_t nameSize)
	{
		if (m_hasParams) {
			m_buf += '&';
		} else {
			m_buf += '?';
			m_hasParams = true;
		}
		appendUrlEncoded(name, nameSize);
		return *this;
	}

	inline UrlBuilder &paramName(afc::ConstStringRef name) { return paramName(name.value(), name.size()); }

	inline UrlBuilder &paramValue(const char * const value)
	{
		m_buf += '=';
		appendUrlEncoded(value);
		return *this;
	}

	inline UrlBuilder &paramValue(const char * const value, const size_t valueSize)
	{
		m_buf += '=';
		appendUrlEncoded(value, valueSize);
		return *this;
	}

	inline UrlBuilder &paramValue(afc::ConstStringRef value) { return paramValue(value.value(), value.size()); }

	const std::string &toString() const { return m_buf; }
private:
	void appendUrlEncoded(const char *str);
	void appendUrlEncoded(const char *str, const size_t n);

	std::string m_buf;
	bool m_hasParams;
};

#endif /* URLBUILDER_HPP_ */
