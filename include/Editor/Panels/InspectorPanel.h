// InspectorPanel.h

#pragma once

#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Entity.h"
#include "Editor/Commands/IEditorCommand.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Scripting/ScriptFieldSchema.h"

#include <EASTL/functional.h>
#include <EASTL/unique_ptr.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <optional>
#include <string>

namespace NeneEngine
{
	class ScriptEngine;

	namespace ECS
	{
		struct TransformComponent;
	}

	class InspectorPanel final : public IEditorPanel
	{
	  public:
		explicit InspectorPanel(ScriptEngine& scriptEngine) : m_scriptEngine(scriptEngine) {}

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
			m_failedScriptPath.clear();
			m_scriptFieldEdit.reset();
		}

		struct FieldEdits
		{
			bool activated = false;
			bool active = false;
			bool deactivatedAfterEdit = false;
			bool changedInstantly = false;
			ImFont* valueFont = nullptr;
			const EditorThemeColorsConfig* colors = nullptr;
		};

	  private:
		using PendingEditCommit = eastl::function<void(EditorContext&)>;

		struct ScriptFieldEdit
		{
			ECS::Entity entity;
			std::string path;
			std::string field;
			ScriptFieldValue before;
			ScriptFieldValue value;
		};

		template <typename Component, typename DrawFields>
		void DrawComponent(EditorContext& context, const char* icon, const char* label, DrawFields&& drawFields);

		template <typename Component> void DrawRemoveMenu(EditorContext& context, const char* label);
		template <typename Component> bool DrawAddItem(EditorContext& context, const char* icon, const char* label);

		void DrawEntityHeader(EditorContext& context);
		void DrawComponents(EditorContext& context);
		void DrawAddComponent(EditorContext& context);
		void DrawTransformFields(ECS::Entity entity, ECS::TransformComponent& transform, FieldEdits& edits);
		void DrawMaterial(EditorContext& context);
		void DrawMaterialAsset(EditorContext& context, ECS::MeshRendererComponent& material, bool texture);
		void DrawMaterialPreview(EditorContext& context, const ECS::MeshRendererComponent& material);
		void SetMaterialAsset(EditorContext& context, ECS::MeshRendererComponent& material, const std::string& path,
		                      bool texture);

		bool DrawScript(EditorContext& context);
		void FinishScriptFieldEdit(EditorContext& context);

		PendingEditCommit m_commitPendingEdit;
		const char* m_pendingEditLabel = nullptr;

		ScriptEngine& m_scriptEngine;
		std::string m_failedScriptPath;
		std::optional<ScriptFieldEdit> m_scriptFieldEdit;
		eastl::unique_ptr<IEditorCommand> m_pendingCommand;

		ECS::Entity m_eulerEntity = ECS::NullEntity;
		glm::quat m_eulerSource = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 m_eulerDegrees = {0.0f, 0.0f, 0.0f};

		ECS::Entity m_colorEntity = ECS::NullEntity;
		ECS::MeshRendererComponent m_colorBefore;
		std::string m_materialError;
	};

} // namespace NeneEngine
