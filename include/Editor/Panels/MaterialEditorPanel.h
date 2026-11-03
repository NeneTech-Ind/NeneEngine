#pragma once

#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Entity.h"
#include "Editor/IEditorPanel.h"

namespace NeneEngine
{
	class MaterialEditorPanel final : public IEditorPanel
	{
	  public:
		const char* GetName() const override { return "Material Editor"; }
		EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Right; }
		void Draw(EditorContext& context) override;
		void OnSceneLoaded() override { m_colorEntity = ECS::NullEntity; }

	  private:
		void SetAsset(EditorContext& context, ECS::MeshRendererComponent& material, const std::string& path,
		              bool texture);
		void DrawAsset(EditorContext& context, ECS::MeshRendererComponent& material, bool texture);
		ECS::Entity m_colorEntity = ECS::NullEntity;
		ECS::MeshRendererComponent m_colorBefore;
		std::string m_error;
	};
} // namespace NeneEngine
