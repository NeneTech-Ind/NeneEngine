#pragma once

#include "Graphics/Runtime/RenderTypes.h"

#include <filesystem>
#include <span>

namespace NeneEngine
{
	class IRenderAdapter;

	namespace ECS
	{
		class World;
	}

	[[nodiscard]] ShaderId CreateTexturedMeshShader(IRenderAdapter& renderer, const std::filesystem::path& shaderPath);
	void SpawnModelsFromManifest(ECS::World& world, std::span<IRenderAdapter* const> renderers,
	                             const std::filesystem::path& shaderPath, const std::filesystem::path& manifestPath);

} // namespace NeneEngine
