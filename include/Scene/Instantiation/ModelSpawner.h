#pragma once

#include <filesystem>

namespace NeneEngine
{
	class ResourceLoadQueue;

	namespace ECS
	{
		class World;
	}

	void SpawnModelsFromManifest(ECS::World& world, ResourceLoadQueue& loads, const std::filesystem::path& shaderPath,
	                             const std::filesystem::path& manifestPath);

} // namespace NeneEngine
