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
#include <stdio.h>

namespace
{
	class FileLock
	{
	public:
		FileLock(std::FILE * const file) noexcept : m_file(file) { flockfile(file); }
		~FileLock() { funlockfile(m_file); }
	private:
		std::FILE * const m_file;
	};
}

bool logInternal(const char *format, std::initializer_list<LogFunction> params,
		std::FILE * const dest)
{ FileLock fileLock(dest);
	auto paramPtr = params.begin();
	const char *start = format;
	bool escape = false;
	bool param = false;
	bool success;
	while (*format != '\0') {
		if (param) {
			if (*format == '}') {
				if (paramPtr == params.end() || !(*paramPtr)(dest)) {
					success = false;
					goto finish;
				}
				start = format + 1;
				++paramPtr;
				param = false;
			} else {
				// Invalid pattern.
				success = false;
				goto finish;
			}
		} else if (escape) {
			start = format;
			escape = false;
		} else if (*format == '\\') {
			if (!logText(start, format - start, dest)) {
				success = false;
				goto finish;
			}
			escape = true;
		} else if (*format == '{') {
			if (!logText(start, format - start, dest)) {
				success = false;
				goto finish;
			}
			param = true;
		}
		++format;
	}
	// Flushing plain text data and checking if there are too many arguments.
	success = logText(start, format - start, dest) && paramPtr == params.end();
finish:
	success &= (fputc('\n', dest) != EOF);
}
