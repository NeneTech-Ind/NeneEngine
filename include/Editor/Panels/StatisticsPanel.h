// StatisticsPanel.h

#pragma once

#include "Editor/IEditorPanel.h"

namespace NeneEngine
{

	class StatisticsPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Statistics"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Bottom; }

		void Draw(EditorContext& context) override;
	};

} // namespace NeneEngine
