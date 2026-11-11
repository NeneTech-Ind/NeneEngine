// JobSystem.h

#pragma once

#include <EASTL/functional.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/unique_ptr.h>
#include <cstdint>

namespace enki
{
	class TaskScheduler;
} // namespace enki

namespace NeneEngine
{

	class JobTaskSet;
	class StreamingJob;

	enum class JobPriority : uint8_t
	{
		High,
		Normal,
		Low
	};

	class JobHandle final
	{
	  public:
		JobHandle() = default;

		[[nodiscard]] bool IsValid() const { return m_job != nullptr || m_streamingJob != nullptr; }
		[[nodiscard]] bool IsComplete() const;
		[[nodiscard]] bool HasFailed() const;

	  private:
		friend class JobSystem;
		explicit JobHandle(eastl::shared_ptr<JobTaskSet> job);
		explicit JobHandle(eastl::shared_ptr<StreamingJob> job);

		eastl::shared_ptr<JobTaskSet> m_job;
		eastl::shared_ptr<StreamingJob> m_streamingJob;
	};

	class JobSystem final
	{
	  public:
		using JobFunction = eastl::function<void()>;
		using RangeFunction = eastl::function<void(uint32_t begin, uint32_t end)>;

		JobSystem();
		~JobSystem();

		JobSystem(const JobSystem&) = delete;
		JobSystem& operator=(const JobSystem&) = delete;

		void Initialize(uint32_t streamingWorkerCount = 2);
		void Shutdown();

		JobHandle Schedule(const char* name, JobFunction function, JobPriority priority = JobPriority::Normal);
		JobHandle ScheduleStreaming(const char* name, JobFunction function, JobPriority priority = JobPriority::Normal);
		JobHandle ParallelFor(const char* name, uint32_t itemCount, uint32_t minRange, RangeFunction function,
		                      JobPriority priority = JobPriority::Normal);
		JobHandle CreateParallelFor(const char* name, uint32_t itemCount, uint32_t minRange, RangeFunction function,
		                            JobPriority priority = JobPriority::Normal);
		bool AddDependency(const JobHandle& job, const JobHandle& dependency);
		bool Submit(const JobHandle& handle);
		void Wait(const JobHandle& handle);

		[[nodiscard]] enki::TaskScheduler& GetScheduler();
		[[nodiscard]] const enki::TaskScheduler& GetScheduler() const;
		[[nodiscard]] uint32_t GetComputeWorkerCount() const;

	  private:
		struct Impl;

		eastl::unique_ptr<Impl> m_impl;
	};

} // namespace NeneEngine
