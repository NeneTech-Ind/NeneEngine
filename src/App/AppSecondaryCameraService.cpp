// AppSecondaryCameraService.cpp

#include "App/AppSecondaryCameraService.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"

namespace NeneEngine
{
	std::vector<ECS::Entity> AppSecondaryCameraService::CreateAdditionalWindowCameras(
	    ECS::World& world, ECS::Entity primaryCameraEntity, size_t count, uint32_t width, uint32_t height) const
	{
		std::vector<ECS::Entity> secondaryCameraEntities;
		secondaryCameraEntities.reserve(count);

		if (primaryCameraEntity == ECS::NullEntity) return secondaryCameraEntities;

		const auto* primaryTransform = world.GetComponent<ECS::TransformComponent>(primaryCameraEntity);
		const auto* primaryCamera = world.GetComponent<ECS::CameraComponent>(primaryCameraEntity);
		const auto* primaryController = world.GetComponent<ECS::CameraControllerComponent>(primaryCameraEntity);
		if (primaryTransform == nullptr || primaryCamera == nullptr || primaryController == nullptr)
			return secondaryCameraEntities;

		for (size_t cameraIndex = 0; cameraIndex < count; ++cameraIndex)
		{
			const ECS::Entity secondaryCameraEntity =
			    world.CreateEntity("SecondaryCamera" + std::to_string(cameraIndex + 1));
			auto& secondaryTransform = world.AddComponent<ECS::TransformComponent>(secondaryCameraEntity);
			secondaryTransform = *primaryTransform;
			secondaryTransform.position.x += 2.0f * static_cast<float>(cameraIndex + 1);

			auto& secondaryCamera = world.AddComponent<ECS::CameraComponent>(secondaryCameraEntity);
			secondaryCamera = *primaryCamera;
			secondaryCamera.aspectRatio = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
			secondaryCamera.isPrimary = false;

			auto& secondaryController = world.AddComponent<ECS::CameraControllerComponent>(secondaryCameraEntity);
			secondaryController = *primaryController;

			secondaryCameraEntities.push_back(secondaryCameraEntity);
		}

		return secondaryCameraEntities;
	}

} // namespace NeneEngine
