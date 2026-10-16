// InspectorPanel.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"

#include <EASTL/functional.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace NeneEngine
{
	namespace ECS
	{
		struct TransformComponent;
	}

	class InspectorPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Inspector"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Right; }

		void Draw(EditorContext& context) override;

		struct FieldEdits
		{
			bool activated = false;
			bool deactivatedAfterEdit = false;
			bool changedInstantly = false;
		};

	  private:
		using PendingEditCommit = eastl::function<void(EditorContext&)>;

		template <typename Component, typename DrawFields>
		void DrawComponent(EditorContext& context, const char* label, DrawFields&& drawFields);

		void DrawEntityHeader(EditorContext& context);
		void DrawComponents(EditorContext& context);
		void DrawTransformFields(ECS::Entity entity, ECS::TransformComponent& transform, FieldEdits& edits);

		PendingEditCommit m_commitPendingEdit;
		const char* m_pendingEditLabel = nullptr;

		ECS::Entity m_eulerEntity = ECS::NullEntity;
		glm::quat m_eulerSource = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 m_eulerDegrees = {0.0f, 0.0f, 0.0f};
	};

} // namespace NeneEngine
