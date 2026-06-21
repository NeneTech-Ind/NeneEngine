#pragma once

#include <cstdint>
#include <string>

namespace NeneEngine
{

	class GameStateMachine;
	class NeneEngineApp;

	namespace ECS
	{
		class World;
	}

	struct AppStateContext
	{
		NeneEngineApp& app;
		ECS::World& world;
		GameStateMachine& stateMachine;
	};

	class IGameState
	{
	  public:
		explicit IGameState(const AppStateContext& context) : m_context(context) {}

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
		AppStateContext m_context;
	};

} // namespace NeneEngine
