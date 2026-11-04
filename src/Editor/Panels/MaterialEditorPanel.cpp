#include "Editor/Panels/MaterialEditorPanel.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include <algorithm>
#include <glm/gtc/type_ptr.hpp>

namespace NeneEngine
{
	void MaterialEditorPanel::SetAsset(EditorContext& context, ECS::MeshRendererComponent& material,
	                                   const std::string& path, bool texture)
	{
		if (!path.empty())
		{
			const bool valid = texture ? context.assetResolver.Texture(path, *context.renderer).IsValid()
			                           : context.assetResolver.Shader(path, *context.renderer).IsValid();
			if (!valid)
			{
				m_error = texture ? "Could not load texture" : "Could not compile shader";
				return;
			}
		}
		const auto before = material;
		(texture ? material.texturePath : material.shaderPath) = path;
		if (texture && !path.empty() && material.shaderPath.empty())
			material.shaderPath = "assets/shaders/textured_mesh.shader";
		context.history.RecordComponentChange("Edit Material", context.selectedEntity, before, material);
		m_error.clear();
	}

	void MaterialEditorPanel::DrawAsset(EditorContext& context, ECS::MeshRendererComponent& material, bool texture)
	{
		const auto& path = texture ? material.texturePath : material.shaderPath;
		const auto kind = texture ? AssetKind::Texture : AssetKind::Shader;
		ImGui::TextUnformatted(texture ? "Texture" : "Shader");
		ImGui::SetNextItemWidth(-1);
		const std::string label =
		    path.empty() ? (texture ? "None" : "Default") : std::filesystem::path(path).filename().string();
		if (ImGui::BeginCombo(texture ? "##Texture" : "##Shader", label.c_str()))
		{
			if (ImGui::Selectable(texture ? "None" : "Default", path.empty())) SetAsset(context, material, "", texture);
			for (const auto& entry : context.assets.Entries())
				if (entry.kind == kind && ImGui::Selectable(entry.path.c_str(), entry.path == path))
					SetAsset(context, material, entry.path, texture);
			ImGui::EndCombo();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const auto* payload = ImGui::AcceptDragDropPayload(texture ? TextureAssetPayload : ShaderAssetPayload))
				SetAsset(context, material, static_cast<const char*>(payload->Data), texture);
			ImGui::EndDragDropTarget();
		}
		if (!path.empty()) ImGui::SetItemTooltip("%s", path.c_str());
	}

	void MaterialEditorPanel::Draw(EditorContext& context)
	{
		if (context.selectedEntity == ECS::NullEntity || !context.world.GetRegistry().valid(context.selectedEntity))
		{
			m_colorEntity = ECS::NullEntity;
			ImGui::TextWrapped("Select a mesh in Scene Hierarchy to edit its material");
			return;
		}
		auto* material = context.world.GetComponent<ECS::MeshRendererComponent>(context.selectedEntity);
		if (!material || !context.renderer)
		{
			ImGui::TextWrapped("This object has no material. For imported models, select a mesh part.");
			return;
		}
		ImGui::BeginDisabled(context.mode != EditorMode::Edit);
		const auto before = *material;
		ImGui::ColorEdit4("Tint", glm::value_ptr(material->tint));
		if (ImGui::IsItemActivated())
		{
			m_colorBefore = before;
			m_colorEntity = context.selectedEntity;
		}
		if (ImGui::IsItemDeactivatedAfterEdit() && m_colorEntity == context.selectedEntity)
		{
			context.history.RecordComponentChange("Edit Material Color", context.selectedEntity, m_colorBefore,
			                                      *material);
			m_colorEntity = ECS::NullEntity;
		}
		if (!material->meshPath.empty() ||
		    (material->primitiveType != PrimitiveType::Line && material->primitiveType != PrimitiveType::DebugLine))
		{
			DrawAsset(context, *material, true);
			DrawAsset(context, *material, false);
		}
		else
			ImGui::TextWrapped("Line material uses the built-in shader.");
		ImGui::EndDisabled();
		if (!m_error.empty()) ImGui::TextWrapped("%s", m_error.c_str());
		if (!material->texturePath.empty())
		{
			const auto texture = context.assetResolver.Texture(material->texturePath, *context.renderer);
			if (texture.IsValid())
			{
				ImGui::Text("Texture: %u x %u", texture.width, texture.height);
				const float width = (std::min)(ImGui::GetContentRegionAvail().x, 128.0f);
				ImGui::Image(static_cast<ImTextureID>(context.renderer->GetTextureHandle(texture.textureId)),
				             ImVec2(width, width * texture.height / texture.width));
			}
		}
	}

} // namespace NeneEngine
