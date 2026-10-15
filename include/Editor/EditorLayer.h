// EditorLayer.h

#pragma once

#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
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

		EditorLayer();
		~EditorLayer();

		void Initialize(ECS::World& world, IImGuiBackend* imguiBackend, InputDevice* uiInput,
		                RequestShutdownCallback requestShutdown);
		void Shutdown();

		void Update();

	  private:
		struct PanelEntry
		{
			eastl::unique_ptr<IEditorPanel> panel;
			bool isOpen = true;
		};

		void HandleShortcuts();
		void DrawMainMenuBar();
		void DrawHistoryMenuItems();
		void DrawDockSpace();
		void BuildDefaultLayout(ImGuiID dockSpaceId);
		void DrawPanels();
		void ApplyInputCapture();

		IImGuiBackend* m_imguiBackend = nullptr;
		InputDevice* m_uiInput = nullptr;
		RequestShutdownCallback m_requestShutdown;
		EditorHistory m_history;
		eastl::unique_ptr<EditorContext> m_context;
		eastl::vector<PanelEntry> m_panels;
	};

} // namespace NeneEngine
