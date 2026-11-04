// EditorLayer.cpp

#include "Editor/EditorLayer.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "ECS/World.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/StatisticsPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeLoader.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IImGuiBackend.h"
#include "Input/InputDevice.h"

#include <EASTL/string.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <iterator>

namespace NeneEngine
{
	namespace
	{
		constexpr const char* kDockSpaceName = "EditorMainDockSpace";
		constexpr const char* kDockHostName = "##EditorDockHost";
		constexpr ImGuiWindowFlags kDockHostFlags =
		    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
		    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
		    ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings;
		constexpr float kLeftDockRatio = 0.2f;
		constexpr float kRightDockRatio = 0.25f;
		constexpr float kBottomDockRatio = 0.25f;
		constexpr const char* kProductName = "NeneEngine";
		constexpr const char* kPlayLabel = NENE_ICON_PLAY " Play###PlayStop";
		constexpr const char* kStopLabel = NENE_ICON_STOP " Stop###PlayStop";
		constexpr float kStopButtonHoveredTint = 0.25f;
		constexpr const char* kMascotFiles[] = {"mascot_sleep_256.png", "mascot_search_256.png",
		                                        "mascot_happy_256.png", "mascot_scared_256.png"};
		static_assert(std::size(kMascotFiles) == static_cast<size_t>(EditorMascotPose::Count));

		constexpr int kCornerMaskSegments = 8;

		void FillCornerMask(ImDrawList* drawList, const ImVec2& corner, float directionX, float directionY,
		                    float radius, ImU32 color)
		{
			const ImVec2 center(corner.x + directionX * radius, corner.y + directionY * radius);
			drawList->PathLineTo(corner);
			for (int segment = 0; segment <= kCornerMaskSegments; ++segment)
			{
				const float angle = IM_PI * 0.5f * static_cast<float>(segment) / kCornerMaskSegments;
				drawList->PathLineTo(ImVec2(center.x - directionX * radius * ImCos(angle),
				                            center.y - directionY * radius * ImSin(angle)));
			}
			drawList->PathFillConcave(color);
		}

		ImVec4 Mix(const ImVec4& from, const ImVec4& to, float amount)
		{
			return ImVec4(from.x + (to.x - from.x) * amount, from.y + (to.y - from.y) * amount,
			              from.z + (to.z - from.z) * amount, from.w + (to.w - from.w) * amount);
		}
	} // namespace

	EditorLayer::EditorLayer() = default;

	EditorLayer::~EditorLayer()
	{
		Shutdown();
	}

	void EditorLayer::Initialize(ECS::World& world, IImGuiBackend* imguiBackend, InputDevice* uiInput,
	                             ECS::Entity cameraEntity, RequestShutdownCallback requestShutdown,
	                             ModeChangedCallback onModeChanged)
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
		m_context->cameraEntity = cameraEntity;

		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		m_themeService.Apply(GetDefaultEditorThemeConfig(LoadEditorThemeVariant(GetDefaultEditorThemeConfigPath())));
		m_themeService.SetDpiScale(m_imguiBackend->GetDpiScale());
		m_themeService.LoadFonts();
		m_context->monospaceFont = m_themeService.GetMonospaceFont();
		m_context->themeColors = &m_themeService.GetColors();
		LoadMascots();
		m_aboutWindow.Initialize(*m_imguiBackend);

		m_panels.push_back({eastl::make_unique<HierarchyPanel>()});
		m_panels.push_back({eastl::make_unique<InspectorPanel>()});
		m_panels.push_back({eastl::make_unique<StatisticsPanel>()});
		m_panels.push_back({eastl::make_unique<ViewportPanel>()});

		for (auto& entry : m_panels)
			entry.windowTitle = eastl::string(entry.panel->GetIcon()) + " " + entry.panel->GetName() + "###" +
			                    entry.panel->GetName();

		NENE_LOG_INFO("Editor initialized with {} panels", m_panels.size());
	}

	void EditorLayer::Shutdown()
	{
		if (m_uiInput != nullptr)
		{
			m_uiInput->SetMouseCaptured(false);
			m_uiInput->SetKeyboardCaptured(false);
		}

		ReleaseMascots();
		if (m_imguiBackend != nullptr) m_aboutWindow.Shutdown(*m_imguiBackend);
		m_panels.clear();
		m_history.Clear();
		m_sceneSnapshot.Clear();
		m_context.reset();
		m_requestShutdown = nullptr;
		m_onModeChanged = nullptr;
		m_uiInput = nullptr;
		m_imguiBackend = nullptr;
	}

	void EditorLayer::LoadMascots()
	{
		for (size_t pose = 0; pose < std::size(kMascotFiles); ++pose)
		{
			const std::filesystem::path relativePath =
			    std::filesystem::path{"assets"} / "editor" / "mascot" / kMascotFiles[pose];
			const std::filesystem::path path = ResolveFromExecutionRoots(relativePath);
			if (path.empty())
			{
				NENE_LOG_WARN("Editor mascot not found: {}", kMascotFiles[pose]);
				continue;
			}

			m_context->mascots[pose] = m_imguiBackend->RegisterTexture(path);
		}
	}

	void EditorLayer::ReleaseMascots()
	{
		if (m_context == nullptr || m_imguiBackend == nullptr) return;

		for (ImGuiTexture& mascot : m_context->mascots)
		{
			if (mascot.IsValid()) m_imguiBackend->ReleaseTexture(mascot);
			mascot = {};
		}
	}

	void EditorLayer::Update()
	{
		if (m_imguiBackend == nullptr) return;

		if (const float dpiScale = m_imguiBackend->GetDpiScale(); dpiScale != m_themeService.GetDpiScale())
			m_themeService.SetDpiScale(dpiScale);

		m_imguiBackend->BeginFrame();
		ImGuizmo::BeginFrame();

		m_context->isViewportHovered = false;
		m_context->isViewportFocused = false;

		HandleShortcuts();
		DrawMainMenuBar();
		DrawDockSpace();
		DrawPanels();
		m_aboutWindow.Draw(*m_context);

		ApplyInputCapture();
	}

	void EditorLayer::DrawMainMenuBar()
	{
		const EditorThemeColorsConfig& colors = m_themeService.GetColors();
		if (!ImGui::BeginMainMenuBar()) return;

		ImGui::PushStyleColor(ImGuiCol_Text, colors.accent);
		ImGui::TextUnformatted(NENE_ICON_LOGO);
		ImGui::PopStyleColor();
		ImGui::TextUnformatted(kProductName);

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
			for (auto& entry : m_panels) ImGui::MenuItem(entry.windowTitle.c_str(), nullptr, &entry.isOpen);
			ImGui::Separator();
			DrawThemeMenu();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Help"))
		{
			if (ImGui::MenuItem(NENE_ICON_LOGO " About NeneEngine")) m_aboutWindow.Open();
			ImGui::EndMenu();
		}

		DrawPlayControls();
		ImGui::EndMainMenuBar();
	}

	void EditorLayer::DrawThemeMenu()
	{
		if (!ImGui::BeginMenu("Theme")) return;

		const EditorThemeVariant current = m_themeService.GetVariant();
		if (ImGui::MenuItem("Evening", nullptr, current == EditorThemeVariant::Evening))
			SetTheme(EditorThemeVariant::Evening);
		if (ImGui::MenuItem("Day", nullptr, current == EditorThemeVariant::Day)) SetTheme(EditorThemeVariant::Day);
		ImGui::EndMenu();
	}

	void EditorLayer::SetTheme(EditorThemeVariant variant)
	{
		if (m_themeService.GetVariant() == variant) return;

		m_themeService.Apply(GetDefaultEditorThemeConfig(variant));
		SaveEditorThemeVariant(GetDefaultEditorThemeConfigPath(), variant);
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
		const float buttonWidth = (std::max)(ImGui::CalcTextSize(kPlayLabel, nullptr, true).x,
		                                     ImGui::CalcTextSize(kStopLabel, nullptr, true).x) +
		                          style.FramePadding.x * 2.0f + ImGui::GetFrameHeight();
		ImGui::SetCursorPosX((ImGui::GetWindowWidth() - buttonWidth) * 0.5f);

		if (isPlaying)
		{
			const EditorThemeColorsConfig& colors = m_themeService.GetColors();
			ImGui::PushStyleColor(ImGuiCol_Button, colors.accentPlay);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, Mix(colors.accentPlay, colors.text, kStopButtonHoveredTint));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, colors.accentPlay);
			ImGui::PushStyleColor(ImGuiCol_Text, colors.background);
		}

		if (PillButton(isPlaying ? kStopLabel : kPlayLabel, ImVec2(buttonWidth, 0.0f)))
			SetMode(isPlaying ? EditorMode::Edit : EditorMode::Play);
		if (isPlaying) ImGui::PopStyleColor(4);

		ImGui::SetItemTooltip(isPlaying ? "Stop and restore the scene (Ctrl+P)" : "Start the game (Ctrl+P)");

		if (isPlaying)
		{
			ImGui::SameLine();
			ImGui::TextColored(m_themeService.GetColors().accentPlay, "PLAYING");
		}
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
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float margin = ImGui::GetStyle().DockingSeparatorSize;
		const ImVec2 dockSpaceSize((std::max)(1.0f, viewport->WorkSize.x - margin * 2.0f),
		                           (std::max)(1.0f, viewport->WorkSize.y - margin * 2.0f));

		if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) BuildDefaultLayout(dockSpaceId, dockSpaceSize);

		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(margin, margin));
		ImGui::Begin(kDockHostName, nullptr, kDockHostFlags);
		ImGui::PopStyleVar(3);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
		ImGui::DockSpace(dockSpaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
		ImGui::PopStyleColor(2);
		ImGui::End();

		const ImGuiDockNode* centralNode = ImGui::DockBuilderGetCentralNode(dockSpaceId);
		if (centralNode == nullptr) return;

		const ImVec2 workMax(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
		ImGui::RenderRectFilledWithHole(ImGui::GetBackgroundDrawList(viewport), ImRect(viewport->WorkPos, workMax),
		                                centralNode->Rect(),
		                                ImGui::GetColorU32(m_themeService.GetColors().backgroundSunken), 0.0f);
	}

	void EditorLayer::BuildDefaultLayout(ImGuiID dockSpaceId, const ImVec2& size)
	{
		ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockSpaceId, size);

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

			ImGui::DockBuilderDockWindow(entry.windowTitle.c_str(), nodeId);
		}

		ImGui::DockBuilderFinish(dockSpaceId);
	}

	void EditorLayer::DrawPanels()
	{
		for (auto& entry : m_panels)
		{
			if (!entry.isOpen) continue;

			if (ImGui::Begin(entry.windowTitle.c_str(), &entry.isOpen, entry.panel->GetWindowFlags()))
			{
				entry.panel->Draw(*m_context);
				DrawDockedPanelCorners();
			}
			ImGui::End();
		}
	}

	void EditorLayer::DrawDockedPanelCorners() const
	{
		const ImGuiWindow* window = ImGui::GetCurrentWindow();
		const float radius = ImGui::GetStyle().WindowRounding;
		if (!window->DockIsActive || window->DockNode == nullptr || radius <= 0.0f) return;

		const EditorThemeColorsConfig& colors = m_themeService.GetColors();
		const ImRect rect = window->DockNode->Rect();
		const ImU32 color = ImGui::GetColorU32(colors.backgroundSunken);
		ImDrawList* drawList = window->DrawList;
		drawList->PushClipRect(rect.Min, rect.Max, false);
		FillCornerMask(drawList, rect.Min, 1.0f, 1.0f, radius, color);
		FillCornerMask(drawList, ImVec2(rect.Max.x, rect.Min.y), -1.0f, 1.0f, radius, color);
		FillCornerMask(drawList, ImVec2(rect.Min.x, rect.Max.y), 1.0f, -1.0f, radius, color);
		FillCornerMask(drawList, rect.Max, -1.0f, -1.0f, radius, color);

		const float baseBorderSize = m_themeService.GetMetrics().panelBorderSize;
		const float borderSize =
		    baseBorderSize > 0.0f ? (std::max)(1.0f, ImTrunc(baseBorderSize * m_themeService.GetDpiScale())) : 0.0f;
		if (borderSize > 0.0f)
		{
			const bool isFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
			const ImVec4& borderColor = isFocused ? colors.accentPlay : colors.border;
			const float inset = borderSize * 0.5f;
			drawList->AddRect(ImVec2(rect.Min.x + inset, rect.Min.y + inset),
			                  ImVec2(rect.Max.x - inset, rect.Max.y - inset),
			                  ImGui::GetColorU32(borderColor), radius, 0, borderSize);
		}
		drawList->PopClipRect();
	}

	void EditorLayer::ApplyInputCapture()
	{
		if (m_uiInput == nullptr) return;

		ImGuiIO& io = ImGui::GetIO();
		m_uiInput->SetMouseCaptured(io.WantCaptureMouse && !m_context->isViewportHovered);
		m_uiInput->SetKeyboardCaptured(io.WantCaptureKeyboard && !m_context->isViewportFocused);

		if (m_context->isViewportFocused)
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
		else
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	}

} // namespace NeneEngine
