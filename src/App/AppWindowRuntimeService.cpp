// AppWindowRuntimeService.cpp

#include "App/AppWindowRuntimeService.h"

#include "App/AppInputBindingUtils.h"
#include "Core/CustomLogger.h"
#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Systems/CameraControllerSystem.h"
#include "ECS/Systems/PrimitiveControlSystem.h"
#include "ECS/World.h"
#include "Platform/Windows32/Windows32Window.h"
#include "RenderAdapters/DiligentDX12Adapter.h"

#include <Windows.h>

namespace NeneEngine
{
	namespace
	{
		std::wstring AnsiToWString(const std::string& str)
		{
			WCHAR buffer[512];
			MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
			return std::wstring(buffer);
		}
	} // namespace

	AppWindowRuntimeService::~AppWindowRuntimeService()
	{
		Shutdown();
	}

	bool AppWindowRuntimeService::Initialize(const AppConfig& config, ECS::World& world, ECS::Entity primaryCameraEntity,
	                                         uint32_t width, uint32_t height)
	{
		m_world = &world;

		if (config.windows.empty())
		{
			NENE_LOG_ERROR("Init failed: app config does not contain any windows");
			return false;
		}

		size_t mainWindowCount = 0;
		size_t secondaryWindowCount = 0;
		for (const auto& windowConfig : config.windows)
		{
			if (windowConfig.isMain)
				++mainWindowCount;
			else
				++secondaryWindowCount;
		}

		if (mainWindowCount == 0)
		{
			NENE_LOG_ERROR("Init failed: no main window defined in config");
			return false;
		}

		if (mainWindowCount > 1)
			NENE_LOG_WARN("App config: multiple windows marked as main, only the first one will control the primary "
			              "camera");

		m_windows.reserve(config.windows.size());

		const auto secondaryCameraEntities =
		    CreateAdditionalWindowCameras(primaryCameraEntity, secondaryWindowCount, width, height);
		size_t secondaryCameraIndex = 0;
		bool mainWindowCreated = false;

		for (const auto& windowConfig : config.windows)
		{
			ECS::Entity cameraEntity = primaryCameraEntity;
			if (windowConfig.isMain && !mainWindowCreated)
			{
				mainWindowCreated = true;
			}
			else
			{
				if (secondaryCameraIndex >= secondaryCameraEntities.size())
				{
					NENE_LOG_ERROR("Init failed: not enough secondary cameras for configured windows");
					return false;
				}

				cameraEntity = secondaryCameraEntities[secondaryCameraIndex++];
			}

			if (!CreateWindowContext(windowConfig.width, windowConfig.height, windowConfig.title, cameraEntity))
				return false;
		}

		return true;
	}

	void AppWindowRuntimeService::Shutdown()
	{
		for (auto& windowContext : m_windows)
		{
			if (windowContext.window && windowContext.resizeHandle.IsValid())
			{
				windowContext.window->OnResized().Remove(windowContext.resizeHandle);
				windowContext.resizeHandle.Reset();
			}
		}

		m_appSystems.clear();
		m_windows.clear();
		m_world = nullptr;
	}

	bool AppWindowRuntimeService::CreateWindowContext(uint32_t width, uint32_t height, const std::string& title,
	                                                  ECS::Entity cameraEntity)
	{
		WindowContext windowContext{};
		windowContext.title = title;
		windowContext.cameraEntity = cameraEntity;
		windowContext.window = eastl::make_unique<Windows32Window>();
		if (!windowContext.window->Create(width, height, title))
		{
			NENE_LOG_ERROR("Failed to create window '{}'", title);
			return false;
		}

		windowContext.renderer = eastl::make_unique<DiligentDX12Adapter>();
		if (!windowContext.renderer->Init(windowContext.window->GetHWND(), width, height))
		{
			NENE_LOG_ERROR("Failed to initialize renderer for window '{}'", title);
			return false;
		}

		const size_t windowIndex = m_windows.size();
		windowContext.resizeHandle =
		    windowContext.window->OnResized().AddLambda([this, windowIndex](uint32_t newWidth, uint32_t newHeight)
		                                                { HandleWindowResize(windowIndex, newWidth, newHeight); });

		m_windows.push_back(std::move(windowContext));
		auto& storedWindowContext = m_windows.back();
		storedWindowContext.inputManager.SetInputDevice(&storedWindowContext.window->GetInput());
		storedWindowContext.renderSystem =
		    std::make_unique<ECS::RenderSystem>(storedWindowContext.renderer.get(), cameraEntity);
		AddAppSystem(std::make_unique<ECS::CameraControllerSystem>(storedWindowContext.inputManager, cameraEntity));
		AddAppSystem(std::make_unique<ECS::PrimitiveControlSystem>(storedWindowContext.inputManager));

		HandleWindowResize(windowIndex, width, height);
		return true;
	}

	void AppWindowRuntimeService::AddAppSystem(std::unique_ptr<ECS::ISystem> system)
	{
		m_appSystems.push_back(std::move(system));
	}

	std::vector<ECS::Entity> AppWindowRuntimeService::CreateAdditionalWindowCameras(ECS::Entity primaryCameraEntity,
	                                                                                 size_t count, uint32_t width,
	                                                                                 uint32_t height) const
	{
		std::vector<ECS::Entity> secondaryCameraEntities;
		secondaryCameraEntities.reserve(count);

		if (m_world == nullptr) return secondaryCameraEntities;
		if (primaryCameraEntity == ECS::NullEntity) return secondaryCameraEntities;

		const auto* primaryTransform = m_world->GetComponent<ECS::TransformComponent>(primaryCameraEntity);
		const auto* primaryCamera = m_world->GetComponent<ECS::CameraComponent>(primaryCameraEntity);
		const auto* primaryController = m_world->GetComponent<ECS::CameraControllerComponent>(primaryCameraEntity);
		if (primaryTransform == nullptr || primaryCamera == nullptr || primaryController == nullptr)
			return secondaryCameraEntities;

		for (size_t cameraIndex = 0; cameraIndex < count; ++cameraIndex)
		{
			const ECS::Entity secondaryCameraEntity =
			    m_world->CreateEntity("SecondaryCamera" + std::to_string(cameraIndex + 1));
			auto& secondaryTransform = m_world->AddComponent<ECS::TransformComponent>(secondaryCameraEntity);
			secondaryTransform = *primaryTransform;
			secondaryTransform.position.x += 2.0f * static_cast<float>(cameraIndex + 1);

			auto& secondaryCamera = m_world->AddComponent<ECS::CameraComponent>(secondaryCameraEntity);
			secondaryCamera = *primaryCamera;
			secondaryCamera.aspectRatio = height == 0 ? 1.0f : static_cast<float>(width) / static_cast<float>(height);
			secondaryCamera.isPrimary = false;

			auto& secondaryController = m_world->AddComponent<ECS::CameraControllerComponent>(secondaryCameraEntity);
			secondaryController = *primaryController;

			secondaryCameraEntities.push_back(secondaryCameraEntity);
		}

		return secondaryCameraEntities;
	}

	bool AppWindowRuntimeService::AreAllWindowsClosed() const
	{
		if (m_windows.empty()) return true;

		for (const auto& windowContext : m_windows)
		{
			if (windowContext.window && !windowContext.window->ShouldClose()) return false;
		}

		return true;
	}

	void AppWindowRuntimeService::ApplyRuntimeConfig(const AppConfig& config)
	{
		for (size_t index = 0; index < m_windows.size(); ++index)
		{
			auto& windowContext = m_windows[index];
			const std::string managerName = "window input manager[" + std::to_string(index) + "]";
			ApplyInputBindings(windowContext.inputManager, config.input, managerName);
			if (windowContext.renderer) windowContext.renderer->SetClearColor(config.window.backgroundColor);
		}
	}

	void AppWindowRuntimeService::HandleWindowResize(size_t windowIndex, uint32_t width, uint32_t height)
	{
		if (windowIndex >= m_windows.size()) return;

		if (width == 0 || height == 0)
		{
			NENE_LOG_WARN("Application resize ignored for window {} with invalid size {}x{}", windowIndex, width,
			              height);
			return;
		}

		auto& windowContext = m_windows[windowIndex];
		if (windowContext.renderer) windowContext.renderer->Resize(width, height);

		if (m_world != nullptr)
		{
			if (auto* camera = m_world->GetComponent<ECS::CameraComponent>(windowContext.cameraEntity))
				camera->aspectRatio = static_cast<float>(width) / static_cast<float>(height);
		}

		NENE_LOG_INFO("Window '{}' resized to {}x{}", windowContext.title, width, height);
	}

	void AppWindowRuntimeService::PumpWindowMessages()
	{
		for (auto& windowContext : m_windows)
		{
			if (windowContext.window && !windowContext.window->ShouldClose()) windowContext.window->PumpMessages();
		}
	}

	void AppWindowRuntimeService::UpdateInputManagers()
	{
		for (auto& windowContext : m_windows)
		{
			if (windowContext.window) windowContext.inputManager.UpdateState();
		}
	}

	void AppWindowRuntimeService::UpdateWindowSystems(float deltaTime)
	{
		if (m_world == nullptr) return;
		for (auto& system : m_appSystems) system->Update(*m_world, deltaTime);
	}

	void AppWindowRuntimeService::Render()
	{
		if (m_world == nullptr) return;

		for (auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;
			if (!windowContext.renderer || !windowContext.renderSystem) continue;

			windowContext.renderer->BeginFrame();
			windowContext.renderSystem->Render(*m_world);
			windowContext.renderer->EndFrame();
			windowContext.renderer->Present();
		}
	}

	void AppWindowRuntimeService::EndFrameInputs()
	{
		for (auto& windowContext : m_windows)
		{
			if (windowContext.window) windowContext.window->GetInput().EndFrame();
		}
	}

	void AppWindowRuntimeService::UpdateFrameStatsText(const std::wstring& fpsText, const std::wstring& mspfText) const
	{
		for (const auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;

			std::wstring windowText =
			    AnsiToWString(windowContext.window->GetTitle()) + L" | fps: " + fpsText + L" | mspf: " + mspfText;

			SetWindowTextW(windowContext.window->GetHWND(), windowText.c_str());
		}
	}

	InputDevice* AppWindowRuntimeService::GetFocusedInput()
	{
		for (auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;

			InputDevice& input = windowContext.window->GetInput();
			if (input.IsFocused()) return &input;
		}

		return nullptr;
	}

	const InputDevice* AppWindowRuntimeService::GetFocusedInput() const
	{
		for (const auto& windowContext : m_windows)
		{
			if (!windowContext.window || windowContext.window->ShouldClose()) continue;

			const InputDevice& input = windowContext.window->GetInput();
			if (input.IsFocused()) return &input;
		}

		return nullptr;
	}

	IRenderAdapter* AppWindowRuntimeService::GetPrimaryRenderer()
	{
		if (m_windows.empty()) return nullptr;
		return m_windows.front().renderer.get();
	}

} // namespace NeneEngine
