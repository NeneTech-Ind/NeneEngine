// InspectorPanel.h

#pragma once

#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeConfig.h"

#include <EASTL/functional.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>

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
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_INSPECTOR; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Right; }

		void Draw(EditorContext& context) override;
		void OnSceneLoaded() override
		{
			m_commitPendingEdit = nullptr;
			m_pendingEditLabel = nullptr;
			m_eulerEntity = ECS::NullEntity;
			m_colorEntity = ECS::NullEntity;
			m_materialError.clear();
		}

		struct FieldEdits
		{
			bool activated = false;
			bool deactivatedAfterEdit = false;
			bool changedInstantly = false;
			ImFont* valueFont = nullptr;
			const EditorThemeColorsConfig* colors = nullptr;
		};

	  private:
		using PendingEditCommit = eastl::function<void(EditorContext&)>;

		template <typename Component, typename DrawFields>
		void DrawComponent(EditorContext& context, const char* icon, const char* label, DrawFields&& drawFields);

		void DrawEntityHeader(EditorContext& context);
		void DrawComponents(EditorContext& context);
		void DrawTransformFields(ECS::Entity entity, ECS::TransformComponent& transform, FieldEdits& edits);
		void DrawMaterial(EditorContext& context);
		void DrawMaterialAsset(EditorContext& context, ECS::MeshRendererComponent& material, bool texture);
		void DrawMaterialPreview(EditorContext& context, const ECS::MeshRendererComponent& material);
		void SetMaterialAsset(EditorContext& context, ECS::MeshRendererComponent& material, const std::string& path,
		                      bool texture);

		PendingEditCommit m_commitPendingEdit;
		const char* m_pendingEditLabel = nullptr;

		ECS::Entity m_eulerEntity = ECS::NullEntity;
		glm::quat m_eulerSource = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 m_eulerDegrees = {0.0f, 0.0f, 0.0f};

		ECS::Entity m_colorEntity = ECS::NullEntity;
		ECS::MeshRendererComponent m_colorBefore;
		std::string m_materialError;
	};

} // namespace NeneEngine
