#pragma once

#include <glm/vec3.hpp>
#include <map>
#include <string>
#include <variant>

namespace NeneEngine::ECS
{

	using ScriptFieldValue = std::variant<bool, int, float, std::string, glm::vec3>;

	struct ScriptComponent
	{
		std::string scriptPath;
		std::map<std::string, ScriptFieldValue> fields;
	};

}
