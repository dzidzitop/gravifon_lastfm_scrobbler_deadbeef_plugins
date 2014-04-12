/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2014 Dźmitry Laŭčuk

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
#ifndef PATHUTIL_HPP_
#define PATHUTIL_HPP_

#include <string>
#include <cassert>

// Removes trailing slash if it is not the only character in the path.
inline void appendToPath(std::string &path, const char *child)
{
	const bool needsSeparator = path.size() > 1 && path.back() != '/';
	const bool childHasSeparator = child[0] == '/';
	if (needsSeparator) {
		if (!childHasSeparator) {
			path += '/';
		}
	} else {
		if (childHasSeparator) {
			/* This works even if the path is an empty string.
			 * Child's heading separator is deleted.
			 */
			++child;
		}
	}
	path += child;
}

inline int getDataFilePath(const char * const shortFilePath, std::string &dest)
{
	assert(shortFilePath != nullptr);

	const char * const dataDir = getenv("XDG_DATA_HOME");
	if (dataDir != nullptr && dataDir[0] != '\0') {
		dest = dataDir;
	} else {
		// Trying to assign the default data dir ($HOME/.local/share/).
		const char * const homeDir = getenv("HOME");
		if (homeDir == nullptr || homeDir == '\0') {
			return 1;
		}
		dest = homeDir;
		appendToPath(dest, ".local/share");
	}
	appendToPath(dest, shortFilePath);
	return 0;
}

#endif /* PATHUTIL_HPP_ */
