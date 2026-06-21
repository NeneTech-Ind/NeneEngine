// PlayState.cpp

#include "GameStates/PlayState.h"
#include "App/GameStateMachine.h"
#include "App/NeneEngineApp.h"
#include "Core/CustomLogger.h"
#include "Input/InputActions.h"
#include "Input/InputManager.h"
#include "GameStates/PauseState.h"

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
	const InputManager& input = m_context.app.GetInputManager();
	if (input.IsActionPressed(InputActions::Pause))
		m_context.stateMachine.PushState(eastl::make_unique<PauseState>(m_context));
}
