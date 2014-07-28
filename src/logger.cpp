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

bool logError(const char *p)
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
	return logText(start, p - start, stderr) &&  std::fputc('\n', stderr) != EOF;
}
