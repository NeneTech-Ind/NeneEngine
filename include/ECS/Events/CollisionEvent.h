#pragma once

#include "ECS/Entity.h"

#include <glm/glm.hpp>

namespace NeneEngine::ECS
{

	// Published once when two physics bodies start touching.
	struct CollisionEvent
	{
		Entity entityA = NullEntity;
		Entity entityB = NullEntity;
		glm::vec3 contactPoint = {0.0f, 0.0f, 0.0f};
		// Points from entityA towards entityB.
		glm::vec3 normal = {0.0f, 0.0f, 0.0f};
	};

} // namespace NeneEngine::ECS
