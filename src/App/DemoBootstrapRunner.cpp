#include "App/DemoBootstrapRunner.h"

#include "Core/CustomLogger.h"
#include "Core/PathResolver.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include "Rendering/ModelSpawner.h"

namespace NeneEngine
{
	void RunDemoBootstrap(ECS::World& world, IRenderAdapter* primaryRenderer)
	{
		if (primaryRenderer == nullptr)
		{
			NENE_LOG_INFO("Demo bootstrap skipped: no primary renderer is available");
			return;
		}

		const auto shaderPath =
		    ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "shaders" / "textured_mesh.shader");
		const auto manifestPath =
		    ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "models" / "spawn_manifest.json");

		if (shaderPath.empty() || manifestPath.empty())
		{
			NENE_LOG_INFO("Demo bootstrap skipped: demo shader or spawn manifest was not found");
			return;
		}

		NENE_LOG_INFO("Demo bootstrap: spawning demo models from '{}'", manifestPath.string());
		const ShaderId shaderId = CreateTexturedMeshShader(*primaryRenderer, shaderPath);
		SpawnModelsFromManifest(world, *primaryRenderer, shaderId, manifestPath);
		NENE_LOG_INFO("Demo bootstrap completed");
	}
} // namespace NeneEngine
