// DebugConsole.cpp

#include "Core/DebugConsole.h"

#ifndef NDEBUG
#include <Windows.h>

#include <cstdio>
#include <iostream>
#include <string>
#endif

namespace NeneEngine::DebugConsole
{
#ifndef NDEBUG
	namespace
	{
		constexpr short kSpatialHeaderRow = 2;
		constexpr short kLogRow = 9;
		constexpr DWORD kClearWidth = 96;

		bool g_initialized = false;
		HANDLE g_consoleOutput = nullptr;

		void WriteLineAt(short row, const std::string& text)
		{
			if (g_consoleOutput == nullptr) return;

			const COORD position{0, row};
			SetConsoleCursorPosition(g_consoleOutput, position);

			std::string line = text;
			if (line.size() < kClearWidth) line.append(kClearWidth - line.size(), ' ');

			DWORD written = 0;
			WriteConsoleA(g_consoleOutput, line.c_str(), static_cast<DWORD>(line.size()), &written, nullptr);
		}

		void MoveCursorToLogArea()
		{
			if (g_consoleOutput == nullptr) return;
			SetConsoleCursorPosition(g_consoleOutput, COORD{0, kLogRow});
		}
	} // namespace

	void Initialize()
	{
		if (g_initialized) return;
		if (!AllocConsole()) return;

		g_initialized = true;
		g_consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

		SetConsoleTitleW(L"NeneEngine Debug Console");
		SetConsoleOutputCP(CP_UTF8);
		SetConsoleCP(CP_UTF8);

		FILE* stream = nullptr;
		freopen_s(&stream, "CONOUT$", "w", stdout);
		freopen_s(&stream, "CONOUT$", "w", stderr);
		freopen_s(&stream, "CONIN$", "r", stdin);

		std::cout.clear();
		std::cerr.clear();
		std::cin.clear();

		WriteLineAt(0, "NeneEngine Debug Console");
		WriteLineAt(1, "Static debug status");
		WriteLineAt(kSpatialHeaderRow, "Spatial culling: waiting for first frame...");
		WriteLineAt(8, "Logs:");
		MoveCursorToLogArea();
	}

	void UpdateSpatialCullingStats(uint32_t total, uint32_t indexed, std::size_t candidates, uint32_t frustum,
	                               uint32_t submitted)
	{
		if (!g_initialized) return;

		WriteLineAt(kSpatialHeaderRow, "Spatial culling");
		WriteLineAt(kSpatialHeaderRow + 1, "  total renderables : " + std::to_string(total));
		WriteLineAt(kSpatialHeaderRow + 2, "  indexed visible   : " + std::to_string(indexed));
		WriteLineAt(kSpatialHeaderRow + 3, "  grid candidates   : " + std::to_string(candidates));
		WriteLineAt(kSpatialHeaderRow + 4, "  frustum accepted  : " + std::to_string(frustum));
		WriteLineAt(kSpatialHeaderRow + 5, "  submitted         : " + std::to_string(submitted));
		MoveCursorToLogArea();
	}
#else
	void Initialize() {}

	void UpdateSpatialCullingStats(uint32_t, uint32_t, std::size_t, uint32_t, uint32_t) {}
#endif

} // namespace NeneEngine::DebugConsole
