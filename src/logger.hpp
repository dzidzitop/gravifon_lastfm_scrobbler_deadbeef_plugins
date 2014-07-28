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
#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

#include <afc/number.h>
#include <afc/StringRef.hpp>

#ifdef NDEBUG
	#define logDebug(msg) static_cast<void>(0)
#else
	// stdout is flushed so that the message logged becomes visible immediately.
	#define logDebug(msg) std::printf("%s\n", static_cast<std::string>(msg).c_str()); std::fflush(stdout);
#endif

inline bool logText(const char * const s, const std::size_t n, FILE * const dest) noexcept
{
	return std::fwrite(s, sizeof(char), n, dest) == n;
}

template<typename T>
inline typename std::enable_if<std::is_integral<T>::value, bool>::type logPrint(T value, FILE * const dest) noexcept
{
	char buf[afc::maxPrintedSize<T, 10>()];
	char *end = afc::printNumber<T, 10>(value, buf);
	return logText(buf, end - buf, dest);
}

inline bool logPrint(afc::ConstStringRef s, FILE * const dest) noexcept
{
	return logText(s.value(), s.size(), dest);
}

inline bool logPrint(const std::string &s, FILE * const dest) noexcept
{
	return logText(s.data(), s.size(), dest);
}

inline bool logPrint(const char * const s, FILE * const dest) noexcept
{
	return std::fputs(s, dest) >= 0;
}

inline bool logPrint(const std::pair<const char *, const char *> &s, FILE * const dest) noexcept
{
	return logText(s.first, std::size_t(s.second - s.first), dest);
}

template<typename T>
inline bool logErrorMsg(const T &message)
{
	return logPrint(message, stderr) && std::fputc('\n', stderr) != EOF;
}

bool logError(const char *p);

template<typename Arg, typename... Args>
bool logError(const char *p, Arg&& arg, Args&&... args)
{
	const char *start = p;
	bool escape = false;
	bool param = false;
	while (*p != '\0') {
		if (param) {
			if (*p == '}') {
				return logPrint(arg, stderr) && logError(p + 1, std::forward<Args>(args)...);
			} else {
				// Invalid pattern.
				return false;
			}
		} else if (escape) {
			start = p;
			escape = false;
		} else if (*p == '\\') {
			if (!logText(start, p - start, stderr)) {
				return false;
			}
			escape = true;
		} else if (*p == '{') {
			if (!logText(start, p - start, stderr)) {
				return false;
			}
			param = true;
		}
		++p;
	}
	// There are too many arguments.
	return false;
}

#endif /* LOGGER_HPP_ */
