#include "Scene/Instantiation/ModelSpawnManifest.h"

#include "Core/CustomLogger.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace NeneEngine
{
	ModelSpawnManifestConfig LoadModelSpawnManifest(const std::filesystem::path& configPath)
	{
		ModelSpawnManifestConfig config{};

		if (!std::filesystem::exists(configPath))
		{
			NENE_LOG_WARN("Model spawn manifest '{}' not found", configPath.string());
			return config;
		}

		std::ifstream input(configPath);
		if (!input.is_open())
		{
			NENE_LOG_WARN("Model spawn manifest '{}' could not be opened", configPath.string());
			return config;
		}

		try
		{
			nlohmann::json root;
			input >> root;

			const auto modelsIt = root.find("models");
			if (modelsIt != root.end() && modelsIt->is_array())
			{
				for (const auto& modelValue : *modelsIt)
				{
					if (!modelValue.is_object()) continue;

					ModelSpawnEntryConfig entry{};
					entry.entityName = modelValue.value("entityName", entry.entityName);
					entry.meshPath = modelValue.value("meshPath", "");
					entry.instanceConfigPath = modelValue.value("instanceConfigPath", "");
					entry.splitByMeshParts = modelValue.value("splitByMeshParts", false);

					if (!entry.meshPath.empty()) config.models.push_back(std::move(entry));
				}
			}

			NENE_LOG_INFO("Model spawn manifest loaded from '{}'; models={}", configPath.string(),
			              config.models.size());
		}
		catch (const std::exception& ex)
		{
			NENE_LOG_WARN("Model spawn manifest '{}' parse failed: {}", configPath.string(), ex.what());
		}

		return config;
	}

} // namespace NeneEngine
