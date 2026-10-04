// PauseState.cpp

#include "GameStates/PauseState.h"
#include "Core/NeneLogger.h"
#include "GameStates/GameStateMachine.h"
#include "Input/IInputHandler.h"
#include "Input/InputActions.h"

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
	const IInputHandler& input = m_context.input;

	if (input.IsActionPressed(InputActions::Pause)) m_context.stateMachine.PopState();

	if (input.IsActionPressed(InputActions::Quit) && m_context.requestShutdown) m_context.requestShutdown();
}
