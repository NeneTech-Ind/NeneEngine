// LogHistorySink.cpp

#include "Core/LogHistorySink.h"

#include <chrono>
#include <ctime>

namespace NeneEngine
{
	namespace
	{
		std::string FormatTime(std::chrono::system_clock::time_point time)
		{
			const std::time_t seconds = std::chrono::system_clock::to_time_t(time);
			std::tm local{};
			localtime_s(&local, &seconds);

			char buffer[16]{};
			std::strftime(buffer, sizeof(buffer), "%H:%M:%S", &local);
			return buffer;
		}
	} // namespace

	LogHistorySink::LogHistorySink(size_t capacity) : m_capacity(capacity) {}

	uint64_t LogHistorySink::CopySince(uint64_t sequence, eastl::vector<LogEntry>& entries)
	{
		std::lock_guard lock(mutex_);
		for (const LogEntry& entry : m_entries)
		{
			if (entry.sequence > sequence) entries.push_back(entry);
		}
		return m_lastSequence;
	}

	void LogHistorySink::sink_it_(const spdlog::details::log_msg& message)
	{
		LogEntry entry;
		entry.sequence = ++m_lastSequence;
		entry.level = message.level;
		entry.time = FormatTime(message.time);
		entry.message.assign(message.payload.data(), message.payload.size());

		m_entries.push_back(std::move(entry));
		while (m_entries.size() > m_capacity) m_entries.pop_front();
	}

} // namespace NeneEngine
