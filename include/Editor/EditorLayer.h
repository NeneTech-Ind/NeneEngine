// EditorLayer.h

#pragma once

#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Editor/EditorSceneSnapshot.h"
#include "Editor/IEditorPanel.h"

#include <EASTL/functional.h>
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

		void Initialize(ECS::World& world, IImGuiBackend* imguiBackend, InputDevice* uiInput,
		                RequestShutdownCallback requestShutdown, ModeChangedCallback onModeChanged);
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
		};

		void HandleShortcuts();
		void DrawMainMenuBar();
		void DrawHistoryMenuItems();
		void DrawPlayControls();
		void SetMode(EditorMode mode);
		void DrawDockSpace();
		void BuildDefaultLayout(ImGuiID dockSpaceId);
		void DrawPanels();
		void ApplyInputCapture();

		IImGuiBackend* m_imguiBackend = nullptr;
		InputDevice* m_uiInput = nullptr;
		RequestShutdownCallback m_requestShutdown;
		ModeChangedCallback m_onModeChanged;
		EditorHistory m_history;
		EditorSceneSnapshot m_sceneSnapshot;
		eastl::unique_ptr<EditorContext> m_context;
		eastl::vector<PanelEntry> m_panels;
	};

} // namespace NeneEngine
