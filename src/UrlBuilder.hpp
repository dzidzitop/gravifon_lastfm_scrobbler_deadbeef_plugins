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
#include <type_traits>
#include <utility>
#include <afc/ensure_ascii.hpp>
#include <cstddef>
#include <afc/StringRef.hpp>
#include <afc/FastStringBuffer.hpp>
#include <afc/number.h>

struct QueryOnly {} static const queryOnly;

template<typename Iterator>
Iterator appendUrlEncoded(const char *src, std::size_t n, Iterator dest);

enum QueryFormat
{
	plain,
	webForm
};

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

template<QueryFormat queryFormat>
class UrlBuilder
{
private:
	UrlBuilder(const UrlBuilder &) = delete;
	UrlBuilder &operator=(const UrlBuilder &) = delete;
public:
	UrlBuilder(const char * const urlBase) : UrlBuilder(urlBase, std::strlen(urlBase)) {}
	UrlBuilder(const char * const urlBase, const std::size_t n)
			: m_buf(std::max(minBufCapacity(), n)), m_queryState(queryEmptyUrl) { m_buf.append(urlBase, n); }
	UrlBuilder(const afc::ConstStringRef urlBase) : UrlBuilder(urlBase.value(), urlBase.size()) {}
	UrlBuilder(const std::string &urlBase) : UrlBuilder(urlBase.c_str(), urlBase.size(), query) {}
	// TODO set query-only mode properly or remove this constructor.
	UrlBuilder(QueryOnly) : m_buf(minBufCapacity()), m_queryState(queryEmptyQueryString) {}

	template<typename QueryString, typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	UrlBuilder(const char * const urlBase, QueryString &&query)
			: UrlBuilder(urlBase, std::strlen(urlBase), query) {}

	template<typename QueryString, typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	UrlBuilder(const char * const urlBase, const std::size_t n, QueryString &&query)
			: m_buf(std::max(minBufCapacity(), n + maxEncodedSize<urlFirst, QueryString>(query)))
	{
		m_buf.append(urlBase, n);
		// m_queryState is initialised here.
		appendQueryString<urlFirst, QueryString>(std::forward<QueryString>(query));
	}

	template<typename QueryString, typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	UrlBuilder(const afc::ConstStringRef urlBase, QueryString &&query)
			: UrlBuilder(urlBase.value(), urlBase.size(), query) {}

	template<typename QueryString, typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	UrlBuilder(const std::string &urlBase, QueryString &&query)
			: UrlBuilder(urlBase.c_str(), urlBase.size(), query) {}

	template<typename QueryString, typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	UrlBuilder(QueryOnly, QueryString &&query)
			: m_buf(std::max(minBufCapacity(), maxEncodedSize<queryString, QueryString>(query)))
	{
		// m_queryState is initialised here.
		appendQueryString<queryString, QueryString>(std::forward<QueryString>(query));
	}

	template<typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	UrlBuilder(const char * const urlBase, Parts &&...paramParts)
			: UrlBuilder(urlBase, std::strlen(urlBase), paramParts...) {}

	template<typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	UrlBuilder(const char * const urlBase, const std::size_t n, Parts &&...paramParts)
			: m_buf(std::max(minBufCapacity(), n + maxEncodedSize<urlFirst, Parts...>(paramParts...)))
	{
		m_buf.append(urlBase, n);
		// m_queryState is initialised here.
		appendParams<urlFirst, Parts...>(std::forward<Parts>(paramParts)...);
	}

	template<typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	UrlBuilder(const afc::ConstStringRef urlBase, Parts &&...paramParts)
			: UrlBuilder(urlBase.value(), urlBase.size(), paramParts...) {}

	template<typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	UrlBuilder(const std::string &urlBase, Parts &&...paramParts)
			: UrlBuilder(urlBase.c_str(), urlBase.size(), paramParts...) {}

	template<typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	UrlBuilder(QueryOnly, Parts &&...paramParts)
			: m_buf(std::max(minBufCapacity(), maxEncodedSize<queryString, Parts...>(paramParts...)))
	{
		// m_queryState is initialised here.
		appendParams<queryString, Parts...>(std::forward<Parts>(paramParts)...);
	}

	UrlBuilder(UrlBuilder &&) = default;
	~UrlBuilder() = default;

	UrlBuilder &operator=(UrlBuilder &&) = default;

	template<typename QueryString, typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	bool query(QueryString &&queryPart)
	{
		if (m_queryState == queryNonEmpty) {
			return false;
		}

		const std::size_t estimatedEncodedSize = maxEncodedSize<unknown, QueryString>(queryPart);
		m_buf.reserve(m_buf.size() + estimatedEncodedSize);

		return appendQueryString<unknown, QueryString>(std::forward<QueryString>(queryPart));
	}

	template<typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	void params(Parts &&...parts)
	{
		const std::size_t estimatedEncodedSize = maxEncodedSize<unknown, Parts...>(parts...);
		m_buf.reserve(m_buf.size() + estimatedEncodedSize);

		appendParams<unknown, Parts...>(std::forward<Parts>(parts)...);
	}

	const char *data() const noexcept { return m_buf.data(); }
	const char *c_str() const noexcept { return m_buf.c_str(); }
	const std::size_t size() const noexcept { return m_buf.size(); }
private:
	enum QueryAppendMode
	{
		// A URL is being built; no query exists.
		urlFirst,
		/* A URL is being built or a query string is appended;
		 * it is unknown if the append to the query is first.
		 */
		unknown,
		// A query string is being built.
		queryString,
		// The append to query is known to be not first.
		notFirst
	};

	template<QueryAppendMode mode, typename Part, typename... Parts>
	static constexpr std::size_t maxEncodedSize(const Part &part, const Parts &...parts) noexcept
	{
		// One of {'?', '&', '='} is counted for all non-first and first non-query param elements.
		return (mode == queryString ? part.maxEncodedSize() : part.maxEncodedSize() + 1) +
				maxEncodedSize<notFirst, Parts...>(parts...);
	}

	/* Terminates the maxEncodedSize() template recusion.
	 * No '?' is appended even if there are no parameters at all.
	 */
	template<QueryAppendMode mode>
	static constexpr std::size_t maxEncodedSize() noexcept { return 0; }

	template<QueryAppendMode mode, typename QueryString,
			typename = typename std::enable_if<queryFormat == plain, QueryString>::type>
	bool appendQueryString(QueryString &&queryPart)
	{
		static_assert(mode != notFirst, "Mode 'notFirst' is not applicable for query strings in the plain format.");

		register afc::FastStringBuffer<char>::Tail p = m_buf.borrowTail();
		if (mode == urlFirst || (mode == unknown && m_queryState == queryEmptyUrl)) {
			*p++ = '?';
		}
		p = queryPart.appendTo(p);
		m_buf.returnTail(p);
		m_queryState = queryNonEmpty;
		return true;
	}

	template<QueryAppendMode mode, typename... Parts,
			typename = typename std::enable_if<queryFormat == webForm && sizeof...(Parts) >= 0>::type>
	void appendParams(Parts &&...parts) noexcept
	{
		static_assert(sizeof...(Parts) > 0, "No parameters are defined.");
		static_assert((sizeof...(Parts) % 2) == 0,
				"The number of URL parameter parts must be even for the web-form query.");

		appendParamName<mode, Parts...>(std::forward<Parts>(parts)...);
	}

	template<QueryAppendMode mode, typename ParamName, typename... Parts>
	void appendParamName(ParamName &&name, Parts &&...parts) noexcept
	{
		// It is guaranteed that there is enough capacity for all the parameters.
		switch (mode) {
		case urlFirst:
			m_buf.append('?');
			m_queryState = queryNonEmpty;
			break;
		case unknown:
			if (m_queryState != queryEmptyQueryString) {
				m_buf.append(m_queryState == queryNonEmpty ? '&' : '?');
			}
			// m_queryState is assigned in either case to produce less jumps.
			m_queryState = queryNonEmpty;
			break;
		case queryString:
			m_queryState = queryNonEmpty;
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
	template<QueryAppendMode mode>
	void appendParamName() noexcept {}

	template<typename ParamValue, typename... Parts>
	void appendParamValue(ParamValue &&value, Parts &&...parts) noexcept
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

	enum QueryState
	{
		queryNonEmpty = 0, // Assigned to zero since it is he most active state.
		queryEmptyUrl = 1,
		queryEmptyQueryString = 2
	};

	/* Many short urls have length less than 64 characters. Setting this value
	 * as the minimal capacity to minimise re-allocations.
	 *
	 * This function emulates normal inlinable constants.
	 */
	static constexpr std::size_t minBufCapacity() { return 64; };

	afc::FastStringBuffer<char> m_buf;
	QueryState m_queryState;
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
				uc == '-' || uc == '_' || uc == '.') {
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
