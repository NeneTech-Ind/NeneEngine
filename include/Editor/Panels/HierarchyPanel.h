// HierarchyPanel.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"

#include <EASTL/optional.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class HierarchyPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Scene Hierarchy"; }
		[[nodiscard]] const char* GetTitle() const override { return "Scene"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_HIERARCHY; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Left; }

		void Draw(EditorContext& context) override;

	  private:
		struct MotionSample
		{
			glm::vec3 position{};
			glm::quat rotation{};
			glm::vec3 scale{};
			double lastChangeTime = 0.0;
		};

		struct PrefabDraft
		{
			ECS::Entity entity;
			uint64_t sceneGeneration;
			std::string name;
			std::string error;
		};

		void UpdateMotionTracking(const EditorContext& context);
		void DrawMotionDot(const EditorContext& context, ECS::Entity entity) const;
		void CollectRoots(const ECS::World& world);
		void UpdateRevealPath(const EditorContext& context);
		void DrawEntityTree(EditorContext& context, ECS::Entity entity, int depth);
		void DrawFilteredList(EditorContext& context);
		void DrawEntityContextMenu(EditorContext& context, ECS::Entity entity);
		void DrawPrefabDialog(EditorContext& context);

		ImGuiTextFilter m_filter;
		eastl::optional<PrefabDraft> m_prefabDraft;
		eastl::vector<ECS::Entity> m_roots;
		eastl::vector<ECS::Entity> m_matches;

		ECS::Entity m_revealedEntity = ECS::NullEntity;
		eastl::vector<ECS::Entity> m_revealPath;
		eastl::unordered_map<uint32_t, MotionSample> m_motionSamples;
	};

} // namespace NeneEngine
