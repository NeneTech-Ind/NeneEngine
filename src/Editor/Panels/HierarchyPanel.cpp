// HierarchyPanel.cpp

#include "Editor/Panels/HierarchyPanel.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Scene/Prefab/PrefabLibrary.h"

#include <EASTL/algorithm.h>
#include <EASTL/sort.h>
#include <cstdint>
#include <glm/common.hpp>
#include <glm/vector_relational.hpp>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr int kMaxHierarchyDepth = 64;
		constexpr float kMotionEpsilon = 1.0e-4f;
		constexpr double kMotionHoldSeconds = 0.3;
		constexpr float kMotionDotRadius = 3.5f;
		constexpr const char* kCreatePrefabDialog = "Create Prefab from Entity";

		bool HasChanged(const glm::vec3& current, const glm::vec3& previous)
		{
			return glm::any(glm::greaterThan(glm::abs(current - previous), glm::vec3(kMotionEpsilon)));
		}

		ECS::Entity GetValidParent(const entt::registry& registry, ECS::Entity entity)
		{
			const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity);
			if (hierarchy == nullptr || hierarchy->parent == ECS::NullEntity || !registry.valid(hierarchy->parent))
				return ECS::NullEntity;

			return hierarchy->parent;
		}

		std::string GetDisplayName(const entt::registry& registry, ECS::Entity entity)
		{
			const auto* tag = registry.try_get<ECS::TagComponent>(entity);
			return tag != nullptr && !tag->name.empty() ? tag->name : "Entity";
		}

		const char* GetEntityIcon(const entt::registry& registry, ECS::Entity entity)
		{
			if (registry.all_of<ECS::CameraComponent>(entity)) return NENE_ICON_CAMERA;
			if (const auto* mesh = registry.try_get<ECS::MeshRendererComponent>(entity))
				return !mesh->meshPath.empty() && mesh->meshPath.front() != '@' ? NENE_ICON_MESH : NENE_ICON_PRIMITIVE;
			if (const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity);
			    hierarchy != nullptr && !hierarchy->children.empty())
				return NENE_ICON_HIERARCHY;
			return NENE_ICON_ENTITY;
		}

		std::string GetRowLabel(const entt::registry& registry, ECS::Entity entity)
		{
			return std::string(GetEntityIcon(registry, entity)) + " " + GetDisplayName(registry, entity);
		}

		void* ToTreeId(ECS::Entity entity)
		{
			return reinterpret_cast<void*>(static_cast<uintptr_t>(entt::to_integral(entity)));
		}
	} // namespace

	void HierarchyPanel::Draw(EditorContext& context)
	{
		const ECS::World& world = context.world;
		const entt::registry& registry = world.GetRegistry();
		if (context.selectedEntity != ECS::NullEntity && !registry.valid(context.selectedEntity))
			context.selectedEntity = ECS::NullEntity;

		UpdateMotionTracking(context);

		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##Filter", NENE_ICON_SEARCH " Filter by name", m_filter.InputBuf,
		                             IM_ARRAYSIZE(m_filter.InputBuf)))
			m_filter.Build();

		size_t entityCount = 0;
		for ([[maybe_unused]] const ECS::Entity entity : registry.view<entt::entity>()) ++entityCount;
		ImGui::TextDisabled("%zu entities", entityCount);
		ImGui::Separator();

		if (ImGui::BeginChild("##EntityTree"))
		{
			if (entityCount == 0)
			{
				DrawEmptyState(context.GetMascot(EditorMascotPose::Happy), "The scene is empty");
			}
			else if (m_filter.IsActive())
			{
				DrawFilteredList(context);
			}
			else
			{
				CollectRoots(world);
				UpdateRevealPath(context);
				for (const ECS::Entity root : m_roots) DrawEntityTree(context, root, 0);
			}

			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
				context.selectedEntity = ECS::NullEntity;
		}
		ImGui::EndChild();
		DrawPrefabDialog(context);
	}

	void HierarchyPanel::UpdateMotionTracking(const EditorContext& context)
	{
		if (context.mode != EditorMode::Play)
		{
			m_motionSamples.clear();
			return;
		}

		const double now = ImGui::GetTime();
		eastl::unordered_map<uint32_t, MotionSample> samples;
		for (const auto [entity, transform] : context.world.GetRegistry().view<ECS::TransformComponent>().each())
		{
			MotionSample sample{transform.position, transform.rotation, transform.scale, -kMotionHoldSeconds};
			const uint32_t key = static_cast<uint32_t>(entt::to_integral(entity));
			if (const auto previous = m_motionSamples.find(key); previous != m_motionSamples.end())
			{
				const MotionSample& last = previous->second;
				const bool hasMoved = HasChanged(transform.position, last.position) ||
				                      HasChanged(transform.scale, last.scale) ||
				                      glm::abs(glm::dot(transform.rotation, last.rotation)) < 1.0f - kMotionEpsilon;
				sample.lastChangeTime = hasMoved ? now : last.lastChangeTime;
			}
			samples.emplace(key, sample);
		}
		m_motionSamples = std::move(samples);
	}

	void HierarchyPanel::DrawMotionDot(const EditorContext& context, ECS::Entity entity) const
	{
		const auto sample = m_motionSamples.find(static_cast<uint32_t>(entt::to_integral(entity)));
		if (sample == m_motionSamples.end() || ImGui::GetTime() - sample->second.lastChangeTime > kMotionHoldSeconds)
			return;

		const float radius = ScaledSize(kMotionDotRadius);
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();
		const ImVec2 center(itemMax.x - ImGui::GetStyle().FramePadding.x - radius, (itemMin.y + itemMax.y) * 0.5f);
		ImGui::GetWindowDrawList()->AddCircleFilled(center, radius,
		                                            ImGui::GetColorU32(context.themeColors->accentPlay));
	}

	void HierarchyPanel::CollectRoots(const ECS::World& world)
	{
		const entt::registry& registry = world.GetRegistry();

		m_roots.clear();
		for (const ECS::Entity entity : registry.view<entt::entity>())
		{
			if (GetValidParent(registry, entity) == ECS::NullEntity) m_roots.push_back(entity);
		}
		eastl::sort(m_roots.begin(), m_roots.end());
	}

	void HierarchyPanel::UpdateRevealPath(const EditorContext& context)
	{
		if (context.selectedEntity == m_revealedEntity) return;

		m_revealedEntity = context.selectedEntity;
		m_revealPath.clear();
		if (m_revealedEntity == ECS::NullEntity) return;

		const entt::registry& registry = context.world.GetRegistry();
		ECS::Entity ancestor = GetValidParent(registry, m_revealedEntity);
		for (int depth = 0; ancestor != ECS::NullEntity && depth < kMaxHierarchyDepth; ++depth)
		{
			m_revealPath.push_back(ancestor);
			ancestor = GetValidParent(registry, ancestor);
		}
	}

	void HierarchyPanel::DrawEntityTree(EditorContext& context, ECS::Entity entity, int depth)
	{
		const entt::registry& registry = context.world.GetRegistry();
		const auto* hierarchy = registry.try_get<ECS::HierarchyComponent>(entity);

		bool hasChildren = false;
		if (hierarchy != nullptr && depth < kMaxHierarchyDepth)
		{
			hasChildren = eastl::any_of(hierarchy->children.begin(), hierarchy->children.end(),
			                            [&registry](ECS::Entity child) { return registry.valid(child); });
		}

		ImGuiTreeNodeFlags flags =
		    ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (entity == context.selectedEntity) flags |= ImGuiTreeNodeFlags_Selected;

		const auto revealIt = eastl::find(m_revealPath.begin(), m_revealPath.end(), entity);
		if (revealIt != m_revealPath.end())
		{
			ImGui::SetNextItemOpen(true);
			m_revealPath.erase(revealIt);
		}

		const std::string label = GetRowLabel(registry, entity);
		const bool isOpen = ImGui::TreeNodeEx(ToTreeId(entity), flags, "%s", label.c_str());
		if (entity == context.selectedEntity) DrawSelectionMarker(context.themeColors->accent);
		DrawMotionDot(context, entity);
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) context.selectedEntity = entity;
		DrawEntityContextMenu(context, entity);

		if (!hasChildren || !isOpen) return;

		for (const ECS::Entity child : hierarchy->children)
		{
			if (registry.valid(child)) DrawEntityTree(context, child, depth + 1);
		}
		ImGui::TreePop();
	}

	void HierarchyPanel::DrawFilteredList(EditorContext& context)
	{
		const entt::registry& registry = context.world.GetRegistry();

		m_matches.clear();
		for (const ECS::Entity entity : registry.view<entt::entity>())
		{
			if (m_filter.PassFilter(GetDisplayName(registry, entity).c_str())) m_matches.push_back(entity);
		}
		eastl::sort(m_matches.begin(), m_matches.end());

		if (m_matches.empty())
		{
			DrawEmptyState(context.GetMascot(EditorMascotPose::Scared), "No matches");
			return;
		}

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(m_matches.size()));
		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				const ECS::Entity entity = m_matches[static_cast<size_t>(row)];
				ImGui::PushID(ToTreeId(entity));
				const std::string label = GetRowLabel(registry, entity);
				const bool isSelected = entity == context.selectedEntity;
				if (ImGui::Selectable(label.c_str(), isSelected)) context.selectedEntity = entity;
				if (isSelected) DrawSelectionMarker(context.themeColors->accent);
				DrawMotionDot(context, entity);
				DrawEntityContextMenu(context, entity);
				ImGui::PopID();
			}
		}
	}

	void HierarchyPanel::DrawEntityContextMenu(EditorContext& context, ECS::Entity entity)
	{
		if (!ImGui::BeginPopupContextItem()) return;
		if (ImGui::MenuItem("Create Prefab from Entity...", nullptr, false, context.CanEdit() && context.prefabs != nullptr))
			m_prefabDraft = PrefabDraft{entity, context.sceneGeneration, {}, {}};
		ImGui::EndPopup();
	}

	void HierarchyPanel::DrawPrefabDialog(EditorContext& context)
	{
		if (!m_prefabDraft) return;
		auto& draft = *m_prefabDraft;
		const auto& registry = context.world.GetRegistry();
		const bool canSave = context.CanEdit() && context.prefabs != nullptr &&
		                     draft.sceneGeneration == context.sceneGeneration && registry.valid(draft.entity);
		if (canSave && !ImGui::IsPopupOpen(kCreatePrefabDialog)) ImGui::OpenPopup(kCreatePrefabDialog);
		const auto* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		const auto flags =
		    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove;
		if (!ImGui::BeginPopupModal(kCreatePrefabDialog, nullptr, flags))
		{
			if (!canSave) m_prefabDraft.reset();
			return;
		}
		if (!canSave)
		{
			ImGui::CloseCurrentPopup();
			ImGui::EndPopup();
			m_prefabDraft.reset();
			return;
		}

		const float width = ScaledSize(320.0f);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
		ImGui::Text("Entity: %s", GetDisplayName(registry, draft.entity).c_str());
		if (!draft.error.empty()) StatusText(*context.themeColors, EditorStatusKind::Danger, draft.error.c_str());
		ImGui::PopTextWrapPos();

		ImGui::SetNextItemWidth(width);
		if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
		const bool submitted =
		    ImGui::InputTextWithHint("##PrefabName", "Prefab name", &draft.name, ImGuiInputTextFlags_EnterReturnsTrue);
		if (ImGui::IsItemEdited()) draft.error.clear();
		const auto first = draft.name.find_first_not_of(" \t");
		ImGui::BeginDisabled(first == std::string::npos);
		const bool save = ImGui::Button("Create") || submitted;
		ImGui::EndDisabled();
		ImGui::SameLine();
		const bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape, false);

		bool saved = false;
		if (save && !cancel && first != std::string::npos)
		{
			try
			{
				const auto name = draft.name.substr(first, draft.name.find_last_not_of(" \t") - first + 1);
				context.prefabs->CreateFromEntity(name, draft.entity);
				context.assets.Refresh();
				context.SetStatus(EditorStatusKind::Success, "Created prefab: " + name);
				saved = true;
			}
			catch (const std::exception& exception)
			{
				draft.error = exception.what();
			}
		}
		if (saved || cancel) ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		if (saved || cancel) m_prefabDraft.reset();
	}

} // namespace NeneEngine
