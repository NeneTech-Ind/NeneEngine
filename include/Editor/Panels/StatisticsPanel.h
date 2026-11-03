// StatisticsPanel.h

#pragma once

#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"

namespace NeneEngine
{

	class StatisticsPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Statistics"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_STATISTICS; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Bottom; }

		void Draw(EditorContext& context) override;
	};

} // namespace NeneEngine
