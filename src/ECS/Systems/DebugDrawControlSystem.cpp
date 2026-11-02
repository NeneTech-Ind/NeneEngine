// DebugDrawControlSystem.cpp

#include "ECS/Systems/DebugDrawControlSystem.h"

#include "Core/NeneLogger.h"
#include "ECS/DebugDrawSettings.h"
#include "ECS/World.h"
#include "Input/IInputHandler.h"
#include "Input/InputActions.h"

namespace NeneEngine::ECS
{

	void DebugDrawControlSystem::Update(World& world, float /*deltaTime*/)
	{
		if (!m_input.IsActionPressed(InputActions::ToggleDebugDraw)) return;

		auto& debugDraw = world.GetRegistry().ctx().emplace<DebugDrawSettings>();
		debugDraw.drawColliders = !debugDraw.drawColliders;
		NENE_LOG_INFO("Collider debug draw {}", debugDraw.drawColliders ? "enabled" : "disabled");
	}

} // namespace NeneEngine::ECS
