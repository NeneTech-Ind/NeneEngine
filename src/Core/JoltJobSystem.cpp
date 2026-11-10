// JoltJobSystem.cpp

#include "Core/JoltJobSystem.h"

#include "Core/JobSystem.h"
#include "Core/Profiler.h"

#include <TaskScheduler.h>
#include <chrono>
#include <thread>

namespace NeneEngine
{

	class JoltJobSystem::JobTask final : public enki::ITaskSet
	{
	  public:
		JobTask() { m_Priority = enki::TASK_PRIORITY_HIGH; }

		bool TryAcquire()
		{
			if (!GetIsComplete()) return false;

			bool expected = false;
			if (!m_acquired.compare_exchange_strong(expected, true, std::memory_order_acquire)) return false;
			if (GetIsComplete()) return true;

			m_acquired.store(false, std::memory_order_release);
			return false;
		}

		void Submit(enki::TaskScheduler& scheduler, Job* job)
		{
			m_job = job;
			scheduler.AddTaskSetToPipe(this);
			m_acquired.store(false, std::memory_order_release);
		}

		void ExecuteRange(enki::TaskSetPartition /*range*/, uint32_t /*threadNumber*/) override
		{
			NENE_PROFILE_SCOPE("Physics Job");
			Job* job = m_job;
			job->Execute();
			job->Release();
		}

	  private:
		Job* m_job = nullptr;
		std::atomic<bool> m_acquired{false};
	};

	JoltJobSystem::JoltJobSystem(NeneEngine::JobSystem& jobSystem, uint32_t maxJobs, uint32_t maxBarriers)
	    : JobSystemWithBarrier(maxBarriers), m_jobSystem(jobSystem), m_tasks(eastl::make_unique<JobTask[]>(maxJobs)),
	      m_taskCount(maxJobs)
	{
		m_jobs.Init(maxJobs, maxJobs);
	}

	JoltJobSystem::~JoltJobSystem()
	{
		enki::TaskScheduler& scheduler = m_jobSystem.GetScheduler();
		for (uint32_t index = 0; index < m_taskCount; ++index)
			if (!m_tasks[index].GetIsComplete()) scheduler.WaitforTask(&m_tasks[index]);
	}

	int JoltJobSystem::GetMaxConcurrency() const
	{
		return static_cast<int>(m_jobSystem.GetComputeWorkerCount()) + 1;
	}

	JPH::JobHandle JoltJobSystem::CreateJob(const char* name, JPH::ColorArg color, const JobFunction& function,
	                                        JPH::uint32 numDependencies)
	{
		JPH::uint32 index = m_jobs.ConstructObject(name, color, this, function, numDependencies);
		while (index == JobPool::cInvalidObjectIndex)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(100));
			index = m_jobs.ConstructObject(name, color, this, function, numDependencies);
		}

		Job* job = &m_jobs.Get(index);
		JPH::JobHandle handle(job);
		if (numDependencies == 0) QueueJob(job);
		return handle;
	}

	void JoltJobSystem::QueueJob(Job* job)
	{
		if (CanDispatch()) Dispatch(job);
	}

	void JoltJobSystem::QueueJobs(Job** jobs, JPH::uint count)
	{
		if (!CanDispatch()) return;
		for (JPH::uint index = 0; index < count; ++index) Dispatch(jobs[index]);
	}

	void JoltJobSystem::FreeJob(Job* job)
	{
		m_jobs.DestructObject(job);
	}

	bool JoltJobSystem::CanDispatch() const
	{
		const enki::TaskScheduler& scheduler = m_jobSystem.GetScheduler();
		return !scheduler.GetIsShutdownRequested() && scheduler.GetThreadNum() != enki::NO_THREAD_NUM;
	}

	void JoltJobSystem::Dispatch(Job* job)
	{
		job->AddRef();

		enki::TaskScheduler& scheduler = m_jobSystem.GetScheduler();
		const uint32_t start = m_nextTask.fetch_add(1, std::memory_order_relaxed);
		for (uint32_t offset = 0; offset < m_taskCount; ++offset)
		{
			JobTask& task = m_tasks[(start + offset) % m_taskCount];
			if (!task.TryAcquire()) continue;

			task.Submit(scheduler, job);
			return;
		}

		job->Execute();
		job->Release();
	}

} // namespace NeneEngine
