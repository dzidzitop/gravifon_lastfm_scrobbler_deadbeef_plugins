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

#include "logger.hpp"

#include <cassert>
#include <cstdint>

#include <afc/builtin.hpp>

bool logInternal(const char *format, std::initializer_list<Printer *> params, std::FILE * const dest)
{ FileLock fileLock(dest);
	auto paramPtr = params.begin();
	const char *start = format;

	bool success;
	while (likely(*format != '\0')) {
		if (unlikely(*format == '#')) {
			if (!logText(start, format - start, dest)) {
				success = false;
				goto finish;
			}
			if (unlikely(paramPtr == params.end() || !(**paramPtr)(dest))) {
				success = false;
				goto finish;
			}
			++paramPtr;
			start = format + 1;
		} else if (unlikely(*format == '\\')) { // Two chars are processed at once.
			if (unlikely(!logText(start, format - start, dest))) {
				success = false;
				goto finish;
			}
			++format;
			if (unlikely(*format == '\0')) {
				// Premature end of sequence.
				success = false;
				goto finish;
			}
			// The character escaped becomes the first character of the new text sequence.
			start = format;
		} else {
			// Nothing to do, the end of the text sequence is incremented in the end of the loop.
		}

		++format;
	}
	// Flushing plain text data and checking if there are too many arguments.
	success = likely(logText(start, format - start, dest) && paramPtr == params.end());
finish:
	success &= (fputc('\n', dest) != EOF);
	return success;
}
