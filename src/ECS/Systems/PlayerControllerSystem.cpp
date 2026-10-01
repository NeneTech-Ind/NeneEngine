// PlayerControllerSystem.cpp

#include "ECS/Systems/PlayerControllerSystem.h"

#include "ECS/Components/ContactStateComponent.h"
#include "ECS/Components/PlayerControllerComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/World.h"
#include "Input/IInputHandler.h"
#include "Input/InputActions.h"

#include <glm/geometric.hpp>

namespace NeneEngine::ECS
{

	void PlayerControllerSystem::Update(World& world, float /*deltaTime*/)
	{
		glm::vec3 direction{0.0f};
		if (m_input.IsActionActive(InputActions::PlayerMoveForward)) direction.z -= 1.0f;
		if (m_input.IsActionActive(InputActions::PlayerMoveBackward)) direction.z += 1.0f;
		if (m_input.IsActionActive(InputActions::PlayerMoveLeft)) direction.x -= 1.0f;
		if (m_input.IsActionActive(InputActions::PlayerMoveRight)) direction.x += 1.0f;
		if (glm::dot(direction, direction) > 0.0f) direction = glm::normalize(direction);

		const bool jumpPressed = m_input.IsActionPressed(InputActions::PlayerJump);

		auto view = world.GetRegistry().view<const PlayerControllerComponent, RigidbodyComponent>();
		for (auto [entity, controller, rigidbody] : view.each())
		{
			// Horizontal velocity is fully player-driven; vertical velocity stays under gravity's control.
			rigidbody.velocity.x = direction.x * controller.moveSpeed;
			rigidbody.velocity.z = direction.z * controller.moveSpeed;

			const auto* contactState = world.GetComponent<ContactStateComponent>(entity);
			if (jumpPressed && contactState != nullptr && contactState->isGrounded)
				rigidbody.velocity.y = controller.jumpSpeed;
		}
	}

} // namespace NeneEngine::ECS
