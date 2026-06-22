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
} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR commandLine, int)
{
	try
	{
		const bool smokeTestMode = HasCommandLineFlag(commandLine, L"--smoke-test");

		NeneEngine::NeneEngineApp app;
		if (!app.Init(1280, 720, "NeneEngine"))
		{
			std::cerr << "Failed to initialize application\n";
			return -1;
		}

		if (smokeTestMode) return 0;

		app.Run();
		return 0;
	}
	catch (const std::exception& e)
	{
		std::cerr << "Fatal error: " << e.what() << "\n";
		return -1;
	}
}
