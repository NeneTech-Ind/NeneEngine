// EditorWidgets.h

#pragma once

#include "Graphics/Backend/IImGuiBackend.h"

#include <imgui.h>

namespace NeneEngine
{
	struct AxisDragResult
	{
		bool changed = false;
		bool activated = false;
		bool deactivatedAfterEdit = false;
	};

	[[nodiscard]] float ScaledSize(float size);
	bool PillButton(const char* label, const ImVec2& size = ImVec2(0.0f, 0.0f));
	bool ComponentHeader(const char* icon, const char* label);
	void DrawSelectionMarker(const ImVec4& color);
	[[nodiscard]] ImVec2 CalcStatusPillSize(const char* label);
	void StatusPill(const char* label, const ImVec4& background, const ImVec4& textColor);
	void DrawEmptyState(const ImGuiTexture& mascot, const char* hint);
	AxisDragResult DragFloat3Axes(const char* id, float* values, float speed, float min, float max,
	                              ImGuiSliderFlags flags, const ImVec4 (&axisColors)[3]);

	class ComponentCardScope final
	{
	  public:
		ComponentCardScope();
		~ComponentCardScope();

		ComponentCardScope(const ComponentCardScope&) = delete;
		ComponentCardScope& operator=(const ComponentCardScope&) = delete;

	  private:
		ImDrawList* m_drawList = nullptr;
		ImVec2 m_start{};
		float m_width = 0.0f;
	};

} // namespace NeneEngine
