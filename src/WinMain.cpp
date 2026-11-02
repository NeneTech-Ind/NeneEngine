#include "App/NeneEngineApp.h"
#include <iostream>
#include <sstream>
#include <string_view>

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
