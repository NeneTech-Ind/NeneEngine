#include "App/AppConfig.h"

#include "Core/PathResolver.h"
#include "Input/InputActions.h"
#include "Input/KeyCodeStrings.h"

#include <fstream>
#include <nlohmann/json.hpp>

#include "../external/DiligentEngine/DiligentCore/Graphics/GraphicsAccessories/interface/ColorConversion.h"
#include "Core/CustomLogger.h"

#include <algorithm>

namespace NeneEngine
{
	namespace
	{
		constexpr glm::vec4 kDefaultBackgroundColor{0.1f, 0.1f, 0.2f, 1.0f};
		constexpr uint32_t kDefaultWindowWidth = 1280;
		constexpr uint32_t kDefaultWindowHeight = 720;
		constexpr std::string_view kDefaultWindowTitle = "NeneEngine";

		[[nodiscard]] InputConfig DefaultInputConfig()
		{
			InputConfig config{};
			config.actions[std::string(InputActions::Pause)] = {KeyCode::Escape};
			config.actions[std::string(InputActions::Quit)] = {KeyCode::Q};
			config.actions[std::string(InputActions::MoveForward)] = {KeyCode::W};
			config.actions[std::string(InputActions::MoveBackward)] = {KeyCode::S};
			config.actions[std::string(InputActions::MoveLeft)] = {KeyCode::A};
			config.actions[std::string(InputActions::MoveRight)] = {KeyCode::D};
			config.actions[std::string(InputActions::MoveUp)] = {KeyCode::Space};
			config.actions[std::string(InputActions::MoveDown)] = {KeyCode::LeftControl, KeyCode::RightControl};
			config.actions[std::string(InputActions::Sprint)] = {KeyCode::LeftShift, KeyCode::RightShift};
			config.actions[std::string(InputActions::LookModifier)] = {KeyCode::MouseRight};
			config.actions[std::string(InputActions::PrimitiveMoveUp)] = {KeyCode::Up};
			config.actions[std::string(InputActions::PrimitiveMoveDown)] = {KeyCode::Down};
			config.actions[std::string(InputActions::PrimitiveMoveLeft)] = {KeyCode::Left};
			config.actions[std::string(InputActions::PrimitiveMoveRight)] = {KeyCode::Right};
			config.actions[std::string(InputActions::ScaleStep)] = {KeyCode::MouseLeft};
			config.actions[std::string(InputActions::RotateStep)] = {KeyCode::MouseRight};
			return config;
		}

		[[nodiscard]] glm::vec4 ReadBackgroundColorOrDefault(const nlohmann::json& root)
		{
			const auto windowIt = root.find("window");
			if (windowIt == root.end() || !windowIt->is_object())
			{
				NENE_LOG_WARN("App config: 'window' section is missing or invalid, using default background color");
				return kDefaultBackgroundColor;
			}

			const auto colorIt = windowIt->find("backgroundColor");
			if (colorIt == windowIt->end() || !colorIt->is_object())
			{
				NENE_LOG_WARN(
				    "App config: 'window.backgroundColor' is missing or invalid, using default background color");
				return kDefaultBackgroundColor;
			}

			const auto& color = *colorIt;
			const auto hasValidComponents = color.contains("r") && color["r"].is_number() && color.contains("g") &&
			                                color["g"].is_number() && color.contains("b") && color["b"].is_number();

			if (!hasValidComponents)
			{
				NENE_LOG_WARN(
				    "App config: 'window.backgroundColor' must contain numeric r/g/b, using default background color");
				return kDefaultBackgroundColor;
			}

			glm::vec4 parsedColor{color["r"].get<float>(), color["g"].get<float>(), color["b"].get<float>(), 1.0f};

			const bool hasValidRgbRange = parsedColor.r >= 0.0f && parsedColor.r <= 255.0f && parsedColor.g >= 0.0f &&
			                              parsedColor.g <= 255.0f && parsedColor.b >= 0.0f && parsedColor.b <= 255.0f;
			if (!hasValidRgbRange)
			{
				NENE_LOG_WARN("App config: 'window.backgroundColor' RGB components must be in 0..255, using default "
				              "background color");
				return kDefaultBackgroundColor;
			}

			parsedColor.r /= 255.0f;
			parsedColor.g /= 255.0f;
			parsedColor.b /= 255.0f;

			// Config RGB values are treated as standard sRGB palette bytes.
			// Convert them to linear space before clearing the render target.
			parsedColor.r = Diligent::GammaToLinear(parsedColor.r);
			parsedColor.g = Diligent::GammaToLinear(parsedColor.g);
			parsedColor.b = Diligent::GammaToLinear(parsedColor.b);

			return parsedColor;
		}

		[[nodiscard]] std::vector<WindowDefinitionConfig> ReadWindowsOrDefault(const nlohmann::json& root)
		{
			std::vector<WindowDefinitionConfig> windows;

			const auto windowsIt = root.find("windows");
			if (windowsIt == root.end())
			{
				windows.push_back(WindowDefinitionConfig{std::string(kDefaultWindowTitle), kDefaultWindowWidth,
				                                         kDefaultWindowHeight, true});
				return windows;
			}

			if (!windowsIt->is_array() || windowsIt->empty())
			{
				NENE_LOG_WARN("App config: 'windows' must be a non-empty array, using default main window");
				windows.push_back(WindowDefinitionConfig{std::string(kDefaultWindowTitle), kDefaultWindowWidth,
				                                         kDefaultWindowHeight, true});
				return windows;
			}

			bool hasMainWindow = false;
			size_t windowIndex = 0;
			for (const auto& windowValue : *windowsIt)
			{
				++windowIndex;
				if (!windowValue.is_object())
				{
					NENE_LOG_WARN("App config: windows[{}] is not an object, skipping", windowIndex - 1);
					continue;
				}

				WindowDefinitionConfig windowConfig{};
				windowConfig.title = windowValue.value("title", std::string(kDefaultWindowTitle));
				windowConfig.width = windowValue.value("width", kDefaultWindowWidth);
				windowConfig.height = windowValue.value("height", kDefaultWindowHeight);
				windowConfig.isMain = windowValue.value("isMain", false);

				if (windowConfig.title.empty()) windowConfig.title = std::string(kDefaultWindowTitle);

				if (windowConfig.width == 0 || windowConfig.height == 0)
				{
					NENE_LOG_WARN("App config: windows[{}] has invalid size {}x{}, skipping", windowIndex - 1,
					              windowConfig.width, windowConfig.height);
					continue;
				}

				hasMainWindow = hasMainWindow || windowConfig.isMain;
				windows.push_back(std::move(windowConfig));
			}

			if (windows.empty())
			{
				NENE_LOG_WARN("App config: no valid window definitions found, using default main window");
				windows.push_back(WindowDefinitionConfig{std::string(kDefaultWindowTitle), kDefaultWindowWidth,
				                                         kDefaultWindowHeight, true});
				return windows;
			}

			if (!hasMainWindow)
			{
				NENE_LOG_WARN("App config: no window marked as main, first window will be used as main");
				windows.front().isMain = true;
			}

			return windows;
		}

		[[nodiscard]] InputConfig ReadInputConfigOrDefault(const nlohmann::json& root)
		{
			InputConfig config = DefaultInputConfig();

			const auto inputIt = root.find("input");
			if (inputIt == root.end())
			{
				return config;
			}

			if (!inputIt->is_object())
			{
				NENE_LOG_WARN("App config: 'input' section is invalid, using default input bindings");
				return config;
			}

			const auto actionsIt = inputIt->find("actions");
			if (actionsIt == inputIt->end())
			{
				return config;
			}

			if (!actionsIt->is_object())
			{
				NENE_LOG_WARN("App config: 'input.actions' must be an object, using default input bindings");
				return config;
			}

			config.actions.clear();
			for (const auto& [actionName, actionValue] : actionsIt->items())
			{
				std::vector<KeyCode> bindings;

				if (actionValue.is_string())
				{
					if (const auto keyCode = TryParseKeyCode(actionValue.get<std::string>()); keyCode.has_value())
					{
						bindings.push_back(*keyCode);
					}
					else
					{
						NENE_LOG_WARN("App config: input.actions.{} contains unknown key '{}', skipping binding",
						              actionName, actionValue.get<std::string>());
					}
				}
				else if (actionValue.is_array())
				{
					for (const auto& bindingValue : actionValue)
					{
						if (!bindingValue.is_string())
						{
							NENE_LOG_WARN(
							    "App config: input.actions.{} must contain only string key names, skipping entry",
							    actionName);
							continue;
						}

						const std::string keyName = bindingValue.get<std::string>();
						if (const auto keyCode = TryParseKeyCode(keyName); keyCode.has_value())
						{
							if (std::find(bindings.begin(), bindings.end(), *keyCode) == bindings.end())
								bindings.push_back(*keyCode);
						}
						else
						{
							NENE_LOG_WARN("App config: input.actions.{} contains unknown key '{}', skipping binding",
							              actionName, keyName);
						}
					}
				}
				else
				{
					NENE_LOG_WARN("App config: input.actions.{} must be a string or array of strings, skipping",
					              actionName);
				}

				if (!bindings.empty()) config.actions[actionName] = std::move(bindings);
			}

			if (config.actions.empty())
			{
				NENE_LOG_WARN("App config: no valid input bindings found, using default input bindings");
				return DefaultInputConfig();
			}

			return config;
		}

	} // namespace

	std::filesystem::path DefaultAppConfigPath()
	{
		if (const auto resolvedPath =
		        ResolveFromExecutionRoots(std::filesystem::path{"assets"} / "config" / "engine.json");
		    !resolvedPath.empty())
			return resolvedPath;

		return std::filesystem::path{"assets"} / "config" / "engine.json";
	}

	AppConfig LoadAppConfig(const std::filesystem::path& configPath)
	{
		AppConfig config{};
		config.window.backgroundColor = kDefaultBackgroundColor;
		config.input = DefaultInputConfig();
		config.windows.push_back(
		    WindowDefinitionConfig{std::string(kDefaultWindowTitle), kDefaultWindowWidth, kDefaultWindowHeight, true});

		std::ifstream input(configPath);
		if (!input.is_open())
		{
			NENE_LOG_WARN("App config: failed to open '{}', using defaults", configPath.string());
			return config;
		}

		try
		{
			nlohmann::json root;
			input >> root;

			config.window.backgroundColor = ReadBackgroundColorOrDefault(root);
			config.input = ReadInputConfigOrDefault(root);
			config.windows = ReadWindowsOrDefault(root);
			NENE_LOG_INFO(
			    "App config loaded from '{}'; backgroundColor=({:.2f}, {:.2f}, {:.2f}, {:.2f}), windows={}, actions={}",
			              configPath.string(), config.window.backgroundColor.r, config.window.backgroundColor.g,
			              config.window.backgroundColor.b, config.window.backgroundColor.a, config.windows.size(),
			              config.input.actions.size());
		}
		catch (const std::exception& ex)
		{
			NENE_LOG_WARN("App config: failed to parse '{}': {}. Using defaults", configPath.string(), ex.what());
		}

		return config;
	}
} // namespace NeneEngine
