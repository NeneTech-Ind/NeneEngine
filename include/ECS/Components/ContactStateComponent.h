// ContactStateComponent.h

#pragma once

#include <cstdint>

namespace NeneEngine::ECS
{

	// Runtime-only contact summary written by PhysicsSystem after each simulation step.
	struct ContactStateComponent
	{
		uint32_t contactCount = 0;
		// True while the body rests on a surface whose normal points mostly up.
		bool isGrounded = false;
	};

} // namespace NeneEngine::ECS
