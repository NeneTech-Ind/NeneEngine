// ResourceLoadQueue.h

#pragma once

#include "Core/JobSystem.h"

#include <EASTL/functional.h>
#include <EASTL/shared_ptr.h>
#include <cstddef>
#include <string>

namespace NeneEngine
{
	enum class ResourceLoadState
	{
		Loading,
		Ready,
		Failed
	};

	class ResourceLoadQueue final
	{
	  public:
		using Completion = eastl::function<void()>;
		using PrepareFunction = eastl::function<Completion()>;
		using ErrorFunction = eastl::function<void(const std::string&)>;

		explicit ResourceLoadQueue(JobSystem& jobSystem);
		~ResourceLoadQueue();
		ResourceLoadQueue(const ResourceLoadQueue&) = delete;
		ResourceLoadQueue& operator=(const ResourceLoadQueue&) = delete;

		// Request, pump and shutdown belong to the main thread and prepare runs on a streaming worker
		bool Request(std::string name, PrepareFunction prepare, ErrorFunction onError,
		             JobPriority priority = JobPriority::Normal);
		void Pump(size_t maxFinalizations = 2, double budgetMilliseconds = 2.0);
		void Shutdown();
		[[nodiscard]] size_t GetPendingCount() const;

	  private:
		struct Impl;
		JobSystem& m_jobSystem;
		eastl::shared_ptr<Impl> m_impl;
	};
} // namespace NeneEngine
