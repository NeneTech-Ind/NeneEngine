// NeneLogger.h

#pragma once

#include <EASTL/memory.h>
#include <EASTL/string.h>
#include <memory>
#include <spdlog/async.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <string_view>

namespace NeneEngine
{

	class NeneLogger final
	{
	  public:
		NeneLogger(const NeneLogger&) = delete;
		NeneLogger& operator=(const NeneLogger&) = delete;
		NeneLogger(NeneLogger&&) = delete;
		NeneLogger& operator=(NeneLogger&&) = delete;

		static NeneLogger& GetInstance();

		bool Initialize(const std::string& logFileName = "NeneEngine.log", bool async = false,
		                spdlog::level::level_enum logLevel = spdlog::level::level_enum::info,
		                bool consoleWithColor = true);
		void Shutdown();

		void SetLevel(spdlog::level::level_enum lvl);

		template <typename... Args> void Trace(std::string_view fmt, Args&&... args) const
		{
			if (m_logger) m_logger->trace(spdlog::fmt_lib::runtime(fmt), std::forward<Args>(args)...);
		}

		template <typename... Args> void Debug(std::string_view fmt, Args&&... args) const
		{
			if (m_logger) m_logger->debug(spdlog::fmt_lib::runtime(fmt), std::forward<Args>(args)...);
		}

		template <typename... Args> void Info(std::string_view fmt, Args&&... args) const
		{
			if (m_logger) m_logger->info(spdlog::fmt_lib::runtime(fmt), std::forward<Args>(args)...);
		}

		template <typename... Args> void Warn(std::string_view fmt, Args&&... args) const
		{
			if (m_logger) m_logger->warn(spdlog::fmt_lib::runtime(fmt), std::forward<Args>(args)...);
		}

		template <typename... Args> void Error(std::string_view fmt, Args&&... args) const
		{
			if (m_logger) m_logger->error(spdlog::fmt_lib::runtime(fmt), std::forward<Args>(args)...);
		}

		template <typename... Args> void Critical(std::string_view fmt, Args&&... args) const
		{
			if (m_logger) m_logger->critical(spdlog::fmt_lib::runtime(fmt), std::forward<Args>(args)...);
		}

		std::shared_ptr<spdlog::logger> GetRawLogger() const { return m_logger; }

	  private:
		NeneLogger() = default;
		~NeneLogger() = default;

		std::shared_ptr<spdlog::logger> m_logger;
	};

} // namespace NeneEngine

#ifdef NENE_LOG_TRACE
#undef NENE_LOG_TRACE
#endif
#ifdef NENE_LOG_DEBUG
#undef NENE_LOG_DEBUG
#endif
#ifdef NENE_LOG_INFO
#undef NENE_LOG_INFO
#endif
#ifdef NENE_LOG_WARN
#undef NENE_LOG_WARN
#endif
// Fix define conflict with DiligentEngine
#ifdef NENE_LOG_ERROR
#undef NENE_LOG_ERROR
#endif
#ifdef NENE_LOG_CRITICAL
#undef NENE_LOG_CRITICAL
#endif

#define NENE_LOG_TRACE(...) ::NeneEngine::NeneLogger::GetInstance().Trace(__VA_ARGS__)
#define NENE_LOG_DEBUG(...) ::NeneEngine::NeneLogger::GetInstance().Debug(__VA_ARGS__)
#define NENE_LOG_INFO(...) ::NeneEngine::NeneLogger::GetInstance().Info(__VA_ARGS__)
#define NENE_LOG_WARN(...) ::NeneEngine::NeneLogger::GetInstance().Warn(__VA_ARGS__)
#define NENE_LOG_ERROR(...) ::NeneEngine::NeneLogger::GetInstance().Error(__VA_ARGS__)
#define NENE_LOG_CRITICAL(...) ::NeneEngine::NeneLogger::GetInstance().Critical(__VA_ARGS__)
