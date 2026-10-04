#pragma once

#include "Core/EASTLStdHash.h"
#include "Input/KeyCode.h"

#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <cstdint>
#include <filesystem>
#include <string>

#include <glm/glm.hpp>

namespace NeneEngine
{
	struct WindowDefinitionConfig
	{
		std::string title = "NeneEngine";
		uint32_t width = 1280;
		uint32_t height = 720;
		bool isMain = false;
	};

	struct WindowConfig
	{
		glm::vec4 backgroundColor{0.1f, 0.1f, 0.2f, 1.0f};
	};

	struct InputConfig
	{
		eastl::unordered_map<std::string, eastl::vector<KeyCode>> actions{};
	};

	struct AppConfig
	{
		WindowConfig window{};
		InputConfig input{};
		eastl::vector<WindowDefinitionConfig> windows{};
	};

	[[nodiscard]] std::filesystem::path GetDefaultAppConfigPath();
	[[nodiscard]] AppConfig LoadAppConfig(const std::filesystem::path& configPath = GetDefaultAppConfigPath());
} // namespace NeneEngine
