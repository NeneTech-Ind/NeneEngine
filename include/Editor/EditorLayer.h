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
#include <chrono>
#include <string>

namespace NeneEngine
{
	class IImGuiBackend;
	class InputDevice;
	class ScriptEngine;
	class PrefabLibrary;

	class EditorLayer final
	{
	  public:
		using RequestShutdownCallback = eastl::function<void()>;
		using ModeChangedCallback = eastl::function<void(EditorMode)>;

		EditorLayer();
		~EditorLayer();

		void Initialize(ECS::World& world, ScriptEngine& scriptEngine, IImGuiBackend* imguiBackend, InputDevice* uiInput,
		                ECS::Entity cameraEntity, RequestShutdownCallback requestShutdown,
		                ModeChangedCallback onModeChanged);
		void SetRenderer(IRenderAdapter* renderer);
		void SetWindow(IWindow* window);
		void SetResourceLoading(ResourceLoadQueue& loads, const SceneAssetResolver& assets);
		void SetPrefabLibrary(PrefabLibrary& prefabs);
		void SetWindowCameras(eastl::vector<ECS::Entity> cameras);
		void Shutdown();

		void Update();
		[[nodiscard]] bool HandleWindowCloseRequest();
		void RequestExit() { m_exitRequested = true; }

		[[nodiscard]] bool IsGameplayRunning() const
		{
			return m_context == nullptr || m_context->mode == EditorMode::Play;
		}

	  private:
		enum class SceneAction
		{
			None,
			Load,
			Exit
		};

		struct TitleBarRect
		{
			ImVec2 min;
			ImVec2 max;
		};

		struct PanelEntry
		{
			eastl::unique_ptr<IEditorPanel> panel;
			bool isOpenByDefault = true;
			bool isOpen = isOpenByDefault;
			eastl::string windowTitle;
			bool focusRequested = false;
		};

		void HandleShortcuts();
		void CommitPendingEdits();
		void DrawMainMenuBar();
		void ReloadScripts();
		void DrawStatusBar();
		void OpenConsole();
		bool SaveScene(bool saveAs, std::string* errorMessage = nullptr);
		void RequestSceneAction(SceneAction action);
		void RunSceneAction(SceneAction action);
		void DrawSaveChangesDialog();
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
		ScriptEngine* m_scriptEngine = nullptr;
		std::chrono::steady_clock::time_point m_nextScriptReloadCheck{};
		EditorStatus m_scriptReloadStatus;
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
		SceneAction m_pendingSceneAction = SceneAction::None;
		std::string m_saveChangesError;
	};

} // namespace NeneEngine
