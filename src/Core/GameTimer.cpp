// GameTimer.cpp

#include "Core/GameTimer.h"

namespace NeneEngine
{

	GameTimer::GameTimer()
	    : m_baseTime{}, m_pausedDuration{}, m_stopTime{}, m_prevTime{}, m_currTime{}, m_deltaTime(-1.0),
	      m_stopped(false)
	{
	}

	float GameTimer::GetTotalTime() const
	{
		Clock::time_point currentTime = m_stopped ? m_stopTime : m_currTime;

		auto effective = currentTime - m_baseTime - m_pausedDuration;

		return static_cast<float>(std::chrono::duration<double>(effective).count());
	}

	float GameTimer::GetDeltaTime() const
	{
		return static_cast<float>(m_deltaTime);
	}

	void GameTimer::Reset()
	{
		auto currTime = Clock::now();

		m_baseTime = currTime;
		m_prevTime = currTime;
		m_currTime = currTime;
		m_stopTime = {};
		m_pausedDuration = Clock::duration::zero();
		m_stopped = false;
	}

	void GameTimer::Start()
	{
		if (m_stopped)
		{
			auto startTime = Clock::now();

			m_pausedDuration += (startTime - m_stopTime);

			m_prevTime = startTime;
			m_currTime = startTime;
			m_stopTime = {};
			m_stopped = false;
		}
	}

	void GameTimer::Stop()
	{
		if (!m_stopped)
		{
			m_stopTime = Clock::now();
			m_stopped = true;
		}
	}

	void GameTimer::Tick()
	{
		if (m_stopped)
		{
			m_deltaTime = 0.0;
			return;
		}

		auto currTime = Clock::now();
		m_currTime = currTime;

		auto deltaDuration = currTime - m_prevTime;
		m_deltaTime = std::chrono::duration<double>(deltaDuration).count();

		if (m_deltaTime < 0.0) m_deltaTime = 0.0;

		m_prevTime = currTime;
	}

} // namespace NeneEngine
