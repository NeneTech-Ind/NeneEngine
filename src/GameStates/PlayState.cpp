// PlayState.cpp

#include "GameStates/PlayState.h"
#include "Core/NeneLogger.h"
#include "ECS/DebugDrawSettings.h"
#include "ECS/World.h"
#include "GameStates/GameStateMachine.h"
#include "GameStates/PauseState.h"
#include "Input/IInputHandler.h"
#include "Input/InputActions.h"

void NeneEngine::PlayState::OnEnter()
{
	NENE_LOG_INFO("PlayState entered");
}

void NeneEngine::PlayState::OnPause()
{
	NENE_LOG_INFO("PlayState paused");
}

void NeneEngine::PlayState::OnResume()
{
	NENE_LOG_INFO("PlayState resumed");
}

void NeneEngine::PlayState::OnExit()
{
	NENE_LOG_INFO("PlayState exited");
}

void NeneEngine::PlayState::Update(float dt)
{
	m_context.world.Update(dt);
}

void NeneEngine::PlayState::HandleInput()
{
	const IInputHandler& input = m_context.input;
	if (input.IsActionPressed(InputActions::Pause))
		m_context.stateMachine.PushState(eastl::make_unique<PauseState>(m_context));

	if (input.IsActionPressed(InputActions::ToggleDebugDraw))
	{
		auto& debugDraw = m_context.world.GetRegistry().ctx().emplace<ECS::DebugDrawSettings>();
		debugDraw.drawColliders = !debugDraw.drawColliders;
		NENE_LOG_INFO("Collider debug draw {}", debugDraw.drawColliders ? "enabled" : "disabled");
	}
}
