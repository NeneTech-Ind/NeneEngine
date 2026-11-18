// LogHistorySink.h

#pragma once

#include <EASTL/deque.h>
#include <EASTL/vector.h>
#include <spdlog/sinks/base_sink.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>

namespace NeneEngine
{

	struct LogEntry
	{
		uint64_t sequence = 0;
		spdlog::level::level_enum level = spdlog::level::info;
		std::string time;
		std::string message;
	};

	class LogHistorySink final : public spdlog::sinks::base_sink<std::mutex>
	{
	  public:
		explicit LogHistorySink(size_t capacity);

		uint64_t CopySince(uint64_t sequence, eastl::vector<LogEntry>& entries);

	  protected:
		void sink_it_(const spdlog::details::log_msg& message) override;
		void flush_() override {}

	  private:
		size_t m_capacity = 0;
		uint64_t m_lastSequence = 0;
		eastl::deque<LogEntry> m_entries;
	};

} // namespace NeneEngine
