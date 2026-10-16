// EditorLayer.cpp

#include "Editor/EditorLayer.h"

#include "Core/NeneLogger.h"
#include "ECS/World.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/StatisticsPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Graphics/Backend/IImGuiBackend.h"
#include "Input/InputDevice.h"

#include <EASTL/string.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kDockSpaceName = "EditorDockSpace";
		constexpr float kLeftDockRatio = 0.2f;
		constexpr float kRightDockRatio = 0.25f;
		constexpr float kBottomDockRatio = 0.25f;
		constexpr ImU32 kPlayModeMenuBarColor = IM_COL32(90, 50, 20, 255);
		constexpr ImU32 kStopButtonColor = IM_COL32(170, 60, 40, 255);
		constexpr ImU32 kStopButtonHoveredColor = IM_COL32(200, 80, 55, 255);
	} // namespace

	EditorLayer::EditorLayer() = default;

	EditorLayer::~EditorLayer()
	{
		Shutdown();
	}

	void EditorLayer::Initialize(ECS::World& world, IImGuiBackend* imguiBackend, InputDevice* uiInput,
	                             RequestShutdownCallback requestShutdown, ModeChangedCallback onModeChanged)
	{
		Shutdown();

		if (imguiBackend == nullptr)
		{
			NENE_LOG_WARN("Editor disabled: the main window has no ImGui backend");
			return;
		}

		m_imguiBackend = imguiBackend;
		m_uiInput = uiInput;
		m_requestShutdown = std::move(requestShutdown);
		m_onModeChanged = std::move(onModeChanged);
		m_context = eastl::make_unique<EditorContext>(EditorContext{world, m_history});

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;

		m_panels.push_back({eastl::make_unique<HierarchyPanel>()});
		m_panels.push_back({eastl::make_unique<InspectorPanel>()});
		m_panels.push_back({eastl::make_unique<StatisticsPanel>()});
		m_panels.push_back({eastl::make_unique<ViewportPanel>()});

		NENE_LOG_INFO("Editor initialized with {} panels", m_panels.size());
	}

	void EditorLayer::Shutdown()
	{
		if (m_uiInput != nullptr)
		{
			m_uiInput->SetMouseCaptured(false);
			m_uiInput->SetKeyboardCaptured(false);
		}

		m_panels.clear();
		m_history.Clear();
		m_sceneSnapshot.Clear();
		m_context.reset();
		m_requestShutdown = nullptr;
		m_onModeChanged = nullptr;
		m_uiInput = nullptr;
		m_imguiBackend = nullptr;
	}

	void EditorLayer::Update()
	{
		if (m_imguiBackend == nullptr) return;

		m_imguiBackend->BeginFrame();

		m_context->isViewportHovered = false;
		m_context->isViewportFocused = false;

		HandleShortcuts();
		DrawMainMenuBar();
		DrawDockSpace();
		DrawPanels();

		ApplyInputCapture();
	}

	void EditorLayer::DrawMainMenuBar()
	{
		const bool isPlaying = m_context->mode == EditorMode::Play;
		if (isPlaying) ImGui::PushStyleColor(ImGuiCol_MenuBarBg, kPlayModeMenuBarColor);
		const bool isMenuBarOpen = ImGui::BeginMainMenuBar();
		if (isPlaying) ImGui::PopStyleColor();
		if (!isMenuBarOpen) return;

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit") && m_requestShutdown) m_requestShutdown();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Edit"))
		{
			DrawHistoryMenuItems();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			for (auto& entry : m_panels) ImGui::MenuItem(entry.panel->GetName(), nullptr, &entry.isOpen);
			ImGui::EndMenu();
		}

		DrawPlayControls();
		ImGui::EndMainMenuBar();
	}

	void EditorLayer::DrawHistoryMenuItems()
	{
		const bool isEditMode = m_context->mode == EditorMode::Edit;

		const char* undoName = m_history.GetUndoName();
		const eastl::string undoLabel = undoName != nullptr ? eastl::string("Undo ") + undoName : "Undo";
		if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, isEditMode && m_history.CanUndo()))
			m_history.Undo(m_context->world);

		const char* redoName = m_history.GetRedoName();
		const eastl::string redoLabel = redoName != nullptr ? eastl::string("Redo ") + redoName : "Redo";
		if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, isEditMode && m_history.CanRedo()))
			m_history.Redo(m_context->world);
	}

	void EditorLayer::DrawPlayControls()
	{
		const bool isPlaying = m_context->mode == EditorMode::Play;
		const ImGuiStyle& style = ImGui::GetStyle();
		const float buttonWidth =
		    (std::max)(ImGui::CalcTextSize("Play").x, ImGui::CalcTextSize("Stop").x) + style.FramePadding.x * 2.0f;
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);

		if (isPlaying)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, kStopButtonColor);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kStopButtonHoveredColor);
		}

		if (ImGui::Button(isPlaying ? "Stop" : "Play", ImVec2(buttonWidth, 0.0f)))
			SetMode(isPlaying ? EditorMode::Edit : EditorMode::Play);
		ImGui::SetItemTooltip(isPlaying ? "Stop and restore the scene (Ctrl+P)" : "Start the game (Ctrl+P)");

		if (isPlaying) ImGui::PopStyleColor(2);
	}

	void EditorLayer::SetMode(EditorMode mode)
	{
		if (m_context->mode == mode) return;

		ECS::World& world = m_context->world;
		if (mode == EditorMode::Play)
		{
			m_sceneSnapshot.Capture(world);
		}
		else
		{
			m_sceneSnapshot.Restore(world);
			if (!world.GetRegistry().valid(m_context->selectedEntity)) m_context->selectedEntity = ECS::NullEntity;
		}

		m_context->mode = mode;
		NENE_LOG_INFO("Editor switched to {} mode", mode == EditorMode::Play ? "Play" : "Edit");

		if (m_onModeChanged) m_onModeChanged(mode);
	}

	void EditorLayer::HandleShortcuts()
	{
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_P, ImGuiInputFlags_RouteGlobal))
			SetMode(m_context->mode == EditorMode::Play ? EditorMode::Edit : EditorMode::Play);

		if (m_context->mode != EditorMode::Edit) return;

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) m_history.Undo(m_context->world);
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) ||
		    ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
			m_history.Redo(m_context->world);
	}

	void EditorLayer::DrawDockSpace()
	{
		const ImGuiID dockSpaceId = ImGui::GetID(kDockSpaceName);

		if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) BuildDefaultLayout(dockSpaceId);

		ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
		ImGui::DockSpaceOverViewport(dockSpaceId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::PopStyleColor();
	}

	void EditorLayer::BuildDefaultLayout(ImGuiID dockSpaceId)
	{
		ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockSpaceId, ImGui::GetMainViewport()->WorkSize);

		ImGuiID centerId = dockSpaceId;
		const ImGuiID leftId = ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Left, kLeftDockRatio, nullptr, &centerId);
		const ImGuiID rightId =
		    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Right, kRightDockRatio, nullptr, &centerId);
		const ImGuiID bottomId =
		    ImGui::DockBuilderSplitNode(centerId, ImGuiDir_Down, kBottomDockRatio, nullptr, &centerId);

		for (const auto& entry : m_panels)
		{
			ImGuiID nodeId = centerId;
			switch (entry.panel->GetDefaultDockSlot())
			{
			case EditorDockSlot::Left:
				nodeId = leftId;
				break;
			case EditorDockSlot::Right:
				nodeId = rightId;
				break;
			case EditorDockSlot::Bottom:
				nodeId = bottomId;
				break;
			case EditorDockSlot::Center:
				break;
			}

			ImGui::DockBuilderDockWindow(entry.panel->GetName(), nodeId);
		}

		ImGui::DockBuilderFinish(dockSpaceId);
	}

	void EditorLayer::DrawPanels()
	{
		for (auto& entry : m_panels)
		{
			if (!entry.isOpen) continue;

			if (ImGui::Begin(entry.panel->GetName(), &entry.isOpen, entry.panel->GetWindowFlags()))
				entry.panel->Draw(*m_context);
			ImGui::End();
		}
	}

	void EditorLayer::ApplyInputCapture()
	{
		if (m_uiInput == nullptr) return;

		const ImGuiIO& io = ImGui::GetIO();
		m_uiInput->SetMouseCaptured(io.WantCaptureMouse && !m_context->isViewportHovered);
		m_uiInput->SetKeyboardCaptured(io.WantCaptureKeyboard && !m_context->isViewportFocused);
	}

} // namespace NeneEngine
