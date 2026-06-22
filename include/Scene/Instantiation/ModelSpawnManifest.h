#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace NeneEngine
{

	struct ModelSpawnEntryConfig
	{
		std::string entityName = "LoadedModel";
		std::filesystem::path meshPath;
		std::filesystem::path instanceConfigPath;
		bool splitByMeshParts = false;
	};

	struct ModelSpawnManifestConfig
	{
		std::vector<ModelSpawnEntryConfig> models;
	};

	[[nodiscard]] ModelSpawnManifestConfig LoadModelSpawnManifest(const std::filesystem::path& configPath);

} // namespace NeneEngine
