// DebugDrawControlSystem.h

#pragma once

#include "ECS/Systems/ISystem.h"

namespace NeneEngine
{
	class IInputHandler;
}

namespace NeneEngine::ECS
{

	class DebugDrawControlSystem final : public ISystem
	{
	  public:
		explicit DebugDrawControlSystem(const IInputHandler& input) : m_input(input) {}

		void Update(World& world, float deltaTime) override;

	  private:
		const IInputHandler& m_input;
	};

} // namespace NeneEngine::ECS
