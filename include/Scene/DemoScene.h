// DemoScene.h

#pragma once

#include "ECS/World.h"

#include <cstdint>
#include <filesystem>

namespace NeneEngine::DemoScene
{

	std::filesystem::path DefaultScenePath();
	std::filesystem::path DefaultSceneConfigPath();

	void Create(ECS::World& world, uint32_t width, uint32_t height);
	void LoadOrCreate(ECS::World& world, uint32_t width, uint32_t height,
	                  const std::filesystem::path& scenePath = DefaultScenePath(),
	                  const std::filesystem::path& sceneConfigPath = DefaultSceneConfigPath());

} // namespace NeneEngine::DemoScene
