// PhysicsSystem.h

#pragma once

#include "ECS/Systems/ISystem.h"

#include <memory>

namespace NeneEngine::ECS
{

	// Bridges ECS physics components to a Jolt Physics world.
	// Entities with Transform + Collider become static bodies; adding a Rigidbody makes them dynamic.
	// Only root entities (no hierarchy parent) are simulated.
	class PhysicsSystem final : public ISystem
	{
	  public:
		static constexpr float FixedTimeStep = 1.0f / 60.0f;
		static constexpr int MaxStepsPerFrame = 5;

		PhysicsSystem();
		~PhysicsSystem() override;

		PhysicsSystem(const PhysicsSystem&) = delete;
		PhysicsSystem& operator=(const PhysicsSystem&) = delete;

		void Update(World& world, float deltaTime) override;

	  private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

} // namespace NeneEngine::ECS
