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
		const char* GetName() const override { return "Asset Browser"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_FOLDER; }
		EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Bottom; }
		void Draw(EditorContext& context) override;

	  private:
		void DrawList(const EditorContext& context);
		void DrawPreview(EditorContext& context);

		ImGuiTextFilter m_filter;
		eastl::vector<size_t> m_matches;
		std::string m_selectedPath;
		std::string m_failedPreviewPath;
	};
} // namespace NeneEngine
