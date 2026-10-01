#pragma once

#include "ECS/World.h"

#include <EASTL/optional.h>
#include <EASTL/vector.h>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>

namespace NeneEngine
{

	struct SceneEntityVisibilityOverrideConfig
	{
		std::string tag;
		bool visible = true;
	};

	struct SceneEntityTransformOverrideConfig
	{
		std::string tag;
		eastl::optional<glm::vec3> position;
		eastl::optional<glm::vec3> rotationDegrees;
		eastl::optional<glm::vec3> scale;
	};

	struct SceneEntityMaterialOverrideConfig
	{
		std::string tag;
		eastl::optional<glm::vec4> tint;
	};

	struct SceneConfig
	{
		eastl::vector<SceneEntityVisibilityOverrideConfig> entityVisibilityOverrides;
		eastl::vector<SceneEntityTransformOverrideConfig> entityTransformOverrides;
		eastl::vector<SceneEntityMaterialOverrideConfig> entityMaterialOverrides;
	};

	[[nodiscard]] SceneConfig LoadSceneConfig(const std::filesystem::path& configPath);
	void ApplySceneConfig(ECS::World& world, const SceneConfig& config);

} // namespace NeneEngine
