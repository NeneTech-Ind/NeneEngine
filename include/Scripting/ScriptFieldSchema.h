#pragma once

#include <EASTL/vector.h>
#include <glm/vec3.hpp>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <variant>

namespace NeneEngine
{

	using ScriptFieldValue = std::variant<bool, int, float, std::string, glm::vec3>;

	struct ScriptFieldDefinition
	{
		std::string name;
		ScriptFieldValue defaultValue;
		std::optional<ScriptFieldValue> minimum;
		std::optional<ScriptFieldValue> maximum;
		eastl::vector<std::string> options;
	};

	inline bool IsValidScriptFieldValue(const ScriptFieldDefinition& field, const ScriptFieldValue& value)
	{
		if (value.index() != field.defaultValue.index()) return false;
		if (const auto* number = std::get_if<int>(&value))
			return (!field.minimum || *number >= std::get<int>(*field.minimum)) &&
			       (!field.maximum || *number <= std::get<int>(*field.maximum));
		if (const auto* number = std::get_if<float>(&value))
			return std::isfinite(*number) && (!field.minimum || *number >= std::get<float>(*field.minimum)) &&
			       (!field.maximum || *number <= std::get<float>(*field.maximum));
		if (const auto* text = std::get_if<std::string>(&value); text && !field.options.empty())
			return std::find(field.options.begin(), field.options.end(), *text) != field.options.end();
		if (const auto* vector = std::get_if<glm::vec3>(&value))
			return std::isfinite(vector->x) && std::isfinite(vector->y) && std::isfinite(vector->z);
		return true;
	}

	using ScriptFieldSchema = eastl::vector<ScriptFieldDefinition>;

} // namespace NeneEngine
