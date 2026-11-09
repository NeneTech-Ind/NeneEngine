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
		    : enki::ITaskSet(itemCount, (std::max)(minRange, 1u)), m_name(name ? name : "Job"),
		      m_function(std::move(function))
		{
			m_Priority = ToEnkiPriority(priority);
		}

		void ExecuteRange(enki::TaskSetPartition range, uint32_t /*threadNumber*/) override
		{
			NENE_PROFILE_SCOPE("Job");
			NENE_PROFILE_ZONE_NAME(m_name.data(), m_name.size());

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
		std::string m_name;
		JobSystem::RangeFunction m_function;
		std::atomic<bool> m_submitted{false};
		std::atomic<bool> m_failed{false};
	};

	class StreamingJob final : public enki::IPinnedTask
	{
	  public:
		StreamingJob(uint32_t worker, const char* name, JobSystem::JobFunction function, JobPriority priority)
		    : enki::IPinnedTask(worker), m_name(name ? name : "Streaming Job"), m_function(std::move(function))
		{
			m_Priority = ToEnkiPriority(priority);
		}

		void Execute() override
		{
			NENE_PROFILE_SCOPE("Streaming Job");
			NENE_PROFILE_ZONE_NAME(m_name.data(), m_name.size());
			try
			{
				m_function();
			}
			catch (const std::exception& exception)
			{
				m_failed.store(true, std::memory_order_release);
				NENE_LOG_ERROR("JobSystem: streaming job '{}' failed: {}", m_name, exception.what());
			}
			catch (...)
			{
				m_failed.store(true, std::memory_order_release);
				NENE_LOG_ERROR("JobSystem: streaming job '{}' failed", m_name);
			}
		}

		void MarkSubmitted() { m_submitted.store(true, std::memory_order_release); }
		bool IsFinished() const { return m_submitted.load(std::memory_order_acquire) && GetIsComplete(); }
		bool HasFailed() const { return m_failed.load(std::memory_order_acquire); }

	  private:
		std::string m_name;
		JobSystem::JobFunction m_function;
		std::atomic<bool> m_submitted{false};
		std::atomic<bool> m_failed{false};
	};

	namespace
	{
		class StreamingWorkerLoop final : public enki::IPinnedTask
		{
		  public:
			StreamingWorkerLoop(enki::TaskScheduler& scheduler, uint32_t worker)
			    : enki::IPinnedTask(worker), m_scheduler(scheduler)
			{
			}

			void Execute() override
			{
				const std::string name = "StreamingWorker " + std::to_string(threadNum);
				NENE_PROFILE_THREAD_NAME(name.c_str());
				while (!m_scheduler.GetIsShutdownRequested())
				{
					m_scheduler.WaitForNewPinnedTasks();
					m_scheduler.RunPinnedTasks();
				}
			}

		  private:
			enki::TaskScheduler& m_scheduler;
		};
	} // namespace

	JobHandle::JobHandle(eastl::shared_ptr<JobTaskSet> job) : m_job(std::move(job)) {}
	JobHandle::JobHandle(eastl::shared_ptr<StreamingJob> job) : m_streamingJob(std::move(job)) {}

	bool JobHandle::IsComplete() const
	{
		return m_streamingJob ? m_streamingJob->IsFinished() : m_job == nullptr || m_job->IsFinished();
	}

	bool JobHandle::HasFailed() const
	{
		return m_streamingJob ? m_streamingJob->HasFailed() : m_job != nullptr && m_job->HasFailed();
	}

	struct JobSystem::Impl
	{
		enki::TaskScheduler scheduler;
		std::mutex mutex;
		eastl::vector<eastl::shared_ptr<JobTaskSet>> activeJobs;
		eastl::vector<eastl::shared_ptr<StreamingJob>> streamingJobs;
		eastl::vector<eastl::unique_ptr<StreamingWorkerLoop>> streamingLoops;
		uint32_t nextStreamingWorker = 0;
		bool isRunning = false;
	};

	JobSystem::JobSystem() : m_impl(eastl::make_unique<Impl>()) {}

	JobSystem::~JobSystem()
	{
		Shutdown();
	}

	void JobSystem::Initialize(uint32_t streamingWorkerCount)
	{
		std::scoped_lock lock(m_impl->mutex);
		if (m_impl->isRunning) return;

		enki::TaskSchedulerConfig config{};
		const uint32_t hardwareThreads = (std::max)(enki::GetNumHardwareThreads(), 2u);
		streamingWorkerCount = std::clamp(streamingWorkerCount, 1u, hardwareThreads - 1);
		const uint32_t computeWorkers = (std::max)(1u, hardwareThreads - 1 - streamingWorkerCount);
		config.numTaskThreadsToCreate = computeWorkers + streamingWorkerCount;
		config.profilerCallbacks.threadStart = OnWorkerThreadStart;
		m_impl->scheduler.Initialize(config);
		m_impl->isRunning = true;
		m_impl->nextStreamingWorker = 0;
		for (uint32_t index = 0; index < streamingWorkerCount; ++index)
		{
			auto loop = eastl::make_unique<StreamingWorkerLoop>(m_impl->scheduler, computeWorkers + 1 + index);
			m_impl->scheduler.AddPinnedTask(loop.get());
			m_impl->streamingLoops.push_back(std::move(loop));
		}

		NENE_LOG_INFO("JobSystem: started with {} compute and {} streaming workers", computeWorkers,
		              streamingWorkerCount);
	}

	void JobSystem::Shutdown()
	{
		{
			std::scoped_lock lock(m_impl->mutex);
			if (!m_impl->isRunning) return;
			m_impl->isRunning = false;
		}

		NENE_PROFILE_SCOPE("Job Shutdown");

		m_impl->scheduler.WaitforAllAndShutdown();

		std::scoped_lock lock(m_impl->mutex);
		m_impl->activeJobs.clear();
		m_impl->streamingJobs.clear();
		m_impl->streamingLoops.clear();

		NENE_LOG_INFO("JobSystem: stopped");
	}

	JobHandle JobSystem::Schedule(const char* name, JobFunction function, JobPriority priority)
	{
		if (!function) return {};

		return ParallelFor(name, 1, 1, [function = std::move(function)](uint32_t, uint32_t) { function(); }, priority);
	}

	JobHandle JobSystem::ScheduleStreaming(const char* name, JobFunction function, JobPriority priority)
	{
		if (!function) return {};
		NENE_PROFILE_SCOPE("Streaming Submit");
		std::scoped_lock lock(m_impl->mutex);
		if (!m_impl->isRunning || m_impl->streamingLoops.empty()) return {};
		eastl::erase_if(m_impl->streamingJobs, [](const auto& job) { return job->IsFinished(); });
		const auto& loop = m_impl->streamingLoops[m_impl->nextStreamingWorker++ % m_impl->streamingLoops.size()];
		auto job = eastl::make_shared<StreamingJob>(loop->threadNum, name, std::move(function), priority);
		m_impl->streamingJobs.push_back(job);
		m_impl->scheduler.AddPinnedTask(job.get());
		job->MarkSubmitted();
		return JobHandle(std::move(job));
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
		if (handle.m_streamingJob)
			m_impl->scheduler.WaitforTask(handle.m_streamingJob.get(), handle.m_streamingJob->m_Priority);
		else
			m_impl->scheduler.WaitforTask(handle.m_job.get(), handle.m_job->m_Priority);
	}

	JobHandle JobSystem::Submit(eastl::shared_ptr<JobTaskSet> job)
	{
		NENE_PROFILE_SCOPE("Job Submit");

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
