#include "Scene/SceneConfig.h"

#include "Core/CustomLogger.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"

#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace NeneEngine
{
	namespace
	{
		eastl::optional<glm::vec3> ReadOptionalVec3(const nlohmann::json& value)
		{
			if (!value.is_object()) return eastl::nullopt;
			if (!value.contains("x") || !value.at("x").is_number()) return eastl::nullopt;
			if (!value.contains("y") || !value.at("y").is_number()) return eastl::nullopt;
			if (!value.contains("z") || !value.at("z").is_number()) return eastl::nullopt;

			return glm::vec3{value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>()};
		}

		eastl::optional<glm::vec4> ReadOptionalVec4(const nlohmann::json& value)
		{
			if (!value.is_object()) return eastl::nullopt;
			if (!value.contains("x") || !value.at("x").is_number()) return eastl::nullopt;
			if (!value.contains("y") || !value.at("y").is_number()) return eastl::nullopt;
			if (!value.contains("z") || !value.at("z").is_number()) return eastl::nullopt;
			if (!value.contains("w") || !value.at("w").is_number()) return eastl::nullopt;

			return glm::vec4{value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>(),
			                 value.at("w").get<float>()};
		}
	} // namespace

	SceneConfig LoadSceneConfig(const std::filesystem::path& configPath)
	{
		SceneConfig config{};

		if (!std::filesystem::exists(configPath))
		{
			NENE_LOG_INFO("Scene config '{}' not found, using defaults", configPath.string());
			return config;
		}

		std::ifstream input(configPath);
		if (!input.is_open())
		{
			NENE_LOG_WARN("Scene config '{}' could not be opened, using defaults", configPath.string());
			return config;
		}

		try
		{
			nlohmann::json root;
			input >> root;

			const auto visibilityOverridesIt = root.find("entityVisibilityOverrides");
			if (visibilityOverridesIt != root.end() && visibilityOverridesIt->is_array())
			{
				for (const auto& overrideValue : *visibilityOverridesIt)
				{
					if (!overrideValue.is_object()) continue;

					SceneEntityVisibilityOverrideConfig overrideConfig{};
					overrideConfig.tag = overrideValue.value("tag", "");
					overrideConfig.visible = overrideValue.value("visible", true);
					if (!overrideConfig.tag.empty())
						config.entityVisibilityOverrides.push_back(std::move(overrideConfig));
				}
			}

			const auto transformOverridesIt = root.find("entityTransformOverrides");
			if (transformOverridesIt != root.end() && transformOverridesIt->is_array())
			{
				for (const auto& overrideValue : *transformOverridesIt)
				{
					if (!overrideValue.is_object()) continue;

					SceneEntityTransformOverrideConfig overrideConfig{};
					overrideConfig.tag = overrideValue.value("tag", "");
					overrideConfig.position =
					    ReadOptionalVec3(overrideValue.value("position", nlohmann::json::object()));
					overrideConfig.rotationDegrees =
					    ReadOptionalVec3(overrideValue.value("rotationDegrees", nlohmann::json::object()));
					overrideConfig.scale = ReadOptionalVec3(overrideValue.value("scale", nlohmann::json::object()));

					const bool hasAnyOverride = overrideConfig.position.has_value() ||
					                            overrideConfig.rotationDegrees.has_value() ||
					                            overrideConfig.scale.has_value();
					if (!overrideConfig.tag.empty() && hasAnyOverride)
						config.entityTransformOverrides.push_back(std::move(overrideConfig));
				}
			}

			const auto materialOverridesIt = root.find("entityMaterialOverrides");
			if (materialOverridesIt != root.end() && materialOverridesIt->is_array())
			{
				for (const auto& overrideValue : *materialOverridesIt)
				{
					if (!overrideValue.is_object()) continue;

					SceneEntityMaterialOverrideConfig overrideConfig{};
					overrideConfig.tag = overrideValue.value("tag", "");
					overrideConfig.tint = ReadOptionalVec4(overrideValue.value("tint", nlohmann::json::object()));
					if (!overrideConfig.tag.empty() && overrideConfig.tint.has_value())
						config.entityMaterialOverrides.push_back(std::move(overrideConfig));
				}
			}

			NENE_LOG_INFO(
			    "Scene config loaded from '{}'; visibilityOverrides={}, transformOverrides={}, materialOverrides={}",
			    configPath.string(), config.entityVisibilityOverrides.size(), config.entityTransformOverrides.size(),
			    config.entityMaterialOverrides.size());
		}
		catch (const std::exception& ex)
		{
			NENE_LOG_WARN("Scene config '{}' parse failed: {}. Using defaults", configPath.string(), ex.what());
		}

		return config;
	}

	void ApplySceneConfig(ECS::World& world, const SceneConfig& config)
	{
		auto renderView = world.GetRegistry().view<ECS::TagComponent, ECS::MeshRendererComponent>();
		for (const auto& overrideConfig : config.entityVisibilityOverrides)
		{
			size_t matchedCount = 0;
			for (auto entity : renderView)
			{
				auto& tag = renderView.get<ECS::TagComponent>(entity);
				if (tag.name != overrideConfig.tag) continue;

				auto& meshRenderer = renderView.get<ECS::MeshRendererComponent>(entity);
				meshRenderer.visible = overrideConfig.visible;
				++matchedCount;
			}

			if (matchedCount == 0)
			{
				NENE_LOG_WARN("Scene visibility override for tag '{}' matched no renderable entities",
				              overrideConfig.tag);
				continue;
			}

			NENE_LOG_INFO("Applied scene visibility override for tag '{}': visible={} matched={}", overrideConfig.tag,
			              overrideConfig.visible, matchedCount);
		}

		auto transformView = world.GetRegistry().view<ECS::TagComponent, ECS::TransformComponent>();
		for (const auto& overrideConfig : config.entityTransformOverrides)
		{
			size_t matchedCount = 0;
			for (auto entity : transformView)
			{
				auto& tag = transformView.get<ECS::TagComponent>(entity);
				if (tag.name != overrideConfig.tag) continue;

				auto& transform = transformView.get<ECS::TransformComponent>(entity);
				if (overrideConfig.position.has_value()) transform.position = *overrideConfig.position;
				if (overrideConfig.rotationDegrees.has_value())
					transform.rotation = glm::quat(glm::radians(*overrideConfig.rotationDegrees));
				if (overrideConfig.scale.has_value()) transform.scale = *overrideConfig.scale;
				++matchedCount;
			}

			if (matchedCount == 0)
			{
				NENE_LOG_WARN("Scene transform override for tag '{}' matched no transformable entities",
				              overrideConfig.tag);
				continue;
			}

			NENE_LOG_INFO(
			    "Applied scene transform override for tag '{}': position={}, rotationDegrees={}, scale={}, matched={}",
			    overrideConfig.tag, overrideConfig.position.has_value(), overrideConfig.rotationDegrees.has_value(),
			    overrideConfig.scale.has_value(), matchedCount);
		}

		for (const auto& overrideConfig : config.entityMaterialOverrides)
		{
			size_t matchedCount = 0;
			for (auto entity : renderView)
			{
				auto& tag = renderView.get<ECS::TagComponent>(entity);
				if (tag.name != overrideConfig.tag) continue;

				auto& meshRenderer = renderView.get<ECS::MeshRendererComponent>(entity);
				if (overrideConfig.tint.has_value()) meshRenderer.tint = *overrideConfig.tint;
				++matchedCount;
			}

			if (matchedCount == 0)
			{
				NENE_LOG_WARN("Scene material override for tag '{}' matched no renderable entities",
				              overrideConfig.tag);
				continue;
			}

			NENE_LOG_INFO("Applied scene material override for tag '{}': tint={} matched={}", overrideConfig.tag,
			              overrideConfig.tint.has_value(), matchedCount);
		}
	}

} // namespace NeneEngine
