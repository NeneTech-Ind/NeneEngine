// CameraControllerSystem.cpp

#include "ECS/Systems/CameraControllerSystem.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Input/InputActions.h"
#include "Input/IInputHandler.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <glm/gtc/quaternion.hpp>

namespace NeneEngine::ECS
{
	namespace
	{
		constexpr glm::vec3 kLocalForward{0.0f, 0.0f, -1.0f};
		constexpr glm::vec3 kLocalUp{0.0f, 1.0f, 0.0f};
	}

	void CameraControllerSystem::Update(World& world, float deltaTime)
	{
		auto view = world.GetRegistry().view<TransformComponent, const CameraComponent, CameraControllerComponent>();

		for (auto entity : view)
		{
			if (m_controlledCamera != NullEntity && entity != m_controlledCamera) continue;

			const auto& camera = view.get<CameraComponent>(entity);
			if (m_controlledCamera == NullEntity && !camera.isPrimary) continue;

			auto& transform = view.get<TransformComponent>(entity);
			auto& controller = view.get<CameraControllerComponent>(entity);

			const bool isFlying =
			    !controller.rotateWithRightMouse || m_input.IsActionActive(InputActions::LookModifier);
			const glm::vec2 mouseDelta = m_input.GetMouseDelta();

			if (isFlying && glm::dot(mouseDelta, mouseDelta) > 0.0f)
			{
				// Store yaw/pitch explicitly to avoid extracting Euler angles from the camera quaternion every frame.
				controller.yawRadians -= mouseDelta.x * controller.lookSensitivity;
				controller.pitchRadians -= mouseDelta.y * controller.lookSensitivity;
				controller.pitchRadians =
				    std::clamp(controller.pitchRadians, -controller.maxPitchRadians, controller.maxPitchRadians);

				transform.rotation =
				    glm::normalize(glm::angleAxis(controller.yawRadians, glm::vec3{0.0f, 1.0f, 0.0f}) *
				                   glm::angleAxis(controller.pitchRadians, glm::vec3{1.0f, 0.0f, 0.0f}));
			}

			glm::vec3 direction{0.0f};

			if (m_input.IsActionActive(InputActions::MoveForward)) direction.z -= 1.0f;
			if (m_input.IsActionActive(InputActions::MoveBackward)) direction.z += 1.0f;
			if (m_input.IsActionActive(InputActions::MoveLeft)) direction.x -= 1.0f;
			if (m_input.IsActionActive(InputActions::MoveRight)) direction.x += 1.0f;
			if (m_input.IsActionActive(InputActions::MoveUp)) direction.y += 1.0f;
			if (m_input.IsActionActive(InputActions::MoveDown)) direction.y -= 1.0f;

			if (isFlying && glm::dot(direction, direction) > 0.0f)
			{
				const bool isSprinting = m_input.IsActionActive(InputActions::Sprint);
				const float moveSpeed =
				    isSprinting ? controller.moveSpeed * controller.sprintMultiplier : controller.moveSpeed;
				const glm::vec3 worldDirection = transform.rotation * glm::normalize(direction);
				transform.position += worldDirection * moveSpeed * deltaTime;
			}

			auto* mutableCamera = world.GetComponent<CameraComponent>(entity);
			if (mutableCamera != nullptr)
			{
				mutableCamera->forward = glm::normalize(transform.rotation * kLocalForward);
				mutableCamera->up = glm::normalize(transform.rotation * kLocalUp);
			}

			return;
		}
	}

} // namespace NeneEngine::ECS
