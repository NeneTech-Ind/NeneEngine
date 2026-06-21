// MenuState.cpp

#include "GameStates/MenuState.h"
#include "Core/CustomLogger.h"

void NeneEngine::MenuState::OnEnter()
{
	NENE_LOG_INFO("MenuState entered");
}

void NeneEngine::MenuState::OnPause()
{
	NENE_LOG_INFO("MenuState paused");
}

void NeneEngine::MenuState::OnResume()
{
	NENE_LOG_INFO("MenuState resumed");
}

void NeneEngine::MenuState::OnExit()
{
	NENE_LOG_INFO("MenuState exited");
}

void NeneEngine::MenuState::Update(float /*dt*/)
{
	// TODO
}

void NeneEngine::MenuState::HandleInput()
{
	// TODO
}
