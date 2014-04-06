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
#ifndef JSONUTIL_HPP_
#define JSONUTIL_HPP_

#include <jsoncpp/json/value.h>

static const Json::ValueType nullValue = Json::ValueType::nullValue;
static const Json::ValueType objectValue = Json::ValueType::objectValue;
static const Json::ValueType arrayValue = Json::ValueType::arrayValue;
static const Json::ValueType stringValue = Json::ValueType::stringValue;
static const Json::ValueType intValue = Json::ValueType::intValue;
static const Json::ValueType booleanValue = Json::ValueType::booleanValue;

inline bool isType(const Json::Value &val, const Json::ValueType type)
{
	return val.type() == type;
}

#endif /* JSONUTIL_HPP_ */
