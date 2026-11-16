#pragma once

#include <EASTL/vector.h>
#include <glm/vec3.hpp>
#include <string>
#include <variant>

namespace NeneEngine
{

	using ScriptFieldValue = std::variant<bool, int, float, std::string, glm::vec3>;

	struct ScriptFieldDefinition
	{
		std::string name;
		ScriptFieldValue defaultValue;
	};

	using ScriptFieldSchema = eastl::vector<ScriptFieldDefinition>;

} // namespace NeneEngine
