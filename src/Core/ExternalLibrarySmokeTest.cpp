// ExternalLibrarySmokeTest.cpp

#include "Core/ExternalLibrarySmokeTest.h"

#include "Core/CustomLogger.h"
#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "Graphics/Runtime/RenderTypes.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <filesystem>
#include <string>

namespace NeneEngine
{
	namespace
	{
		void RunAssimpSmokeTest()
		{
			const auto modelPath = ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "models" /
			                                                 "momosuzu_nene_posed" / "momosuzu_nene_posed.obj");
			if (modelPath.empty())
			{
				NENE_LOG_ERROR("Assimp smoke test: model file was not found");
				return;
			}

			try
			{
				auto meshResource = ResourceManager::GetInstance().Load<Mesh>(modelPath.string());
				if (meshResource == nullptr)
				{
					NENE_LOG_ERROR("Assimp smoke test failed for '{}': resource manager returned null",
					               modelPath.string());
					return;
				}

				const MeshData& meshData = meshResource->GetData().data;
				NENE_LOG_INFO("Assimp smoke test: loaded '{}' | vertices={} indices={}", modelPath.string(),
				              meshData.vertices.size(), meshData.indices.size());
			}
			catch (const std::exception& exception)
			{
				NENE_LOG_ERROR("Assimp smoke test failed for '{}': {}", modelPath.string(), exception.what());
			}
		}

		void RunStbImageSmokeTest()
		{
			const auto texturePath =
			    ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "readme" / "banner.png");
			if (texturePath.empty())
			{
				NENE_LOG_ERROR("stb_image smoke test: texture file was not found");
				return;
			}

			int width = 0;
			int height = 0;
			int channels = 0;
			stbi_uc* pixels = stbi_load(texturePath.string().c_str(), &width, &height, &channels, 0);
			if (pixels == nullptr)
			{
				NENE_LOG_ERROR("stb_image smoke test failed for '{}': {}", texturePath.string(), stbi_failure_reason());
				return;
			}

			NENE_LOG_INFO("stb_image smoke test: loaded '{}' | width={} height={} channels={}", texturePath.string(),
			              width, height, channels);

			stbi_image_free(pixels);
		}
	} // namespace

	void RunExternalLibrarySmokeTests()
	{
		NENE_LOG_INFO("External library smoke tests: started");
		RunAssimpSmokeTest();
		RunStbImageSmokeTest();
		NENE_LOG_INFO("External library smoke tests: finished");
	}

} // namespace NeneEngine
