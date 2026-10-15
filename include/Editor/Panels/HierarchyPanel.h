// HierarchyPanel.h

#pragma once

#include "Editor/IEditorPanel.h"

namespace NeneEngine
{

	class HierarchyPanel final : public IEditorPanel
	{
	  public:
		[[nodiscard]] const char* GetName() const override { return "Scene Hierarchy"; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Left; }

		void Draw(EditorContext& context) override;
	};

} // namespace NeneEngine
