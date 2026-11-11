// AppWindowContextFactory.h

#pragma once

#include "App/AppWindowContext.h"

#include <EASTL/optional.h>
#include <EASTL/shared_ptr.h>

namespace NeneEngine
{

	class JobSystem;
	struct DiligentDX12SharedDevice;
	class SceneAssetResolver;

	class AppWindowContextFactory final
	{
	  public:
		void SetAssetResolver(SceneAssetResolver& assets) { m_assets = &assets; }
		void SetJobSystem(JobSystem& jobs) { m_jobs = &jobs; }
		[[nodiscard]] eastl::optional<AppWindowContext> Create(uint32_t width, uint32_t height,
		                                                       const std::string& title, ECS::Entity cameraEntity,
		                                                       bool isMain);

	  private:
		SceneAssetResolver* m_assets = nullptr;
		JobSystem* m_jobs = nullptr;
		eastl::shared_ptr<DiligentDX12SharedDevice> m_sharedDevice;
	};

} // namespace NeneEngine
