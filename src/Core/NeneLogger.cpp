// NeneLogger.cpp

#include "Core/NeneLogger.h"

#include "Core/LogHistorySink.h"

#include <EASTL/vector.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <functional>
#include <iostream>
#include <regex>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <vector>

namespace NeneEngine
{
	namespace
	{
		constexpr size_t kHistoryCapacity = 2000;
		constexpr size_t kLogFileLimit = 20;

		void RemoveOldLogs(const std::filesystem::path& logPath)
		{
			const std::wregex logName(LR"(nene_engine-[0-9]{8}-[0-9]{6}-[0-9]+-[0-9]+\.log)");
			if (!std::regex_match(logPath.filename().wstring(), logName)) return;

			std::error_code error;
			std::vector<std::filesystem::path> logs;
			std::filesystem::directory_iterator it(logPath.parent_path(), error);
			const std::filesystem::directory_iterator end;
			for (; !error && it != end; it.increment(error))
			{
				std::error_code entryError;
				if (it->is_regular_file(entryError) && std::regex_match(it->path().filename().wstring(), logName))
					logs.push_back(it->path());
			}
			if (error) return;

			std::sort(logs.begin(), logs.end(), std::greater<>());
			for (size_t index = kLogFileLimit; index < logs.size(); ++index)
			{
				if (logs[index] == logPath) continue;
				std::filesystem::remove(logs[index], error);
				error.clear();
			}
		}
	} // namespace

	NeneLogger& NeneLogger::GetInstance()
	{
		static NeneLogger instance;
		return instance;
	}

	bool NeneLogger::Initialize(const std::filesystem::path& logFileName, spdlog::level::level_enum logLevel, bool consoleWithColor)
	{
		try
		{
			Shutdown();

			std::string fileSinkError;
			eastl::vector<spdlog::sink_ptr> sinks;
			auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			console->set_pattern(consoleWithColor ? "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
			                                      : "[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
			sinks.push_back(console);

			m_history = std::make_shared<LogHistorySink>(kHistoryCapacity);
			sinks.push_back(m_history);

			if (!logFileName.empty())
			{
				const std::filesystem::path logPath{logFileName};
				if (const auto parentPath = logPath.parent_path(); !parentPath.empty())
				{
					std::error_code errorCode;
					std::filesystem::create_directories(parentPath, errorCode);
				}

				m_logFile.open(logPath, std::ios::out | std::ios::trunc | std::ios::binary);
				if (m_logFile.is_open())
				{
					RemoveOldLogs(logPath);
					auto file = std::make_shared<spdlog::sinks::ostream_sink_mt>(m_logFile);
					file->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
					sinks.push_back(file);
				}
				else
				{
					fileSinkError = "could not open log file";
				}
			}

			m_logger = std::make_shared<spdlog::logger>("game", sinks.begin(), sinks.end());

			m_logger->set_level(logLevel);
			m_logger->flush_on((std::max)(logLevel, spdlog::level::warn));

			spdlog::set_default_logger(m_logger);
			spdlog::flush_every(std::chrono::seconds(1));
			m_logger->info("Logger initialized (level={})", spdlog::level::to_string_view(logLevel));
			if (!fileSinkError.empty()) m_logger->error("Logger: file logging is disabled: {}", fileSinkError);

			return true;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << "spdlog initialization failed: " << ex.what() << std::endl;
			return false;
		}
	}

	void NeneLogger::Shutdown()
	{
		if (m_logger)
		{
			m_logger->flush();
			spdlog::set_default_logger({});
			spdlog::drop(m_logger->name());
			m_logger.reset();
		}
		m_history.reset();
		m_logFile.close();
	}

} // namespace NeneEngine
