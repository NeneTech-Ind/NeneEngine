// StartupLibraryCheck.cpp

#include "App/StartupLibraryCheck.h"

#include "Core/JobSystem.h"
#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Core/ResourceManager.h"
#include "Graphics/Runtime/RenderTypes.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <atomic>
#include <filesystem>
#include <string>

namespace NeneEngine
{
	namespace
	{
		void CheckAssimp()
		{
			const auto modelPath = ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "models" /
			                                                 "momosuzu_nene_posed" / "momosuzu_nene_posed.obj");
			if (modelPath.empty())
			{
				NENE_LOG_ERROR("Assimp check: model file was not found");
				return;
			}

			try
			{
				auto meshResource = ResourceManager::GetInstance().Load<Mesh>(modelPath.string());
				if (meshResource == nullptr)
				{
					NENE_LOG_ERROR("Assimp check failed for '{}': resource manager returned null", modelPath.string());
					return;
				}

				const MeshData& meshData = meshResource->GetData().data;
				NENE_LOG_INFO("Assimp check: loaded '{}' | vertices={} indices={}", modelPath.string(),
				              meshData.vertices.size(), meshData.indices.size());
			}
			catch (const std::exception& exception)
			{
				NENE_LOG_ERROR("Assimp check failed for '{}': {}", modelPath.string(), exception.what());
			}
		}

		void CheckStbImage()
		{
			const auto texturePath =
			    ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "readme" / "banner.png");
			if (texturePath.empty())
			{
				NENE_LOG_ERROR("stb_image check: texture file was not found");
				return;
			}

			int width = 0;
			int height = 0;
			int channels = 0;
			stbi_uc* pixels = stbi_load(texturePath.string().c_str(), &width, &height, &channels, 0);
			if (pixels == nullptr)
			{
				NENE_LOG_ERROR("stb_image check failed for '{}': {}", texturePath.string(), stbi_failure_reason());
				return;
			}

			NENE_LOG_INFO("stb_image check: loaded '{}' | width={} height={} channels={}", texturePath.string(), width,
			              height, channels);

			stbi_image_free(pixels);
		}

		void CheckJobSystem(JobSystem& jobSystem)
		{
			constexpr uint32_t kItemCount = 100000;
			constexpr uint32_t kMinRange = 1024;
			constexpr uint64_t kExpectedSum = static_cast<uint64_t>(kItemCount) * (kItemCount - 1) / 2;

			std::atomic<uint64_t> sum{0};
			const JobHandle job = jobSystem.ParallelFor("StartupCheck", kItemCount, kMinRange,
			                                            [&sum](uint32_t begin, uint32_t end)
			                                            {
				                                            uint64_t rangeSum = 0;
				                                            for (uint32_t index = begin; index < end; ++index)
					                                            rangeSum += index;
				                                            sum.fetch_add(rangeSum, std::memory_order_relaxed);
			                                            });
			jobSystem.Wait(job);

			if (job.HasFailed() || sum.load(std::memory_order_relaxed) != kExpectedSum)
			{
				NENE_LOG_ERROR("Job system check failed: sum={} expected={}", sum.load(std::memory_order_relaxed),
				               kExpectedSum);
				return;
			}

			NENE_LOG_INFO("Job system check: sum={} over {} items", kExpectedSum, kItemCount);
		}
	} // namespace

	void RunStartupLibraryChecks(JobSystem& jobSystem)
	{
		NENE_LOG_INFO("Startup library checks: started");
		CheckAssimp();
		CheckStbImage();
		CheckJobSystem(jobSystem);
		NENE_LOG_INFO("Startup library checks: finished");
	}

} // namespace NeneEngine
