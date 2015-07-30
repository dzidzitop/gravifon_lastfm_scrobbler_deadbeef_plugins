/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2015 Dźmitry Laŭčuk

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

#include <afc/FastStringBuffer.hpp>

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

inline void appendToPath(afc::FastStringBuffer<char> &path, const char *child, const std::size_t childSize)
{
	path.reserve(childSize + 1); // '/' and whole child are to be appended at mos.

	const bool needsSeparator = path.size() > 1 && *(path.end() - 1) != '/';
	const bool childHasSeparator = child[0] == '/';
	std::size_t effectiveChildSize = childSize;
	if (needsSeparator) {
		if (!childHasSeparator) {
			path.append('/');
		}
	} else {
		if (childHasSeparator) {
			/* This works even if the path is an empty string.
			 * Child's heading separator is deleted.
			 */
			++child;
			--effectiveChildSize;
		}
	}
	path.append(child, effectiveChildSize);
}

inline int getDataFilePath(const char * const shortFilePath, std::string &dest)
{
	assert(shortFilePath != nullptr);

	const char * const dataDir = getenv("XDG_DATA_HOME");
	if (dataDir != nullptr && dataDir[0] != '\0') {
		dest.append(dataDir);
	} else {
		// Trying to assign the default data dir ($HOME/.local/share/).
		const char * const homeDir = getenv("HOME");
		if (homeDir == nullptr || homeDir == '\0') {
			return 1;
		}
		const std::size_t homeDirSize = std::strlen(homeDir);
		dest.append(homeDir, homeDirSize);
		// TODO Optimise this call w.r.t. memory realloc
		appendToPath(dest, ".local/share");
	}
	appendToPath(dest, shortFilePath);
	return 0;
}

#endif /* PATHUTIL_HPP_ */
