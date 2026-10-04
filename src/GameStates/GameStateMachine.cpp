// GameStateMachine.cpp

#include "GameStates/GameStateMachine.h"
#include "Core/NeneLogger.h"

namespace NeneEngine
{

	GameStateMachine::~GameStateMachine()
	{
		Clear();
	}

	void GameStateMachine::ChangeState(eastl::unique_ptr<IGameState> newState)
	{
		while (!m_states.empty())
		{
			m_states.back()->OnExit();
			NENE_LOG_INFO("Popped state during ChangeState: {}", typeid(*m_states.back()).name());
			m_states.pop_back();
		}

		PushState(eastl::move(newState));
	}

	void GameStateMachine::PushState(eastl::unique_ptr<IGameState> newState)
	{
		if (!m_states.empty())
		{
			m_states.back()->OnPause();
		}

		m_states.push_back(eastl::move(newState));

		if (!m_states.empty())
		{
			m_states.back()->OnEnter();
			NENE_LOG_INFO("Pushed state: {}", typeid(*m_states.back()).name());
		}
	}

	void GameStateMachine::PopState()
	{
		if (m_states.empty()) return;

		m_states.back()->OnExit();
		NENE_LOG_INFO("Popped state: {}", typeid(*m_states.back()).name());

		m_states.pop_back();

		if (!m_states.empty())
		{
			m_states.back()->OnResume();
		}
	}

	void GameStateMachine::Clear()
	{
		while (!m_states.empty())
		{
			m_states.back()->OnExit();
			NENE_LOG_INFO("Popped state during Clear: {}", typeid(*m_states.back()).name());
			m_states.pop_back();
		}
	}

	void GameStateMachine::Update(float deltaTime)
	{
		// Overlay states can keep lower states ticking by returning true from IsPausing().
		for (int i = static_cast<int>(m_states.size()) - 1; i >= 0; --i)
		{
			auto& state = m_states[static_cast<size_t>(i)];

			state->Update(deltaTime);

			if (!state->IsPausing())
			{
				break;
			}
		}
	}

	void GameStateMachine::HandleInput()
	{
		if (!m_states.empty())
		{
			m_states.back()->HandleInput();
		}
	}

} // namespace NeneEngine
