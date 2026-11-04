// EditorWidgets.cpp

#include "Editor/Theme/EditorWidgets.h"

#include <algorithm>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr float kCardPadding = 6.0f;
		constexpr float kSelectionMarkerWidth = 3.0f;
		constexpr const char* kAxisNames[] = {"X", "Y", "Z"};
		constexpr float kEmptyStateMascotSize = 128.0f;
		constexpr float kEmptyStateMascotMinSize = 32.0f;
		constexpr float kEmptyStateMascotWidthRatio = 0.6f;
	} // namespace

	float ScaledSize(float size)
	{
		return size * ImGui::GetStyle().FontScaleDpi;
	}

	bool PillButton(const char* label, const ImVec2& size)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ImGui::GetFrameHeight() * 0.5f);
		const bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleVar();
		return pressed;
	}

	bool ComponentHeader(const char* icon, const char* label)
	{
		const std::string headerLabel = std::string(icon) + " " + label + "###" + label;
		return ImGui::CollapsingHeader(headerLabel.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
	}

	void DrawSelectionMarker(const ImVec4& color)
	{
		const float left = ImGui::GetWindowPos().x;
		const ImVec2 itemMin = ImGui::GetItemRectMin();
		const ImVec2 itemMax = ImGui::GetItemRectMax();
		ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(left, itemMin.y),
		                                          ImVec2(left + ScaledSize(kSelectionMarkerWidth), itemMax.y),
		                                          ImGui::GetColorU32(color));
	}

	void DrawEmptyState(const ImGuiTexture& mascot, const char* hint)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const ImVec2 origin = ImGui::GetCursorPos();
		const ImVec2 available = ImGui::GetContentRegionAvail();
		const ImVec2 textSize = ImGui::CalcTextSize(hint);

		ImVec2 imageSize(0.0f, 0.0f);
		if (mascot.IsValid() && mascot.width > 0 && mascot.height > 0)
		{
			const float aspect = static_cast<float>(mascot.height) / static_cast<float>(mascot.width);
			const float maxHeight = available.y - textSize.y - style.ItemSpacing.y;
			const float width = (std::min)(
			    {ScaledSize(kEmptyStateMascotSize), available.x * kEmptyStateMascotWidthRatio, maxHeight / aspect});
			if (width >= ScaledSize(kEmptyStateMascotMinSize)) imageSize = ImVec2(width, width * aspect);
		}

		const bool hasImage = imageSize.x > 0.0f;
		const float contentHeight = textSize.y + (hasImage ? imageSize.y + style.ItemSpacing.y : 0.0f);
		ImGui::SetCursorPosY(origin.y + (std::max)(0.0f, (available.y - contentHeight) * 0.5f));

		if (hasImage)
		{
			ImGui::SetCursorPosX(origin.x + (available.x - imageSize.x) * 0.5f);
			ImGui::Image(ImTextureRef(static_cast<ImTextureID>(mascot.id)), imageSize);
		}

		ImGui::SetCursorPosX(origin.x + (std::max)(0.0f, (available.x - textSize.x) * 0.5f));
		ImGui::TextDisabled("%s", hint);
	}

	AxisDragResult DragFloat3Axes(const char* id, float* values, float speed, float min, float max,
	                              ImGuiSliderFlags flags, const ImVec4 (&axisColors)[3])
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const float frameHeight = ImGui::GetFrameHeight();
		const float chipWidth = ImGui::CalcTextSize(kAxisNames[0]).x + style.FramePadding.x * 2.0f;
		const float spacing = style.ItemInnerSpacing.x;
		const float fieldWidth = (std::max)(1.0f, (ImGui::CalcItemWidth() - chipWidth * 3.0f - spacing * 2.0f) / 3.0f);
		ImDrawList* drawList = ImGui::GetWindowDrawList();

		AxisDragResult result;
		ImGui::PushID(id);
		ImGui::BeginGroup();
		for (int axis = 0; axis < 3; ++axis)
		{
			if (axis > 0) ImGui::SameLine(0.0f, spacing);

			const ImVec2 chipMin = ImGui::GetCursorScreenPos();
			const ImVec2 chipMax(chipMin.x + chipWidth, chipMin.y + frameHeight);
			drawList->AddRectFilled(chipMin, chipMax, ImGui::GetColorU32(axisColors[axis]), style.FrameRounding,
			                        ImDrawFlags_RoundCornersLeft);
			drawList->AddText(ImVec2(chipMin.x + style.FramePadding.x, chipMin.y + style.FramePadding.y),
			                  ImGui::GetColorU32(ImGuiCol_WindowBg), kAxisNames[axis]);
			ImGui::Dummy(ImVec2(chipWidth, frameHeight));
			ImGui::SameLine(0.0f, 0.0f);

			ImGui::PushID(axis);
			ImGui::SetNextItemWidth(fieldWidth);
			result.changed |= ImGui::DragFloat("##Value", &values[axis], speed, min, max, "%.3f", flags);
			result.activated |= ImGui::IsItemActivated();
			result.deactivatedAfterEdit |= ImGui::IsItemDeactivatedAfterEdit();
			ImGui::PopID();
		}
		ImGui::EndGroup();
		ImGui::PopID();
		return result;
	}

	ComponentCardScope::ComponentCardScope()
	    : m_drawList(ImGui::GetWindowDrawList()), m_start(ImGui::GetCursorScreenPos()),
	      m_width(ImGui::GetContentRegionAvail().x)
	{
		m_drawList->ChannelsSplit(2);
		m_drawList->ChannelsSetCurrent(1);
		ImGui::Dummy(ImVec2(0.0f, ScaledSize(kCardPadding)));
		ImGui::Indent(ScaledSize(kCardPadding));
	}

	ComponentCardScope::~ComponentCardScope()
	{
		ImGui::Unindent(ScaledSize(kCardPadding));
		ImGui::Dummy(ImVec2(0.0f, ScaledSize(kCardPadding)));

		const float bottom = ImGui::GetItemRectMax().y;
		m_drawList->ChannelsSetCurrent(0);
		m_drawList->AddRectFilled(m_start, ImVec2(m_start.x + m_width, bottom), ImGui::GetColorU32(ImGuiCol_PopupBg),
		                          ImGui::GetStyle().ChildRounding);
		m_drawList->ChannelsMerge();
	}

} // namespace NeneEngine
