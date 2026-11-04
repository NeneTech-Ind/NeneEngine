#include "App/NeneEngineApp.h"
#include "Core/Profiler.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <string_view>
#include <thread>

namespace
{
	bool HasCommandLineFlag(LPWSTR commandLine, std::wstring_view flag)
	{
		if (commandLine == nullptr || *commandLine == L'\0') return false;

		std::wistringstream input(commandLine);
		std::wstring argument;
		while (input >> argument)
		{
			if (argument == flag) return true;
		}

		return false;
	}

	void WaitForProfilerConnection()
	{
#ifdef NENEENGINE_ENABLE_TRACY
		constexpr auto kWaitTimeout = std::chrono::seconds{30};
		constexpr auto kPollInterval = std::chrono::milliseconds{10};
		const auto deadline = std::chrono::steady_clock::now() + kWaitTimeout;

		OutputDebugStringA("30 seconds for the profiler to connect\n");
		while (!NENE_PROFILE_IS_CONNECTED())
		{
			if (std::chrono::steady_clock::now() >= deadline)
			{
				OutputDebugStringA("connection timed out\n");
				return;
			}

			std::this_thread::sleep_for(kPollInterval);
		}

		OutputDebugStringA("profiler connected\n");
#else
		OutputDebugStringA("profiler wait skipped because profiling is disabled\n");
#endif
	}

	void ReportFatalError(const std::string& message, bool showDialog)
	{
		std::cerr << message << "\n";
		if (showDialog) MessageBoxA(nullptr, message.c_str(), "NeneEngine", MB_OK | MB_ICONERROR);
	}
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR commandLine, int)
{
	const bool smokeTestMode = HasCommandLineFlag(commandLine, L"--smoke-test");

	try
	{
		if (HasCommandLineFlag(commandLine, L"--wait-for-profiler")) WaitForProfilerConnection();

		NeneEngine::NeneEngineApp app;
		if (!app.Init(1280, 720))
		{
			ReportFatalError("Failed to initialize the application. See logs\nene_engine.log next to the executable.",
			                 !smokeTestMode);
			return -1;
		}

		if (smokeTestMode) return 0;

		app.Run();
		return 0;
	}
	catch (const std::exception& e)
	{
		ReportFatalError(std::string("Fatal error: ") + e.what(), !smokeTestMode);
		return -1;
	}
	catch (...)
	{
		ReportFatalError("Fatal error: unknown exception", !smokeTestMode);
		return -1;
	}
}
