// AssetBrowserPanel.cpp

#include "Editor/Panels/AssetBrowserPanel.h"

#include "Editor/AssetCatalog.h"
#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IRenderAdapter.h"

#include <algorithm>

namespace NeneEngine
{
	namespace
	{
		constexpr float kListWidthRatio = 0.7f;

		const char* GetAssetIcon(AssetKind kind)
		{
			switch (kind)
			{
			case AssetKind::Model:
				return NENE_ICON_MESH;
			case AssetKind::Texture:
				return NENE_ICON_IMAGE;
			case AssetKind::Shader:
				return NENE_ICON_FILE_CODE;
			}
			return NENE_ICON_FILE;
		}

		const char* GetAssetPayload(AssetKind kind)
		{
			switch (kind)
			{
			case AssetKind::Model:
				return ModelAssetPayload;
			case AssetKind::Texture:
				return TextureAssetPayload;
			case AssetKind::Shader:
				break;
			}
			return ShaderAssetPayload;
		}

		std::string GetFileName(const std::string& path)
		{
			return path.substr(path.find_last_of("/\\") + 1);
		}

		void DrawPath(const EditorContext& context, const std::string& path)
		{
			if (context.monospaceFont != nullptr) ImGui::PushFont(context.monospaceFont, 0.0f);
			ImGui::TextWrapped("%s", path.c_str());
			if (context.monospaceFont != nullptr) ImGui::PopFont();
		}
	} // namespace

	void AssetBrowserPanel::Draw(EditorContext& context)
	{
		const EditorThemeColorsConfig& colors = *context.themeColors;

		if (ImGui::Button(NENE_ICON_REFRESH "##Refresh"))
		{
			context.assets.Refresh();
			m_failedPreviewPath.clear();
		}
		ImGui::SetItemTooltip("Refresh assets");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##Filter", NENE_ICON_SEARCH " Filter by name", m_filter.InputBuf,
		                             IM_ARRAYSIZE(m_filter.InputBuf)))
			m_filter.Build();

		StatusText(colors, EditorStatusKind::Muted,
		           context.mode == EditorMode::Edit ? "Drag a model or texture into Viewport to create an entity"
		                                            : "Assets cannot be dragged during Play");
		if (!context.assets.Error().empty())
			StatusText(colors, EditorStatusKind::Danger, context.assets.Error().c_str());
		ImGui::Separator();

		if (ImGui::BeginChild("##AssetList", ImVec2(ImGui::GetContentRegionAvail().x * kListWidthRatio, 0.0f)))
			DrawList(context);
		ImGui::EndChild();
		ImGui::SameLine();
		if (ImGui::BeginChild("##AssetPreview")) DrawPreview(context);
		ImGui::EndChild();
	}

	void AssetBrowserPanel::DrawList(const EditorContext& context)
	{
		const auto& entries = context.assets.Entries();
		if (entries.empty())
		{
			DrawEmptyState(context.GetMascot(EditorMascotPose::Sleep), "No assets found");
			return;
		}

		m_matches.clear();
		for (size_t index = 0; index < entries.size(); ++index)
		{
			if (m_filter.PassFilter(GetFileName(entries[index].path).c_str())) m_matches.push_back(index);
		}

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
				const AssetEntry& entry = entries[m_matches[static_cast<size_t>(row)]];
				const std::string label = std::string(GetAssetIcon(entry.kind)) + " " + GetFileName(entry.path);
				const bool isSelected = m_selectedPath == entry.path;

				ImGui::PushID(entry.path.c_str());
				if (ImGui::Selectable(label.c_str(), isSelected)) m_selectedPath = entry.path;
				if (isSelected) DrawSelectionMarker(context.themeColors->accent);
				if (ImGui::BeginItemTooltip())
				{
					DrawPath(context, entry.path);
					ImGui::EndTooltip();
				}
				if (context.mode == EditorMode::Edit && ImGui::BeginDragDropSource())
				{
					ImGui::SetDragDropPayload(GetAssetPayload(entry.kind), entry.path.c_str(), entry.path.size() + 1);
					ImGui::TextUnformatted(label.c_str());
					ImGui::EndDragDropSource();
				}
				ImGui::PopID();
			}
		}
	}

	void AssetBrowserPanel::DrawPreview(EditorContext& context)
	{
		const auto& entries = context.assets.Entries();
		const auto selected = std::find_if(entries.begin(), entries.end(),
		                                   [this](const AssetEntry& entry) { return entry.path == m_selectedPath; });
		if (selected == entries.end())
		{
			DrawEmptyState(context.GetMascot(EditorMascotPose::Search), "Select an asset to preview it");
			return;
		}

		DrawPath(context, selected->path);
		if (selected->kind != AssetKind::Texture || context.renderer == nullptr) return;

		if (m_failedPreviewPath == selected->path)
		{
			DrawEmptyState(context.GetMascot(EditorMascotPose::Scared), "Could not load this texture");
			return;
		}

		const auto texture = context.assetResolver.Texture(selected->path, *context.renderer);
		if (!texture.IsValid())
		{
			m_failedPreviewPath = selected->path;
			context.SetStatus(EditorStatusKind::Danger, "Could not load texture: " + GetFileName(selected->path));
			return;
		}

		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float side = (std::min)(available.x, available.y);
		if (side <= 0.0f) return;

		const float scale = side / static_cast<float>((std::max)(texture.width, texture.height));
		ImGui::Image(static_cast<ImTextureID>(context.renderer->GetTextureHandle(texture.textureId)),
		             ImVec2(texture.width * scale, texture.height * scale));
	}
} // namespace NeneEngine
