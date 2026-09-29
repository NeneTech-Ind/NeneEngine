// RigidbodyComponent.h

#pragma once

#include <glm/glm.hpp>

namespace NeneEngine::ECS
{

	// Marks an entity with a ColliderComponent as a dynamic body. Colliders without a rigidbody are static.
	struct RigidbodyComponent
	{
		glm::vec3 velocity = {0.0f, 0.0f, 0.0f};
		// Constant extra acceleration applied on top of gravity every physics step.
		glm::vec3 acceleration = {0.0f, 0.0f, 0.0f};
		float mass = 1.0f;
		bool useGravity = true;
		bool freezeRotation = false;
	};

} // namespace NeneEngine::ECS
