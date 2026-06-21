// PauseState.cpp

#include "GameStates/PauseState.h"
#include "App/GameStateMachine.h"
#include "App/NeneEngineApp.h"
#include "Core/CustomLogger.h"
#include "Input/InputActions.h"
#include "Input/InputManager.h"

void NeneEngine::PauseState::OnEnter()
{
	NENE_LOG_INFO("PauseState entered");
}

void NeneEngine::PauseState::OnPause()
{
	NENE_LOG_INFO("PauseState paused");
}

void NeneEngine::PauseState::OnResume()
{
	NENE_LOG_INFO("PauseState resumed");
}

void NeneEngine::PauseState::OnExit()
{
	NENE_LOG_INFO("PauseState exited");
}

void NeneEngine::PauseState::Update(float /*dt*/) {}

void NeneEngine::PauseState::HandleInput()
{
	const InputManager& input = m_context.app.GetInputManager();

	if (input.IsActionPressed(InputActions::Pause)) m_context.stateMachine.PopState();

	if (input.IsActionPressed(InputActions::Quit)) m_context.app.RequestShutdown();
}
