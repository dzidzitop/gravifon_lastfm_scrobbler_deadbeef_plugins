/* gravifon_scrobbler - an audio track scrobbler to Gravifon plugin to the audio player DeaDBeeF.
Copyright (C) 2013-2016 Dźmitry Laŭčuk

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

#include <algorithm>

#include <afc/FastStringBuffer.hpp>
#include <afc/StringRef.hpp>

// Removes trailing slash if it is not the only character in the path.
// @param pathLastChar the last char in the destination path to append to. '\0' indicates an empty path.
template<typename Iterator>
inline Iterator appendToPath(const char pathLastChar, const char *child, const std::size_t childSize, Iterator dest)
{
	const bool needsSeparator = pathLastChar != '\0' && pathLastChar != '/';
	const bool childHasSeparator = child[0] == '/';
	std::size_t effectiveChildSize = childSize;
	if (needsSeparator) {
		if (!childHasSeparator) {
			*dest = '/';
			++dest;
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
	return std::copy_n(child, effectiveChildSize, dest);
}

template<typename Iterator>
inline Iterator appendToPath(const char pathLastChar, afc::ConstStringRef str, Iterator dest)
{
	return appendToPath(pathLastChar, str.value(), str.size(), dest);
}

/**
 * Constructs a path to a given data file depending on environment configuration and
 * appends it to {@code dest}.
 * The following rules are used:
 * - if {@code XDG_DATA_HOME} is defined then the path is @{code $XDG_DATA_HOME/$shortFilePath};
 * - otherwise the path is {@code $HOME/.local/share/$shortFilePath}.
 *
 * @return {@code true} if data file path is built successfully; {@code false} is returned
 * 		otherwise. {@code dest} is not modified in the latter case.
 */
inline bool getDataFilePath(afc::ConstStringRef shortFilePath,
		afc::FastStringBuffer<char, afc::AllocMode::accurate> &dest)
{
	using afc::operator"" _s;

	const char * const dataDir = getenv("XDG_DATA_HOME");
	if (dataDir != nullptr && dataDir[0] != '\0') {
		const std::size_t dataDirSize = std::strlen(dataDir);
		dest.reserve(dest.size() + dataDirSize + 1 + shortFilePath.size()); // {dataDir}/{shortFilePath} in the worst case.
		auto p = std::copy_n(dataDir, dataDirSize, dest.borrowTail());
		p = appendToPath(dataDirSize == 0 ? '\0' : *(p - 1), shortFilePath, p);
		dest.returnTail(p);
		return true;
	} else {
		// Trying to assign the default data dir ($HOME/.local/share/).
		const char * const homeDir = getenv("HOME");
		if (homeDir == nullptr || homeDir[0] == '\0') {
			return false;
		}
		const std::size_t homeDirSize = std::strlen(homeDir);
		dest.reserve(dest.size() + homeDirSize + 1 + ".local/share"_s.size() + 1 + shortFilePath.size()); // {homeDir}/.local/share/{shortFilePath} in the worst case.
		auto p = std::copy_n(homeDir, homeDirSize, dest.borrowTail());
		p = appendToPath(homeDirSize == 0 ? '\0' : *(p - 1), ".local/share"_s, p);
		p = appendToPath('e', shortFilePath, p);
		dest.returnTail(p);
		return true;
	}
}

#endif /* PATHUTIL_HPP_ */
