// NeneLogger.cpp

#include "Core/NeneLogger.h"

#include <EASTL/vector.h>
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace NeneEngine
{

	NeneLogger& NeneLogger::GetInstance()
	{
		static NeneLogger instance;
		return instance;
	}

	bool NeneLogger::Initialize(const std::string& logFileName, spdlog::level::level_enum logLevel, bool consoleWithColor)
	{
		try
		{
			Shutdown();

			eastl::vector<spdlog::sink_ptr> sinks;
			auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
			console->set_pattern(consoleWithColor ? "[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v"
			                                      : "[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
			sinks.push_back(console);

			if (!logFileName.empty())
			{
				const std::filesystem::path logPath{logFileName};
				if (const auto parentPath = logPath.parent_path(); !parentPath.empty())
				{
					std::error_code errorCode;
					std::filesystem::create_directories(parentPath, errorCode);
				}

				auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(logFileName.c_str(), true);
				file->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
				sinks.push_back(file);
			}

			m_logger = std::make_shared<spdlog::logger>("game", sinks.begin(), sinks.end());

			m_logger->set_level(logLevel);
			m_logger->flush_on((std::max)(logLevel, spdlog::level::warn));

			spdlog::set_default_logger(m_logger);
			spdlog::flush_every(std::chrono::seconds(1));
			m_logger->info("Logger initialized (level={})", spdlog::level::to_string_view(logLevel));

			return true;
		}
		catch (const spdlog::spdlog_ex& ex)
		{
			std::cerr << "spdlog initialization failed: " << ex.what() << std::endl;
			return false;
		}
	}

	void NeneLogger::SetLevel(spdlog::level::level_enum lvl)
	{
		if (m_logger)
		{
			m_logger->set_level(lvl);
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
	}

} // namespace NeneEngine
