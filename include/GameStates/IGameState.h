#pragma once

#include <EASTL/functional.h>
#include <cstdint>
#include <string>

namespace NeneEngine
{

	class GameStateMachine;
	class IInputHandler;

	namespace ECS
	{
		class World;
	}

	struct GameStateContext
	{
		const IInputHandler& input;
		ECS::World& world;
		GameStateMachine& stateMachine;
		eastl::function<void()> requestShutdown;
	};

	class IGameState
	{
	  public:
		explicit IGameState(const GameStateContext& context) : m_context(context) {}

		virtual ~IGameState() = default;

		virtual void OnEnter() = 0;
		virtual void OnPause() {}
		virtual void OnResume() {}

		virtual void OnExit() = 0;

		virtual void Update(float deltaTime) = 0;

		virtual void HandleInput() = 0;

		virtual bool IsTransparent() const { return false; }
		virtual bool IsPausing() const { return false; }

	  protected:
		GameStateContext m_context;
	};

} // namespace NeneEngine
