// LocalAabb.h

#pragma once

#include <glm/vec3.hpp>

namespace NeneEngine
{
	struct LocalAabb
	{
		glm::vec3 min{0.0f};
		glm::vec3 max{0.0f};
	};
} // namespace NeneEngine
