// GameTimer.h

#pragma once

#include <chrono>

namespace NeneEngine
{

	class GameTimer
	{
	  public:
		GameTimer();

		float GetTotalTime() const;
		float GetDeltaTime() const;

		void Reset(); // Call before message loop.
		void Start(); // Call when unpaused.
		void Stop();  // Call when paused.
		void Tick();  // Call every frame.

	  private:
		using Clock = std::chrono::steady_clock;

		Clock::time_point m_baseTime;
		Clock::duration m_pausedDuration;
		Clock::time_point m_stopTime;
		Clock::time_point m_prevTime;
		Clock::time_point m_currTime;

		double m_deltaTime;
		bool m_stopped;
	};

} // namespace NeneEngine
