// DemoScene.h

#pragma once

#include "ECS/World.h"

#include <cstdint>
#include <filesystem>

namespace NeneEngine::DemoScene
{

	std::filesystem::path GetDefaultScenePath();
	std::filesystem::path GetDefaultSceneConfigPath();

	void Create(ECS::World& world, uint32_t width, uint32_t height);
	void LoadOrCreate(ECS::World& world, uint32_t width, uint32_t height,
	                  const std::filesystem::path& scenePath = GetDefaultScenePath(),
	                  const std::filesystem::path& sceneConfigPath = GetDefaultSceneConfigPath());

} // namespace NeneEngine::DemoScene
