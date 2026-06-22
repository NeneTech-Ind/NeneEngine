#include "Scene/Instantiation/ModelInstanceConfig.h"

#include "Core/CustomLogger.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace NeneEngine
{
	namespace
	{
		glm::vec3 ReadVec3OrDefault(const nlohmann::json& value, const glm::vec3& defaultValue)
		{
			if (!value.is_object()) return defaultValue;
			if (!value.contains("x") || !value.at("x").is_number()) return defaultValue;
			if (!value.contains("y") || !value.at("y").is_number()) return defaultValue;
			if (!value.contains("z") || !value.at("z").is_number()) return defaultValue;

			return {value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>()};
		}
	} // namespace

	ModelInstanceConfig LoadModelInstanceConfig(const std::filesystem::path& configPath)
	{
		ModelInstanceConfig config{};

		if (!std::filesystem::exists(configPath))
		{
			NENE_LOG_INFO("Model config '{}' not found, using defaults", configPath.string());
			return config;
		}

		std::ifstream input(configPath);
		if (!input.is_open())
		{
			NENE_LOG_WARN("Model config '{}' could not be opened, using defaults", configPath.string());
			return config;
		}

		try
		{
			nlohmann::json root;
			input >> root;

			config.position = ReadVec3OrDefault(root.value("position", nlohmann::json::object()), config.position);
			config.scale = ReadVec3OrDefault(root.value("scale", nlohmann::json::object()), config.scale);

			const auto partOverridesIt = root.find("partOverrides");
			if (partOverridesIt != root.end() && partOverridesIt->is_array())
			{
				for (const auto& overrideValue : *partOverridesIt)
				{
					if (!overrideValue.is_object()) continue;

					ModelPartOverrideConfig overrideConfig{};
					overrideConfig.nameContains = overrideValue.value("nameContains", "");
					overrideConfig.visible = overrideValue.value("visible", true);
					overrideConfig.positionOffset = ReadVec3OrDefault(
					    overrideValue.value("positionOffset", nlohmann::json::object()), overrideConfig.positionOffset);
					overrideConfig.rotationOffsetDegrees =
					    ReadVec3OrDefault(overrideValue.value("rotationOffsetDegrees", nlohmann::json::object()),
					                      overrideConfig.rotationOffsetDegrees);
					overrideConfig.scaleMultiplier =
					    ReadVec3OrDefault(overrideValue.value("scaleMultiplier", nlohmann::json::object()),
					                      overrideConfig.scaleMultiplier);
					overrideConfig.textureOverride = overrideValue.value("textureOverride", "");

					if (!overrideConfig.nameContains.empty()) config.partOverrides.push_back(std::move(overrideConfig));
				}
			}

			NENE_LOG_INFO("Model config loaded from '{}'; partOverrides={}", configPath.string(),
			              config.partOverrides.size());
		}
		catch (const std::exception& ex)
		{
			NENE_LOG_WARN("Model config '{}' parse failed: {}. Using defaults", configPath.string(), ex.what());
		}

		return config;
	}

	const ModelPartOverrideConfig* FindPartOverride(const ModelInstanceConfig& config, std::string_view partName)
	{
		for (const auto& overrideConfig : config.partOverrides)
		{
			if (partName.find(overrideConfig.nameContains) != std::string_view::npos) return &overrideConfig;
		}

		return nullptr;
	}

} // namespace NeneEngine
