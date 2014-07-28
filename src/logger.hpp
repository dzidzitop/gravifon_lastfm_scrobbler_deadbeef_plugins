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
#include <utility>

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

inline bool logPrint(afc::ConstStringRef s, FILE * const dest) noexcept
{
	return logText(s.value(), s.size(), dest);
}

inline bool logPrint(const char * const s, FILE * const dest) noexcept
{
	return std::fputs(s, dest) >= 0;
}

inline bool logError(const char *p)
{
	const char *start = p;
	bool escape = false;
	while (*p != '\0') {
		if (escape) {
			start = p;
			escape = false;
		}
		if (*p == '\\') {
			if (!logText(start, p - start, stderr)) {
				return false;
			}
			escape = true;
		}
		if (*p == '{') {
			return false;
		}
		++p;
	}
	// success.
	return logText(start, p - start, stderr);
}


template<typename Arg, typename... Args>
inline bool logError(const char *p, Arg&& arg, Args&&... args)
{
	const char *start = p;
	bool escape = false;
	bool param = false;
	while (*p != '\0') {
		if (param) {
			if (*p == '}') {
				// TODO handle error.
				return logPrint(arg, stderr) && logError(p + 1, std::forward<Args>(args)...);
			} else {
				// Invalid pattern.
				return false;
			}
		} else if (escape) {
			start = p;
			escape = false;
		} else if (*p == '\\') {
			// TODO handle error.
			if (!logText(start, p - start, stderr)) {
				return false;
			}
			escape = true;
		} else if (*p == '{') {
			// TODO handle error.
			if (!logText(start, p - start, stderr)) {
				return false;
			}
			param = true;
		}
		++p;
	}
	return false;
}

#endif /* LOGGER_HPP_ */
