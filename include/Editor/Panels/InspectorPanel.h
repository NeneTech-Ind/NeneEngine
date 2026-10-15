// InspectorPanel.h

#pragma once

#include "Editor/IEditorPanel.h"

namespace NeneEngine
{

	class InspectorPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Inspector"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Right; }

		void Draw(EditorContext& context) override;
	};

} // namespace NeneEngine
