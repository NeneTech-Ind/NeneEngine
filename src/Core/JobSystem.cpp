// JobSystem.cpp

#include "Core/JobSystem.h"

#include "Core/NeneLogger.h"
#include "Core/Profiler.h"

#include <EASTL/vector.h>
#include <TaskScheduler.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>

namespace NeneEngine
{
	namespace
	{
		enki::TaskPriority ToEnkiPriority(JobPriority priority)
		{
			switch (priority)
			{
			case JobPriority::High:
				return enki::TASK_PRIORITY_HIGH;
			case JobPriority::Low:
				return enki::TASK_PRIORITY_LOW;
			case JobPriority::Normal:
				break;
			}

			return enki::TASK_PRIORITY_MED;
		}

		void OnWorkerThreadStart(uint32_t threadNumber)
		{
			const std::string threadName = "JobWorker " + std::to_string(threadNumber);
			NENE_PROFILE_THREAD_NAME(threadName.c_str());
		}
	} // namespace

	class JobTaskSet final : public enki::ITaskSet
	{
	  public:
		JobTaskSet(const char* name, uint32_t itemCount, uint32_t minRange, JobSystem::RangeFunction function,
		           JobPriority priority)
		    : enki::ITaskSet(itemCount, (std::max)(minRange, 1u)), m_name(name), m_function(std::move(function))
		{
			m_Priority = ToEnkiPriority(priority);
		}

		void ExecuteRange(enki::TaskSetPartition range, uint32_t /*threadNumber*/) override
		{
			NENE_PROFILE_SCOPE("Job");
			NENE_PROFILE_ZONE_NAME(m_name, std::strlen(m_name));

			try
			{
				m_function(range.start, range.end);
			}
			catch (const std::exception& exception)
			{
				m_failed.store(true, std::memory_order_release);
				NENE_LOG_ERROR("JobSystem: job '{}' failed: {}", m_name, exception.what());
			}
			catch (...)
			{
				m_failed.store(true, std::memory_order_release);
				NENE_LOG_ERROR("JobSystem: job '{}' failed with unknown error", m_name);
			}
		}

		void MarkSubmitted() { m_submitted.store(true, std::memory_order_release); }

		[[nodiscard]] bool IsFinished() const { return m_submitted.load(std::memory_order_acquire) && GetIsComplete(); }

		[[nodiscard]] bool HasFailed() const { return m_failed.load(std::memory_order_acquire); }

	  private:
		const char* m_name;
		JobSystem::RangeFunction m_function;
		std::atomic<bool> m_submitted{false};
		std::atomic<bool> m_failed{false};
	};

	JobHandle::JobHandle(eastl::shared_ptr<JobTaskSet> job) : m_job(std::move(job)) {}

	bool JobHandle::IsComplete() const
	{
		return m_job == nullptr || m_job->IsFinished();
	}

	bool JobHandle::HasFailed() const
	{
		return m_job != nullptr && m_job->HasFailed();
	}

	struct JobSystem::Impl
	{
		enki::TaskScheduler scheduler;
		std::mutex mutex;
		eastl::vector<eastl::shared_ptr<JobTaskSet>> activeJobs;
		bool isRunning = false;
	};

	JobSystem::JobSystem() : m_impl(eastl::make_unique<Impl>()) {}

	JobSystem::~JobSystem()
	{
		Shutdown();
	}

	void JobSystem::Initialize()
	{
		std::scoped_lock lock(m_impl->mutex);
		if (m_impl->isRunning) return;

		enki::TaskSchedulerConfig config{};
		config.numTaskThreadsToCreate = (std::max)(enki::GetNumHardwareThreads(), 2u) - 1;
		config.profilerCallbacks.threadStart = OnWorkerThreadStart;
		m_impl->scheduler.Initialize(config);
		m_impl->isRunning = true;

		NENE_LOG_INFO("JobSystem: started with {} worker threads", config.numTaskThreadsToCreate);
	}

	void JobSystem::Shutdown()
	{
		{
			std::scoped_lock lock(m_impl->mutex);
			if (!m_impl->isRunning) return;
			m_impl->isRunning = false;
		}

		m_impl->scheduler.WaitforAllAndShutdown();

		std::scoped_lock lock(m_impl->mutex);
		m_impl->activeJobs.clear();

		NENE_LOG_INFO("JobSystem: stopped");
	}

	JobHandle JobSystem::Schedule(const char* name, JobFunction function, JobPriority priority)
	{
		if (!function) return {};

		return ParallelFor(name, 1, 1, [function = std::move(function)](uint32_t, uint32_t) { function(); }, priority);
	}

	JobHandle JobSystem::ParallelFor(const char* name, uint32_t itemCount, uint32_t minRange, RangeFunction function,
	                                 JobPriority priority)
	{
		if (itemCount == 0 || !function) return {};

		return Submit(eastl::make_shared<JobTaskSet>(name, itemCount, minRange, std::move(function), priority));
	}

	void JobSystem::Wait(const JobHandle& handle)
	{
		if (handle.IsComplete()) return;

		if (m_impl->scheduler.GetThreadNum() == enki::NO_THREAD_NUM)
		{
			NENE_LOG_ERROR("JobSystem: Wait called from a thread the scheduler does not know");
			return;
		}

		NENE_PROFILE_SCOPE("JobWait");
		m_impl->scheduler.WaitforTask(handle.m_job.get(), handle.m_job->m_Priority);
	}

	JobHandle JobSystem::Submit(eastl::shared_ptr<JobTaskSet> job)
	{
		{
			std::scoped_lock lock(m_impl->mutex);
			if (!m_impl->isRunning)
			{
				NENE_LOG_WARN("JobSystem: job rejected because the system is not running");
				return {};
			}

			if (m_impl->scheduler.GetThreadNum() == enki::NO_THREAD_NUM)
			{
				NENE_LOG_ERROR("JobSystem: job submitted from a thread the scheduler does not know");
				return {};
			}

			eastl::erase_if(m_impl->activeJobs, [](const auto& activeJob) { return activeJob->IsFinished(); });
			m_impl->activeJobs.push_back(job);
		}

		m_impl->scheduler.AddTaskSetToPipe(job.get());
		job->MarkSubmitted();
		return JobHandle(std::move(job));
	}

} // namespace NeneEngine
