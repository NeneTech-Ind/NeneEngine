// EditorLayer.cpp

#include "Editor/EditorLayer.h"

#include "Core/NeneLogger.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Panels/StatisticsPanel.h"
#include "Editor/Panels/ViewportPanel.h"
#include "Graphics/Backend/IImGuiBackend.h"
#include "Input/InputDevice.h"

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
	} // namespace

	EditorLayer::EditorLayer() = default;

	EditorLayer::~EditorLayer()
	{
		Shutdown();
	}

	void EditorLayer::Initialize(ECS::World& world, IImGuiBackend* imguiBackend, InputDevice* uiInput,
	                             RequestShutdownCallback requestShutdown)
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
		m_context.reset();
		m_requestShutdown = nullptr;
		m_uiInput = nullptr;
		m_imguiBackend = nullptr;
	}

	void EditorLayer::Update()
	{
		if (m_imguiBackend == nullptr) return;

		m_imguiBackend->BeginFrame();

		m_context->isViewportHovered = false;
		m_context->isViewportFocused = false;

		DrawMainMenuBar();
		DrawDockSpace();
		DrawPanels();

		ApplyInputCapture();
	}

	void EditorLayer::DrawMainMenuBar()
	{
		if (!ImGui::BeginMainMenuBar()) return;

		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Exit") && m_requestShutdown) m_requestShutdown();
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("View"))
		{
			for (auto& entry : m_panels) ImGui::MenuItem(entry.panel->GetName(), nullptr, &entry.isOpen);
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
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
