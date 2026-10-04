// NeneLogger.cpp

#include "Core/NeneLogger.h"

#include <filesystem>
#include <iostream>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace NeneEngine
{

	NeneLogger& NeneLogger::GetInstance()
	{
		static NeneLogger instance;
		return instance;
	}

	bool NeneLogger::Initialize(const std::string& logFileName, bool async, spdlog::level::level_enum logLevel,
	                            bool consoleWithColor)
	{
		try
		{
			Shutdown();

			std::vector<spdlog::sink_ptr> sinks;
			auto console = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

			if (consoleWithColor)
			{
				console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");
				sinks.push_back(console);
			}
			else
			{
				console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
				sinks.push_back(console);
			}

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

			if (async)
			{
				spdlog::init_thread_pool(8192, 1);
				m_logger = std::make_shared<spdlog::async_logger>(
				    "game", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
			}
			else
			{
				m_logger = std::make_shared<spdlog::logger>("game", sinks.begin(), sinks.end());
			}

			m_logger->set_level(logLevel);
			m_logger->flush_on(logLevel);

			spdlog::set_default_logger(m_logger);
			m_logger->info("Logger initialized (async={}, level={})", async, spdlog::level::to_string_view(logLevel));

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
