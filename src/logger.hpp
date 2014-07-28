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
#include <functional>
#include <initializer_list>
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

typedef std::function<bool (std::FILE *)> LogFunction;

bool logInternal(const char *format, std::initializer_list<LogFunction> params, FILE *dest);

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

inline LogFunction logFunction(const LogFunction &val) noexcept { return val; }
inline LogFunction logFunction(LogFunction &&val) noexcept { return std::move(val); }

template<typename T>
inline LogFunction logFunction(const T &val) noexcept
{
	return [&](FILE * const dest) -> bool { return logPrint(val, dest); };
}

template<typename... Args>
bool logError(const char *format, Args&&... args)
{
	return logInternal(format, {logFunction(std::forward<Args>(args))...}, stderr);
}

#endif /* LOGGER_HPP_ */
