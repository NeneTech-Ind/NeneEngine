// JoltJobSystem.h

#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>

#include <EASTL/unique_ptr.h>
#include <atomic>
#include <cstdint>

namespace NeneEngine
{

	class JobSystem;

	class JoltJobSystem final : public JPH::JobSystemWithBarrier
	{
	  public:
		JoltJobSystem(NeneEngine::JobSystem& jobSystem, uint32_t maxJobs, uint32_t maxBarriers);
		~JoltJobSystem() override;

		int GetMaxConcurrency() const override;
		JPH::JobHandle CreateJob(const char* name, JPH::ColorArg color, const JobFunction& function,
		                         JPH::uint32 numDependencies = 0) override;

	  protected:
		void QueueJob(Job* job) override;
		void QueueJobs(Job** jobs, JPH::uint count) override;
		void FreeJob(Job* job) override;

	  private:
		class JobTask;
		using JobPool = JPH::FixedSizeFreeList<Job>;

		[[nodiscard]] bool CanDispatch() const;
		void Dispatch(Job* job);

		NeneEngine::JobSystem& m_jobSystem;
		JobPool m_jobs;
		eastl::unique_ptr<JobTask[]> m_tasks;
		uint32_t m_taskCount = 0;
		std::atomic<uint32_t> m_nextTask{0};
	};

} // namespace NeneEngine
