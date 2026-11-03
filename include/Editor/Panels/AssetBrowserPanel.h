#pragma once
#include "Editor/IEditorPanel.h"
#include <string>

namespace NeneEngine
{
	class AssetBrowserPanel final : public IEditorPanel
	{
	  public:
		const char* GetName() const override { return "Asset Browser"; }
		EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Bottom; }
		void Draw(EditorContext& context) override;
	  private:
		ImGuiTextFilter m_filter;
		int m_kind = 0;
		std::string m_selectedPath;
	};
}
