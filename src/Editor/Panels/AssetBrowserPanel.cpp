#include "Editor/Panels/AssetBrowserPanel.h"
#include "Editor/AssetCatalog.h"
#include "Editor/EditorContext.h"
#include "Graphics/Backend/IRenderAdapter.h"
#include <algorithm>

namespace NeneEngine
{
	void AssetBrowserPanel::Draw(EditorContext& context)
	{
		if (ImGui::Button("Refresh")) context.assets.Refresh();
		ImGui::TextDisabled("Drag a model or texture into Viewport to create an object");
		if (!context.assets.Error().empty()) ImGui::TextWrapped("%s", context.assets.Error().c_str());
		if (!context.assetError.empty()) ImGui::TextWrapped("%s", context.assetError.c_str());
		if (ImGui::BeginChild("AssetList", ImVec2(ImGui::GetContentRegionAvail().x * 0.7f, 0), ImGuiChildFlags_Borders))
		{
			for (const auto& entry : context.assets.Entries())
			{
				const char* label = entry.kind == AssetKind::Model     ? "[Model] "
				                    : entry.kind == AssetKind::Texture ? "[Texture] "
				                                                       : "[Shader] ";
				if (ImGui::Selectable((std::string(label) + entry.path).c_str(), m_selectedPath == entry.path))
					m_selectedPath = entry.path;
				if (context.mode == EditorMode::Edit && ImGui::BeginDragDropSource())
				{
					const char* payload = entry.kind == AssetKind::Model     ? ModelAssetPayload
					                      : entry.kind == AssetKind::Texture ? TextureAssetPayload
					                                                         : ShaderAssetPayload;
					ImGui::SetDragDropPayload(payload, entry.path.c_str(), entry.path.size() + 1);
					ImGui::TextUnformatted(entry.path.c_str());
					ImGui::EndDragDropSource();
				}
			}
		}
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginGroup();
		ImGui::TextWrapped("%s", m_selectedPath.c_str());
		const auto selected = std::find_if(context.assets.Entries().begin(), context.assets.Entries().end(),
		                                   [this](const auto& entry) { return entry.path == m_selectedPath; });
		if (selected != context.assets.Entries().end() && selected->kind == AssetKind::Texture && context.renderer)
		{
			const auto texture = context.assetResolver.Texture(m_selectedPath, *context.renderer);
			if (texture.IsValid())
			{
				const float side = (std::min)(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y);
				const float scale = side / static_cast<float>((std::max)(texture.width, texture.height));
				if (side > 0)
					ImGui::Image(static_cast<ImTextureID>(context.renderer->GetTextureHandle(texture.textureId)),
					             ImVec2(texture.width * scale, texture.height * scale));
			}
		}
		ImGui::EndGroup();
	}
} // namespace NeneEngine
