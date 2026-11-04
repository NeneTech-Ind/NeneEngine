#pragma once
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"
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
		std::string m_selectedPath;
	};
} // namespace NeneEngine
