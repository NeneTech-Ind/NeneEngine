#pragma once

#include <EASTL/vector.h>
#include <filesystem>
#include <string>

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
		eastl::vector<ModelSpawnEntryConfig> models;
	};

	[[nodiscard]] ModelSpawnManifestConfig LoadModelSpawnManifest(const std::filesystem::path& configPath);

} // namespace NeneEngine
