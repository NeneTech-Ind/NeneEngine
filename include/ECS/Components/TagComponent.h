// TagComponent.h

#pragma once

#include <set>
#include <string>
#include <string_view>

namespace NeneEngine::ECS
{

	struct TagComponent
	{
		std::string name;
		std::set<std::string> tags;

		TagComponent() = default;
		explicit TagComponent(std::string_view n) : name(n) {}
	};

} // namespace NeneEngine::ECS
