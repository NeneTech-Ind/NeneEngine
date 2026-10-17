// HierarchyPanel.cpp

#include "Editor/Panels/HierarchyPanel.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/World.h"
#include "Editor/EditorContext.h"

#include <EASTL/algorithm.h>
#include <EASTL/sort.h>
#include <cstdint>
#include <imgui.h>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr int kMaxHierarchyDepth = 64;

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
			std::string name = tag != nullptr && !tag->name.empty() ? tag->name : "Entity";
			if (registry.all_of<ECS::CameraComponent>(entity)) name += " [camera]";
			return name;
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

		m_filter.Draw("##Filter", -FLT_MIN);
		if (ImGui::IsItemHovered() && !m_filter.IsActive()) ImGui::SetTooltip("Filter by name");

		size_t entityCount = 0;
		for ([[maybe_unused]] const ECS::Entity entity : registry.view<entt::entity>()) ++entityCount;
		ImGui::TextDisabled("%zu entities", entityCount);
		ImGui::Separator();

		if (!ImGui::BeginChild("##EntityTree"))
		{
			ImGui::EndChild();
			return;
		}

		if (m_filter.IsActive())
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

		ImGui::EndChild();
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

		const std::string name = GetDisplayName(registry, entity);
		const bool isOpen = ImGui::TreeNodeEx(ToTreeId(entity), flags, "%s", name.c_str());
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) context.selectedEntity = entity;

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
			ImGui::TextDisabled("No matches");
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
				const std::string name = GetDisplayName(registry, entity);
				if (ImGui::Selectable(name.c_str(), entity == context.selectedEntity)) context.selectedEntity = entity;
				ImGui::PopID();
			}
		}
	}

} // namespace NeneEngine
