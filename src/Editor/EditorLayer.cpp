// EditorLayer.cpp

#include "Editor/EditorLayer.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "ECS/World.h"
#include "Editor/Panels/AssetBrowserPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/StatisticsPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeLoader.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IImGuiBackend.h"
#include "Input/InputDevice.h"
#include "Scene/SceneSerializer.h"
#include <Windows.h>
#include <commdlg.h>

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
		constexpr const char* kPlayLabel = NENE_ICON_PLAY " Play###PlayStop";
		constexpr const char* kStopLabel = NENE_ICON_STOP " Stop###PlayStop";
		constexpr float kStopButtonHoveredTint = 0.25f;
		constexpr const char* kMascotFiles[] = {"mascot_sleep_256.png", "mascot_search_256.png", "mascot_happy_256.png",
		                                        "mascot_scared_256.png"};
		static_assert(std::size(kMascotFiles) == static_cast<size_t>(EditorMascotPose::Count));

		constexpr const char* kStatusBarName = "##EditorStatusBar";
		constexpr ImGuiWindowFlags kStatusBarFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		                                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNavFocus;
		constexpr double kStatusHoldSeconds = 5.0;
		constexpr float kStatusBarBorderSize = 1.0f;

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

		std::string ToUtf8(const std::filesystem::path& path)
		{
			const std::u8string utf8Path = path.u8string();
			return std::string(utf8Path.begin(), utf8Path.end());
		}

		ImVec2 ComputeDockSpaceSize()
		{
			const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
			const float margin = ImGui::GetStyle().DockingSeparatorSize;
			return ImVec2((std::max)(1.0f, workSize.x - margin * 2.0f), (std::max)(1.0f, workSize.y - margin * 2.0f));
		}

		std::filesystem::path ChooseSceneFile(bool save, const std::filesystem::path& current)
		{
			wchar_t buffer[32768]{};
			const auto initial = current.empty() ? std::wstring(L"scene.json") : current.wstring();
			wcsncpy_s(buffer, initial.c_str(), _TRUNCATE);
			const auto directory = ResolveFromExecutionRoots("assets/scenes").wstring();
			OPENFILENAMEW dialog{};
			dialog.lStructSize = sizeof(dialog);
			dialog.hwndOwner = GetActiveWindow();
			dialog.lpstrFile = buffer;
			dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
			dialog.lpstrFilter = L"Scene JSON (*.json)\0*.json\0All files (*.*)\0*.*\0";
			dialog.lpstrDefExt = L"json";
			dialog.lpstrInitialDir = directory.c_str();
			dialog.Flags = OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
			return (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) ? std::filesystem::path(buffer)
			                                                                      : std::filesystem::path{};
		}
	} // namespace

	EditorLayer::EditorLayer() = default;

	void EditorLayer::SetResourceLoading(ResourceLoadQueue& loads, const SceneAssetResolver& assets)
	{
		m_resourceLoads = &loads;
		if (m_context)
		{
			m_context->resourceLoads = &loads;
			m_context->assetResolver = assets;
		}
	}

	void EditorLayer::SetRenderer(IRenderAdapter* renderer)
	{
		if (m_context) m_context->renderer = renderer;
	}

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
		m_context->assets.Refresh();

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
		m_panels.push_back({eastl::make_unique<AssetBrowserPanel>()});
		m_panels.push_back({eastl::make_unique<StatisticsPanel>()});
		m_panels.push_back({eastl::make_unique<ViewportPanel>()});

		for (auto& entry : m_panels)
			entry.windowTitle =
			    eastl::string(entry.panel->GetIcon()) + " " + entry.panel->GetName() + "###" + entry.panel->GetName();

		NENE_LOG_INFO("Editor initialized with {} panels", m_panels.size());
	}

	void EditorLayer::Shutdown()
	{
		++m_sceneLoadGeneration;
		m_resourceLoads = nullptr;
		if (m_uiInput != nullptr)
		{
			m_uiInput->SetMouseCaptured(false);
			m_uiInput->SetKeyboardCaptured(false);
		}

		ReleaseMascots();
		if (m_imguiBackend != nullptr) m_aboutWindow.Shutdown(*m_imguiBackend);
		m_panels.clear();
		m_scenePath.clear();
		m_layoutInitialized = false;
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
		if (m_exitRequested)
		{
			m_exitRequested = false;
			if (ConfirmDiscardChanges() && m_requestShutdown) m_requestShutdown();
		}

		if (m_imguiBackend == nullptr) return;
		if (m_context->mode == EditorMode::Edit && !m_context->pendingAssets.empty())
		{
			auto complete = std::move(m_context->pendingAssets.front());
			m_context->pendingAssets.erase(m_context->pendingAssets.begin());
			try
			{
				complete();
			}
			catch (const std::exception& exception)
			{
				m_context->SetStatus(EditorStatusKind::Danger, exception.what());
				NENE_LOG_ERROR("Deferred asset finalization failed: {}", exception.what());
			}
		}

		if (const float dpiScale = m_imguiBackend->GetDpiScale(); dpiScale != m_themeService.GetDpiScale())
			m_themeService.SetDpiScale(dpiScale);

		m_imguiBackend->BeginFrame();
		ImGuizmo::BeginFrame();

		m_context->isViewportHovered = false;
		m_context->isViewportFocused = false;

		HandleShortcuts();
		DrawMainMenuBar();
		DrawStatusBar();
		DrawDockSpace();
		DrawPanels();
		m_aboutWindow.Draw(*m_context);

		ApplyInputCapture();
	}

	void EditorLayer::DrawMainMenuBar()
	{
		const EditorThemeColorsConfig& colors = m_themeService.GetColors();
		if (!ImGui::BeginMainMenuBar()) return;

		const ImVec4& logoColor =
		    m_context->mode == EditorMode::Play ? colors.accentPlay : ImGui::GetStyleColorVec4(ImGuiCol_Button);
		ImGui::PushStyleColor(ImGuiCol_Text, logoColor);
		ImGui::TextUnformatted(NENE_ICON_LOGO);
		ImGui::PopStyleColor();
		ImGui::TextUnformatted(kProductName);

		if (ImGui::BeginMenu("File"))
		{
			const bool canEdit = m_context->CanEdit();
			if (ImGui::MenuItem(NENE_ICON_SAVE " Save Scene", "Ctrl+S", false, canEdit)) SaveScene(false);
			if (ImGui::MenuItem(NENE_ICON_SAVE " Save Scene As...", "Ctrl+Shift+S", false, canEdit)) SaveScene(true);
			if (ImGui::MenuItem(NENE_ICON_FOLDER_OPEN " Load Scene...", "Ctrl+O", false, canEdit)) LoadScene();
			ImGui::Separator();
			if (ImGui::MenuItem(NENE_ICON_EXIT " Exit") && m_requestShutdown && ConfirmDiscardChanges())
				m_requestShutdown();
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
			if (ImGui::MenuItem(NENE_ICON_LAYOUT " Reset Layout"))
			{
				const auto dockSpaceId = ImHashStr(kDockSpaceName);
				ImGui::DockBuilderRemoveNode(dockSpaceId);
				BuildDefaultLayout(dockSpaceId, ComputeDockSpaceSize());
			}
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
		if (ImGui::MenuItem(NENE_ICON_MOON " Evening", nullptr, current == EditorThemeVariant::Evening))
			SetTheme(EditorThemeVariant::Evening);
		if (ImGui::MenuItem(NENE_ICON_SUN " Day", nullptr, current == EditorThemeVariant::Day))
			SetTheme(EditorThemeVariant::Day);
		ImGui::EndMenu();
	}

	void EditorLayer::SetTheme(EditorThemeVariant variant)
	{
		if (m_themeService.GetVariant() == variant) return;

		m_themeService.Apply(GetDefaultEditorThemeConfig(variant));
		SaveEditorThemeVariant(GetDefaultEditorThemeConfigPath(), variant);
	}

	void EditorLayer::SaveScene(bool saveAs)
	{
		try
		{
			const auto path = saveAs || m_scenePath.empty() ? ChooseSceneFile(true, m_scenePath) : m_scenePath;
			if (path.empty()) return;
			SceneSerializer::SaveToFile(m_context->world, path);
			m_scenePath = path;
			m_history.MarkSaved();
			m_context->SetStatus(EditorStatusKind::Success, "Saved: " + ToUtf8(path.filename()));
			NENE_LOG_INFO("Saved: {}", ToUtf8(path));
		}
		catch (const std::exception& e)
		{
			m_context->SetStatus(EditorStatusKind::Danger, e.what());
			NENE_LOG_ERROR("Save scene: {}", e.what());
		}
	}

	bool EditorLayer::ConfirmDiscardChanges()
	{
		if (m_context == nullptr || !m_history.IsDirty()) return true;

		const std::wstring sceneName =
		    m_scenePath.empty() ? std::wstring(L"Untitled") : m_scenePath.filename().wstring();
		const std::wstring message = L"Save changes to " + sceneName + L"?";
		switch (MessageBoxW(GetActiveWindow(), message.c_str(), L"NeneEngine", MB_YESNOCANCEL | MB_ICONWARNING))
		{
		case IDYES:
			SetMode(EditorMode::Edit);
			SaveScene(false);
			return !m_history.IsDirty();
		case IDNO:
			return true;
		default:
			return false;
		}
	}

	void EditorLayer::LoadScene()
	{
		if (!m_resourceLoads || !ConfirmDiscardChanges()) return;
		const auto path = ChooseSceneFile(false, m_scenePath);
		if (path.empty()) return;
		const auto generation = ++m_sceneLoadGeneration;
		m_context->SetStatus(EditorStatusKind::Muted, "Loading scene...");
		m_context->isSceneLoading = true;
		const auto cameras = SceneSerializer::CaptureCameras(m_context->world);
		const auto fail = [this, generation](const std::string& error)
		{
			if (generation != m_sceneLoadGeneration || !m_context) return;
			m_context->isSceneLoading = false;
			m_context->SetStatus(EditorStatusKind::Danger, error);
			NENE_LOG_ERROR("Load scene: {}", error);
		};
		if (!m_resourceLoads->Request(
		        path.generic_string(),
		        [this, generation, path, cameras]() -> ResourceLoadQueue::Completion
		        {
			        auto scene = SceneSerializer::Prepare(SceneSerializer::ReadFromFile(path), cameras);
			        return [this, generation, path, scene = std::move(scene)]()
			        {
				        if (generation != m_sceneLoadGeneration || !m_context) return;
				        auto apply = [this, generation, path, scene]()
				        {
					        if (generation != m_sceneLoadGeneration || !m_context) return;
					        m_context->isSceneLoading = false;
					        SceneSerializer::Apply(scene, m_context->world);
					        ++m_context->sceneGeneration;
					        m_context->pendingAssets.clear();
					        m_history.Clear();
					        m_sceneSnapshot.Clear();
					        m_context->selectedEntity = ECS::NullEntity;
					        for (auto& entry : m_panels) entry.panel->OnSceneLoaded();
					        m_scenePath = path;
					        m_context->SetStatus(EditorStatusKind::Success, "Loaded: " + ToUtf8(path.filename()));
					        NENE_LOG_INFO("Loaded: {}", ToUtf8(path));
				        };
				        if (m_context->mode == EditorMode::Edit)
					        apply();
				        else
					        m_context->pendingAssets.push_back(std::move(apply));
			        };
		        },
		        fail, JobPriority::High))
			fail("Resource loader is stopping");
	}

	void EditorLayer::DrawStatusBar()
	{
		const EditorThemeColorsConfig& colors = m_themeService.GetColors();
		EditorStatus& status = m_context->status;
		if (status.kind == EditorStatusKind::Success && ImGui::GetTime() - status.time > kStatusHoldSeconds)
			status = {};

		std::string label = status.message.empty() ? "Ready" : status.message;
		if (status.kind == EditorStatusKind::Danger) label = std::string(NENE_ICON_ERROR) + " " + label;
		if (status.kind == EditorStatusKind::Success) label = std::string(NENE_ICON_SUCCESS) + " " + label;

		const bool isPlaying = m_context->mode == EditorMode::Play;
		const ImVec4& messageColor = isPlaying ? colors.background : GetStatusColor(colors, status.kind);
		const ImVec4& detailColor = isPlaying ? colors.background : colors.textMuted;

		const ImGuiStyle& style = ImGui::GetStyle();
		ImGui::PushStyleColor(ImGuiCol_WindowBg, isPlaying ? colors.accentPlay : colors.backgroundSunken);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(style.FramePadding.x, 0.0f));
		const bool isVisible = ImGui::BeginViewportSideBar(kStatusBarName, ImGui::GetMainViewport(), ImGuiDir_Down,
		                                                   ImGui::GetFrameHeight(), kStatusBarFlags);
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
		if (isVisible)
		{
			const ImVec2 windowMin = ImGui::GetWindowPos();
			const ImVec2 windowMax(windowMin.x + ImGui::GetWindowWidth(), windowMin.y + ImGui::GetWindowHeight());
			const float borderSize = (std::max)(1.0f, ImTrunc(ScaledSize(kStatusBarBorderSize)));
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->PushClipRect(windowMin, windowMax, false);
			drawList->AddRectFilled(windowMin, ImVec2(windowMax.x, windowMin.y + borderSize),
			                        ImGui::GetColorU32(colors.border));
			drawList->PopClipRect();

			const std::string sceneName = m_scenePath.empty() ? "Untitled" : ToUtf8(m_scenePath.filename());
			const std::string sceneLabel =
			    std::string(NENE_ICON_FILE) + " " + sceneName + (m_history.IsDirty() ? " *" : "");
			const char* modeLabel = isPlaying ? "Play" : "Edit";
			const float segmentSpacing = style.ItemSpacing.x * 2.0f;
			const float rightWidth =
			    ImGui::CalcTextSize(sceneLabel.c_str()).x + segmentSpacing + ImGui::CalcTextSize(modeLabel).x;
			const float startX = ImGui::GetCursorPosX();
			const float rightX = (std::max)(startX, startX + ImGui::GetContentRegionAvail().x - rightWidth);
			const float messageWidth = (std::max)(0.0f, rightX - startX - style.ItemSpacing.x);
			const ImVec2 start = ImGui::GetCursorScreenPos();

			ImGui::AlignTextToFramePadding();
			ImGui::PushClipRect(start, ImVec2(start.x + messageWidth, start.y + ImGui::GetFrameHeight()), true);
			ImGui::TextColored(messageColor, "%s", label.c_str());
			ImGui::PopClipRect();
			if (ImGui::CalcTextSize(label.c_str()).x > messageWidth)
				ImGui::SetItemTooltip("%s", status.message.c_str());

			ImGui::SameLine(rightX);
			ImGui::TextColored(detailColor, "%s", sceneLabel.c_str());
			if (!m_scenePath.empty()) ImGui::SetItemTooltip("%s", ToUtf8(m_scenePath).c_str());
			ImGui::SameLine(0.0f, segmentSpacing);
			ImGui::TextColored(detailColor, "%s", modeLabel);
		}
		ImGui::End();
	}

	void EditorLayer::DrawHistoryMenuItems()
	{
		const bool isEditMode = m_context->CanEdit();

		const char* undoName = m_history.GetUndoName();
		const eastl::string undoLabel =
		    undoName != nullptr ? eastl::string(NENE_ICON_UNDO " Undo ") + undoName : NENE_ICON_UNDO " Undo";
		if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, isEditMode && m_history.CanUndo()))
			m_history.Undo(m_context->world);

		const char* redoName = m_history.GetRedoName();
		const eastl::string redoLabel =
		    redoName != nullptr ? eastl::string(NENE_ICON_REDO " Redo ") + redoName : NENE_ICON_REDO " Redo";
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

		if (!m_context->CanEdit()) return;
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
			SaveScene(true);
		else if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_S, ImGuiInputFlags_RouteGlobal))
			SaveScene(false);
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_O, ImGuiInputFlags_RouteGlobal)) LoadScene();

		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal)) m_history.Undo(m_context->world);
		if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y, ImGuiInputFlags_RouteGlobal) ||
		    ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z, ImGuiInputFlags_RouteGlobal))
			m_history.Redo(m_context->world);
	}

	void EditorLayer::DrawDockSpace()
	{
		const ImGuiID dockSpaceId = ImHashStr(kDockSpaceName);
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		const float margin = ImGui::GetStyle().DockingSeparatorSize;

		if (!m_layoutInitialized &&
		    std::none_of(m_panels.begin(), m_panels.end(), [](const PanelEntry& entry)
		                 { return ImGui::FindWindowSettingsByID(ImHashStr(entry.windowTitle.c_str())) != nullptr; }))
			ImGui::DockBuilderRemoveNode(dockSpaceId);

		if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr)
			BuildDefaultLayout(dockSpaceId, ComputeDockSpaceSize());
		else if (!m_layoutInitialized)
		{
			for (const auto& entry : m_panels)
			{
				if (ImGui::FindWindowSettingsByID(ImHashStr(entry.windowTitle.c_str())) != nullptr) continue;
				const auto* centralNode = ImGui::DockBuilderGetCentralNode(dockSpaceId);
				ImGuiID nodeId = centralNode != nullptr ? centralNode->ID : dockSpaceId;
				for (const auto& other : m_panels)
				{
					if (other.panel->GetDefaultDockSlot() != entry.panel->GetDefaultDockSlot()) continue;
					if (const auto* settings = ImGui::FindWindowSettingsByID(ImHashStr(other.windowTitle.c_str()));
					    settings && settings->DockId)
					{
						nodeId = settings->DockId;
						break;
					}
				}
				ImGui::DockBuilderDockWindow(entry.windowTitle.c_str(), nodeId);
			}
			ImGui::DockBuilderFinish(dockSpaceId);
		}
		m_layoutInitialized = true;

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

		const ImVec2 workMax(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
		ImGui::GetBackgroundDrawList(viewport)->AddRectFilled(
		    viewport->WorkPos, workMax, ImGui::GetColorU32(m_themeService.GetColors().backgroundSunken));
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

		const auto firstBottomPanel =
		    std::find_if(m_panels.begin(), m_panels.end(), [](const PanelEntry& entry)
		                 { return entry.panel->GetDefaultDockSlot() == EditorDockSlot::Bottom; });
		if (ImGuiDockNode* bottomNode = ImGui::DockBuilderGetNode(bottomId);
		    bottomNode != nullptr && firstBottomPanel != m_panels.end())
			bottomNode->SelectedTabId = ImHashStr("#TAB", 0, ImHashStr(firstBottomPanel->windowTitle.c_str()));
	}

	void EditorLayer::DrawPanels()
	{
		for (auto& entry : m_panels) entry.panel->Update(*m_context);

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
			                  ImVec2(rect.Max.x - inset, rect.Max.y - inset), ImGui::GetColorU32(borderColor), radius,
			                  0, borderSize);
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
