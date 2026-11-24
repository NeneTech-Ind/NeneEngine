// AssetBrowserPanel.h

#pragma once

#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"

#include <EASTL/vector.h>
#include <cstddef>
#include <string>

namespace NeneEngine
{
	class AssetBrowserPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Asset Browser"; }
		[[nodiscard]] const char* GetTitle() const override { return "Assets"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_FOLDER; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Left; }
		void Draw(EditorContext& context) override;
		void RevealAsset(const std::string& path);

	  private:
		void DrawList(const EditorContext& context);
		void DrawPreview(EditorContext& context);

		ImGuiTextFilter m_filter;
		eastl::vector<size_t> m_matches;
		std::string m_selectedPath;
		std::string m_failedPreviewPath;
		bool m_scrollToSelection = false;
	};
} // namespace NeneEngine
