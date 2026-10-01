// EASTLStdHash.h

#pragma once

#include <EASTL/functional.h>
#include <functional>
#include <string>
#include <string_view>
#include <typeindex>

namespace eastl
{

	template <> struct hash<std::string>
	{
		size_t operator()(const std::string& value) const { return std::hash<std::string>{}(value); }
	};

	template <> struct hash<std::string_view>
	{
		size_t operator()(std::string_view value) const { return std::hash<std::string_view>{}(value); }
	};

	template <> struct hash<std::type_index>
	{
		size_t operator()(const std::type_index& value) const { return value.hash_code(); }
	};

} // namespace eastl
