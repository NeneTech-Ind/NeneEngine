// ResourceLoadQueue.cpp

#include "Core/ResourceLoadQueue.h"

#include "Core/NeneLogger.h"
#include "Core/Profiler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace NeneEngine
{
	struct ResourceLoadQueue::Impl
	{
		struct Request
		{
			std::string name;
			PrepareFunction prepare;
			ErrorFunction onError;
			JobPriority priority;
		};
		struct Result
		{
			Completion complete;
			ErrorFunction onError;
			std::string error;
		};

		std::deque<Request> pending;
		std::deque<Result> finished;
		std::mutex mutex;
		std::atomic<bool> stopping{false};
		size_t inFlight = 0;
		std::thread::id mainThread = std::this_thread::get_id();
	};

	ResourceLoadQueue::ResourceLoadQueue(JobSystem& jobSystem)
	    : m_jobSystem(jobSystem), m_impl(eastl::make_shared<Impl>())
	{
	}

	ResourceLoadQueue::~ResourceLoadQueue()
	{
		Shutdown();
	}

	bool ResourceLoadQueue::Request(std::string name, PrepareFunction prepare, ErrorFunction onError, JobPriority priority)
	{
		if (m_impl->stopping || !prepare) return false;
		if (std::this_thread::get_id() != m_impl->mainThread)
			throw std::logic_error("Resource requests must be submitted on the main thread");
		m_impl->pending.push_back({std::move(name), std::move(prepare), std::move(onError), priority});
		return true;
	}

	void ResourceLoadQueue::Pump(size_t maxFinalizations, double budgetMilliseconds)
	{
		NENE_PROFILE_SCOPE("Resource Pump");
		if (m_impl->stopping) return;
		if (std::this_thread::get_id() != m_impl->mainThread)
			throw std::logic_error("Resource finalization must run on the main thread");
		const auto start = std::chrono::steady_clock::now();
		for (size_t count = 0; count < maxFinalizations; ++count)
		{
			if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >=
			    budgetMilliseconds)
				break;
			Impl::Result result;
			{
				std::scoped_lock lock(m_impl->mutex);
				if (m_impl->finished.empty()) break;
				result = std::move(m_impl->finished.front());
				m_impl->finished.pop_front();
			}
			--m_impl->inFlight;
			try
			{
				NENE_PROFILE_SCOPE("Resource Finalize");
				if (!result.error.empty())
				{
					if (result.onError) result.onError(result.error);
				}
				else if (result.complete)
					result.complete();
			}
			catch (const std::exception& exception)
			{
				if (result.onError) result.onError(exception.what());
				NENE_LOG_ERROR("Resource finalization failed: {}", exception.what());
			}
		}

		// Bound decoded data waiting for the main thread as well as active decoding jobs
		constexpr size_t kMaxInFlight = 4;
		while (!m_impl->pending.empty() && m_impl->inFlight < kMaxInFlight)
		{
			const auto next =
			    std::min_element(m_impl->pending.begin(), m_impl->pending.end(),
			                     [](const auto& left, const auto& right) { return left.priority < right.priority; });
			auto request = std::move(*next);
			m_impl->pending.erase(next);
			const auto state = m_impl;
			const auto onError = request.onError;
			const auto name = request.name;
			const auto priority = request.priority;
			++state->inFlight;
			const auto job = m_jobSystem.ScheduleStreaming(
			    name.c_str(),
			    [state, request = std::move(request)]() mutable
			    {
				    if (state->stopping) return;
				    NENE_PROFILE_SCOPE("Resource Prepare");
				    NENE_PROFILE_ZONE_TEXT(request.name.data(), request.name.size());
				    Impl::Result result;
				    result.onError = std::move(request.onError);
				    try
				    {
					    result.complete = request.prepare();
				    }
				    catch (const std::exception& exception)
				    {
					    result.error = exception.what();
				    }
				    catch (...)
				    {
					    result.error = "Unknown resource loading error";
				    }
				    std::scoped_lock lock(state->mutex);
				    if (!state->stopping) state->finished.push_back(std::move(result));
			    },
			    priority);
			if (!job.IsValid())
			{
				--state->inFlight;
				if (onError) onError("JobSystem rejected resource loading");
			}
		}
	}

	void ResourceLoadQueue::Shutdown()
	{
		m_impl->stopping = true;
		m_impl->pending.clear();
		m_impl->inFlight = 0;
		std::scoped_lock lock(m_impl->mutex);
		m_impl->finished.clear();
	}

	size_t ResourceLoadQueue::GetPendingCount() const
	{
		return m_impl->pending.size() + m_impl->inFlight;
	}
} // namespace NeneEngine
