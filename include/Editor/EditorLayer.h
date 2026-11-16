// EditorLayer.h

#pragma once

#include "Editor/EditorAboutWindow.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/EditorSceneSnapshot.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorThemeService.h"
#include "Platform/IWindow.h"

#include <EASTL/functional.h>
#include <EASTL/string.h>
#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>

namespace NeneEngine
{
	class IImGuiBackend;
	class InputDevice;

	class EditorLayer final
	{
	  public:
		using RequestShutdownCallback = eastl::function<void()>;
		using ModeChangedCallback = eastl::function<void(EditorMode)>;

		EditorLayer();
		~EditorLayer();

		void Initialize(ECS::World& world, IImGuiBackend* imguiBackend, InputDevice* uiInput, ECS::Entity cameraEntity,
		                RequestShutdownCallback requestShutdown, ModeChangedCallback onModeChanged);
		void SetRenderer(IRenderAdapter* renderer);
		void SetWindow(IWindow* window);
		void SetResourceLoading(ResourceLoadQueue& loads, const SceneAssetResolver& assets);
		void Shutdown();

		void Update();
		[[nodiscard]] bool ConfirmDiscardChanges();
		void RequestExit() { m_exitRequested = true; }

		[[nodiscard]] bool IsGameplayRunning() const
		{
			return m_context == nullptr || m_context->mode == EditorMode::Play;
		}

	  private:
		struct TitleBarRect
		{
			ImVec2 min;
			ImVec2 max;
		};

		struct PanelEntry
		{
			eastl::unique_ptr<IEditorPanel> panel;
			bool isOpen = true;
			eastl::string windowTitle;
		};

		void HandleShortcuts();
		void DrawMainMenuBar();
		void DrawStatusBar();
		void SaveScene(bool saveAs);
		void LoadScene();
		void DrawHistoryMenuItems();
		void DrawThemeMenu();
		void SetTheme(EditorThemeVariant variant);
		void DrawPlayControls();
		void DrawWindowButtons();
		void AddTitleBarInteractiveRect(const ImVec2& min, const ImVec2& max);
		[[nodiscard]] WindowHitArea HitTestTitleBar(int32_t x, int32_t y) const;
		void SetMode(EditorMode mode);
		void DrawDockSpace();
		void BuildDefaultLayout(ImGuiID dockSpaceId, const ImVec2& size);
		void DrawPanels();
		void DrawDockedPanelCorners() const;
		void ApplyInputCapture();
		void LoadMascots();
		void ReleaseMascots();

		IImGuiBackend* m_imguiBackend = nullptr;
		InputDevice* m_uiInput = nullptr;
		IWindow* m_window = nullptr;
		TitleBarRect m_titleBar;
		TitleBarRect m_maximizeButtonRect;
		eastl::vector<TitleBarRect> m_titleBarInteractiveRects;
		RequestShutdownCallback m_requestShutdown;
		ModeChangedCallback m_onModeChanged;
		EditorHistory m_history;
		EditorSceneSnapshot m_sceneSnapshot;
		EditorThemeService m_themeService;
		EditorAboutWindow m_aboutWindow;
		eastl::unique_ptr<EditorContext> m_context;
		eastl::vector<PanelEntry> m_panels;
		std::filesystem::path m_scenePath;
		ResourceLoadQueue* m_resourceLoads = nullptr;
		uint64_t m_sceneLoadGeneration = 0;
		bool m_layoutInitialized = false;
		bool m_exitRequested = false;
	};

} // namespace NeneEngine
