// PlayerControllerSystem.h

#pragma once

#include "ECS/Systems/ISystem.h"

namespace NeneEngine
{
	class IInputHandler;
}

namespace NeneEngine::ECS
{

	// Drives rigidbodies with a PlayerControllerComponent by writing their velocity; PhysicsSystem simulates the rest.
	class PlayerControllerSystem final : public ISystem
	{
	  public:
		explicit PlayerControllerSystem(const IInputHandler& input) : m_input(input) {}

		void Update(World& world, float deltaTime) override;

	  private:
		const IInputHandler& m_input;
	};

} // namespace NeneEngine::ECS
