// PrimitiveControlSystem.cpp

#include "ECS/Systems/PrimitiveControlSystem.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Input/InputActions.h"
#include "Input/IInputHandler.h"

#include <algorithm>
#include <glm/common.hpp>
#include <glm/gtc/quaternion.hpp>
#include <iterator>

namespace NeneEngine::ECS
{

	namespace
	{
		constexpr float kScaleLevels[] = {1.0f, 1.5f, 2.0f, 2.5f, 3.0f};
	}

	void PrimitiveControlSystem::Update(World& world, float deltaTime)
	{
		auto view = world.GetRegistry().view<TransformComponent, PrimitiveControlComponent>();
		for (auto entity : view)
		{
			auto& transform = view.get<TransformComponent>(entity);
			auto& control = view.get<PrimitiveControlComponent>(entity);

			glm::vec2 moveAxis{0.0f, 0.0f};
			if (m_input.IsActionActive(InputActions::PrimitiveMoveLeft)) moveAxis.x -= 1.0f;
			if (m_input.IsActionActive(InputActions::PrimitiveMoveRight)) moveAxis.x += 1.0f;
			if (m_input.IsActionActive(InputActions::PrimitiveMoveDown)) moveAxis.y -= 1.0f;
			if (m_input.IsActionActive(InputActions::PrimitiveMoveUp)) moveAxis.y += 1.0f;

			if (glm::dot(moveAxis, moveAxis) > 0.0f)
			{
				const glm::vec2 normalizedAxis = glm::normalize(moveAxis);
				transform.position.x += normalizedAxis.x * control.moveSpeed * deltaTime;
				transform.position.y += normalizedAxis.y * control.moveSpeed * deltaTime;
			}

			if (m_input.IsActionPressed(InputActions::ScaleStep))
			{
				control.currentScaleLevel = (control.currentScaleLevel + 1) % std::size(kScaleLevels);
				control.targetScale = glm::vec3(kScaleLevels[control.currentScaleLevel]);
			}

			if (m_input.IsActionPressed(InputActions::RotateStep))
				control.targetRotationRadians += control.rotationStepRadians;

			// Clicks update intent; smoothing makes the visible transform interpolate toward that target.
			const float scaleAlpha = std::clamp(control.scaleSmoothing * deltaTime, 0.0f, 1.0f);
			transform.scale = glm::mix(transform.scale, control.targetScale, scaleAlpha);

			const glm::quat targetRotation = glm::angleAxis(control.targetRotationRadians, glm::vec3{0.0f, 0.0f, 1.0f});
			const float rotationAlpha = std::clamp(control.rotationSmoothing * deltaTime, 0.0f, 1.0f);
			transform.rotation = glm::normalize(glm::slerp(transform.rotation, targetRotation, rotationAlpha));
		}
	}

} // namespace NeneEngine::ECS
