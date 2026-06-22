#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

namespace NeneEngine
{

	struct ModelPartOverrideConfig
	{
		std::string nameContains;
		bool visible = true;
		glm::vec3 positionOffset = {0.0f, 0.0f, 0.0f};
		glm::vec3 rotationOffsetDegrees = {0.0f, 0.0f, 0.0f};
		glm::vec3 scaleMultiplier = {1.0f, 1.0f, 1.0f};
		std::filesystem::path textureOverride;
	};

	struct ModelInstanceConfig
	{
		glm::vec3 position = {0.0f, 0.0f, 0.0f};
		glm::vec3 scale = {1.0f, 1.0f, 1.0f};
		std::vector<ModelPartOverrideConfig> partOverrides;
	};

	[[nodiscard]] ModelInstanceConfig LoadModelInstanceConfig(const std::filesystem::path& configPath);
	[[nodiscard]] const ModelPartOverrideConfig* FindPartOverride(const ModelInstanceConfig& config,
	                                                              std::string_view partName);

} // namespace NeneEngine
