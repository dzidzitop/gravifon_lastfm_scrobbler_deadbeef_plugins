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

#ifdef NDEBUG
	#define logDebug(msg) static_cast<void>
#else
	#include <cstdio>
	#include <string>

	// stdout is flushed so that the message logged becomes visible immediately.
	#define logDebug(msg) std::printf("%s\n", static_cast<std::string>(msg).c_str()); std::fflush(stdout);
#endif
	inline void logError(std::string msg) { std::fprintf(stderr, "%s\n", msg.c_str()); std::fflush(stderr); }

#endif /* LOGGER_HPP_ */
