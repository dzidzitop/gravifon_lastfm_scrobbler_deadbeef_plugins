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

#include <algorithm>
#include <cassert>
#include <cstring>
#include <string>
#include <utility>
#include <afc/ensure_ascii.hpp>
#include <cstddef>
#include <afc/StringRef.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/number.h>

struct QueryOnly {} static const queryOnly;

template<typename Iterator>
Iterator appendUrlEncoded(const char *src, std::size_t n, Iterator dest);

enum UrlPartType
{
	// Characters are URL encoded.
	ordinary,
	// No characters are URL encoded.
	raw
};

template<UrlPartType type = ordinary>
class UrlPart
{
public:
	explicit constexpr UrlPart(const char * const value) noexcept : UrlPart(value, std::strlen(value)) {}
	constexpr UrlPart(const char * const value, const std::size_t n) noexcept : m_value(value), m_size(n) {}
	explicit constexpr UrlPart(const afc::ConstStringRef value) noexcept : UrlPart(value.value(), value.size()) {}
	explicit constexpr UrlPart(const std::string &value) noexcept : UrlPart(value.data(), value.size()) {}

	template<typename Iterator>
	Iterator appendTo(Iterator dest) const noexcept;
	constexpr std::size_t maxEncodedSize() const noexcept;
private:
	const char * const m_value;
	const std::size_t m_size;
};

template<>
constexpr std::size_t UrlPart<ordinary>::maxEncodedSize() const noexcept
{
	// Each character can be escaped as %xx.
	return 3 * m_size;
}

template<>
constexpr std::size_t UrlPart<raw>::maxEncodedSize() const noexcept { return m_size; }


template<>
template<typename Iterator>
inline Iterator UrlPart<ordinary>::appendTo(Iterator dest) const noexcept { return appendUrlEncoded(m_value, m_size, dest); };

template<>
template<typename Iterator>
inline Iterator UrlPart<raw>::appendTo(Iterator dest) const noexcept { return std::copy_n(m_value, m_size, dest); };

class UrlBuilder
{
private:
	UrlBuilder(const UrlBuilder &) = delete;
	UrlBuilder &operator=(const UrlBuilder &) = delete;
public:
	template<typename... Parts>
	UrlBuilder(const char * const urlBase, Parts&&... paramParts)
			: UrlBuilder(urlBase, std::strlen(urlBase), paramParts...) {}

	template<typename... Parts>
	UrlBuilder(const char * const urlBase, const std::size_t n, Parts&&... paramParts)
			: m_buf(std::max(minBufCapacity(), n + maxEncodedSize(paramParts...))), m_hasParams(false)
	{
		m_buf.append(urlBase, n);
		appendParams<urlFirst, Parts...>(paramParts...);
	}

	template<typename... Parts>
	UrlBuilder(const afc::ConstStringRef urlBase, Parts&&... paramParts)
			: UrlBuilder(urlBase.value(), urlBase.size(), paramParts...) {}

	template<typename... Parts>
	UrlBuilder(const std::string &urlBase, Parts&&... paramParts)
			: UrlBuilder(urlBase.c_str(), urlBase.size(), paramParts...) {}

	template<typename... Parts>
	UrlBuilder(QueryOnly, Parts&&... paramParts)
			: m_buf(std::max(minBufCapacity(), maxEncodedSize(paramParts...))), m_hasParams(false)
	{
		appendParams<queryString, Parts...>(std::forward<Parts>(paramParts)...);
	}

	UrlBuilder(UrlBuilder &&) = default;
	~UrlBuilder() = default;

	UrlBuilder &operator=(UrlBuilder &&) = default;

	template<typename... Parts>
	inline void params(Parts&&... parts)
	{
		const std::size_t estimatedEncodedSize = sizeof...(parts) * 2 + maxEncodedSize(parts...);
		m_buf.reserve(m_buf.size() + estimatedEncodedSize);

		appendParams<urlUnknown, Parts...>(std::forward<Parts>(parts)...);
	}

	const char *data() const noexcept { return m_buf.data(); }
	const char *c_str() const noexcept { return m_buf.c_str(); }
	const std::size_t size() const noexcept { return m_buf.size(); }
private:
	template<typename Part, typename... Parts>
	static constexpr std::size_t maxEncodedSize(Part &&part, Parts&&... parts) noexcept
	{
		// TODO think of removing the redundant '?' from here for query-string-only cases.
		// '?' or '&' or '=' is counted here, too.
		return 1 + part.maxEncodedSize() + maxEncodedSize(parts...);
	}

	// Terminates the maxEncodedSize() template recusion.
	static constexpr std::size_t maxEncodedSize() noexcept { return 0; }

	enum ParamMode
	{
		// A URL is being built; the next parameter is known to be first.
		urlFirst,
		// A URL is being built; it is unknown if the next parameter is first.
		urlUnknown,
		// A query string is being built.
		queryString,
		// The next parameter is known to be not first.
		notFirst
	};

	template<ParamMode mode, typename... Parts>
	void appendParams(Parts&&... parts) noexcept
	{
		static_assert((sizeof...(parts) % 2) == 0, "Number of URL parts must be even.");

		appendParamName<mode, Parts...>(std::forward<Parts>(parts)...);
	}

	template<ParamMode mode, typename ParamName, typename... Parts>
	void appendParamName(ParamName &&name, Parts&&... parts) noexcept
	{
		// It is guaranteed that there is enough capacity for all the parameters.
		switch (mode) {
		case urlFirst:
			m_buf.append('?');
			m_hasParams = true;
			break;
		case urlUnknown:
			m_buf.append(m_hasParams ? '&' : '?');
			// Has params is assigned in either case to produce less jumps.
			m_hasParams = true;
			break;
		case queryString:
			m_hasParams = true;
			break;
		case notFirst:
			m_buf.append('&');
			break;
		default:
			assert(false);
		}
		appendParamPart(name);
		appendParamValue(parts...);
	}

	/* Terminates the appendParamName() template recursion.
	 * No '?' is appended even if there are no parameters at all.
	 */
	template<ParamMode mode>
	void appendParamName() noexcept {}

	template<typename ParamValue, typename... Parts>
	void appendParamValue(ParamValue &&value, Parts&&... parts) noexcept
	{
		// It is guaranteed that there is enough capacity for all the parameters.
		m_buf.append('=');
		appendParamPart(value);
		appendParamName<notFirst, Parts...>(parts...);
	}

	template<typename Part>
	void appendParamPart(Part &&part) noexcept
	{
		afc::FastStringBuffer<char>::Tail p = m_buf.borrowTail();
		afc::FastStringBuffer<char>::Tail q = part.appendTo(p);
		m_buf.returnTail(q);
	}

	/* Many short urls have length less than 64 characters. Setting this value
	 * as the minimal capacity to minimise re-allocations.
	 *
	 * This function emulates normal inlinable constants.
	 */
	static constexpr std::size_t minBufCapacity() { return 64; };

	afc::FastStringBuffer<char> m_buf;
	bool m_hasParams;
};

template<typename Iterator>
Iterator appendUrlEncoded(const char * const src, const std::size_t n, Iterator dest)
{
	if (n == 0) {
		return dest;
	}

	std::size_t i = 0;
	do {
		const char c = src[i];

		/* Casting to unsigned since bitwise operators are defined well for them
		 * in terms of values.
		 *
		 * Only the lowest octet matters for URL encoding, even if unsigned char is larger.
		 */
		const unsigned char uc = static_cast<unsigned char>(c) & 0xff;

		if ((uc >= 'A' && uc <= 'Z') || (uc >= 'a' && uc <= 'z') || (uc >= '0' && uc <= '9') ||
				uc == '-' || uc == '_' || uc == '.' || uc == '~') {
			// An unreserved character. No escaping is needed.
			*dest++ = c;
		} else {
			/* A non-unreserved character. Escaping it to percent-encoded representation.
			 * The reserved characters are escaped, too, for simplicity. */
			char c[3];
			c[0] = '%';
			afc::octetToHex(uc, &c[1]);
			dest = std::copy_n(c, 3, dest);
		}
	} while (++i < n);

	return dest;
}

#endif /* URLBUILDER_HPP_ */
