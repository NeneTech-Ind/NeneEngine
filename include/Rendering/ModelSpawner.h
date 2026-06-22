#pragma once

#include "Graphics/Runtime/RenderTypes.h"

#include <filesystem>

namespace NeneEngine
{
	class IRenderAdapter;

	namespace ECS
	{
		class World;
	}

	[[nodiscard]] ShaderId CreateTexturedMeshShader(IRenderAdapter& renderer, const std::filesystem::path& shaderPath);
	void SpawnModelsFromManifest(ECS::World& world, IRenderAdapter& renderer, ShaderId shaderId,
	                             const std::filesystem::path& manifestPath);

} // namespace NeneEngine
