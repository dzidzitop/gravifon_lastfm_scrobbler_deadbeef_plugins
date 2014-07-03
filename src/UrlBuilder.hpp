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

	inline UrlBuilder &param(const char * const name, const std::size_t nameSize,
			const char * const value, const std::size_t valueSize)
	{
		return paramName(name, nameSize).paramValue(value, valueSize);
	}

	inline UrlBuilder &param(const afc::ConstStringRef name, const afc::ConstStringRef value)
	{
		return paramName(name).paramValue(value);
	}

	inline UrlBuilder &paramName(const char * const name)
	{
		appendParamPrefix();
		appendUrlEncoded(name);
		return *this;
	}

	inline UrlBuilder &paramName(const char * const name, const std::size_t nameSize)
	{
		appendParamPrefix();
		appendUrlEncoded(name, nameSize);
		return *this;
	}

	inline UrlBuilder &paramName(afc::ConstStringRef name) { return paramName(name.value(), name.size()); }

	inline UrlBuilder &paramName(const std::string &name) { return paramName(name.c_str(), name.size()); }

	inline UrlBuilder &paramValue(const char * const value)
	{
		m_buf += '=';
		appendUrlEncoded(value);
		return *this;
	}

	inline UrlBuilder &paramValue(const char * const value, const std::size_t valueSize)
	{
		m_buf += '=';
		appendUrlEncoded(value, valueSize);
		return *this;
	}

	inline UrlBuilder &paramValue(const afc::ConstStringRef value) { return paramValue(value.value(), value.size()); }

	inline UrlBuilder &paramValue(const std::string &value) { return paramValue(value.c_str(), value.size()); }

	// Neither name nor value are URL-encoded.
	inline UrlBuilder &rawParam(const char * const name, const char * const value)
	{
		return rawParamName(name).rawParamValue(value);
	}

	// Neither name nor value are URL-encoded.
	inline UrlBuilder &rawParam(const char * const name, const std::size_t nameSize,
			const char * const value, const std::size_t valueSize)
	{
		return rawParamName(name, nameSize).rawParamValue(value, valueSize);
	}

	// Neither name nor value are URL-encoded.
	inline UrlBuilder &rawParam(const afc::ConstStringRef name, const afc::ConstStringRef value)
	{
		return rawParamName(name).rawParamValue(value);
	}

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const char * const name)
	{
		appendParamPrefix();
		m_buf.append(name);
		return *this;
	}

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const char * const name, const std::size_t nameSize)
	{
		appendParamPrefix();
		m_buf.append(name, nameSize);
		return *this;
	}

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(afc::ConstStringRef name) { return paramName(name.value(), name.size()); }

	// Name is not URL-encoded.
	inline UrlBuilder &rawParamName(const std::string &name) { return paramName(name.c_str(), name.size()); }

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const char * const value)
	{
		m_buf += '=';
		appendUrlEncoded(value);
		return *this;
	}

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const char * const value, const std::size_t valueSize)
	{
		m_buf += '=';
		appendUrlEncoded(value, valueSize);
		return *this;
	}

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const afc::ConstStringRef value) { return paramValue(value.value(), value.size()); }

	// Value is not URL-encoded.
	inline UrlBuilder &rawParamValue(const std::string &value) { return paramValue(value.c_str(), value.size()); }

	const std::string &toString() const { return m_buf; }
private:
	inline void appendParamPrefix()
	{
		if (m_hasParams) {
			m_buf += '&';
		} else {
			m_buf += '?';
			m_hasParams = true;
		}
	}

	inline void appendUrlEncoded(const char *str)
	{
		char c;
		std::size_t i = 0;
		while ((c = str[i++]) != '\0') {
			appendUrlEncoded(c, m_buf);
		}
	}

	inline void appendUrlEncoded(const char *str, const std::size_t n)
	{
		// Each character escaped takes up to three characters.
		m_buf.reserve(m_buf.size() + n * 3);

		for (size_t i = 0; i < n; ++i) {
			appendUrlEncoded(str[i], m_buf);
		}
	}

	static void appendUrlEncoded(char c, std::string &dest);

	std::string m_buf;
	bool m_hasParams;
};

#endif /* URLBUILDER_HPP_ */
