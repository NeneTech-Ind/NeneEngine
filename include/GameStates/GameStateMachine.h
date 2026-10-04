#pragma once

#include "GameStates/IGameState.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <memory>

namespace NeneEngine
{

	class GameStateMachine
	{
	  public:
		GameStateMachine() = default;
		~GameStateMachine();

		void ChangeState(eastl::unique_ptr<IGameState> newState);

		void PushState(eastl::unique_ptr<IGameState> newState);

		void PopState();
		void Clear();

		void Update(float deltaTime);

		void HandleInput();

		bool IsEmpty() const { return m_states.empty(); }

		IGameState* GetCurrentState() const { return m_states.empty() ? nullptr : m_states.back().get(); }

	  private:
		eastl::vector<eastl::unique_ptr<IGameState>> m_states;
	};

} // namespace NeneEngine
