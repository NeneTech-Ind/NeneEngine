// EditorLayer.h

#pragma once

#include "Editor/EditorAboutWindow.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/EditorSceneSnapshot.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorThemeService.h"

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
		void Shutdown();

		void Update();

		[[nodiscard]] bool IsGameplayRunning() const
		{
			return m_context == nullptr || m_context->mode == EditorMode::Play;
		}

	  private:
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
		RequestShutdownCallback m_requestShutdown;
		ModeChangedCallback m_onModeChanged;
		EditorHistory m_history;
		EditorSceneSnapshot m_sceneSnapshot;
		EditorThemeService m_themeService;
		EditorAboutWindow m_aboutWindow;
		eastl::unique_ptr<EditorContext> m_context;
		eastl::vector<PanelEntry> m_panels;
		std::filesystem::path m_scenePath;
		bool m_layoutInitialized = false;
	};

} // namespace NeneEngine
