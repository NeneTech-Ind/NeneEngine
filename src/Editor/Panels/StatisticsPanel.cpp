// StatisticsPanel.cpp

#include "Editor/Panels/StatisticsPanel.h"

#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"

#include <algorithm>
#include <format>
#include <imgui.h>
#include <numeric>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr float kPlotHeight = 80.0f;
		constexpr float kMinPlotScaleMs = 1000.0f / 60.0f;

		float Percentile(const eastl::vector<float>& sortedValues, float fraction)
		{
			const auto index = static_cast<size_t>(fraction * static_cast<float>(sortedValues.size() - 1) + 0.5f);
			return sortedValues[index];
		}

		void DrawStatRow(const EditorContext& context, const char* label, float valueMs)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			if (context.monospaceFont != nullptr) ImGui::PushFont(context.monospaceFont, 0.0f);
			ImGui::Text("%.2f ms", valueMs);
			if (context.monospaceFont != nullptr) ImGui::PopFont();
		}
	} // namespace

	void StatisticsPanel::Update(EditorContext& /*context*/)
	{
		const float deltaTime = ImGui::GetIO().DeltaTime;
		if (deltaTime <= 0.0f) return;

		m_frameTimesMs[m_nextSample] = deltaTime * 1000.0f;
		m_nextSample = (m_nextSample + 1) % kSampleCount;
		m_sampleCount = (std::min)(m_sampleCount + 1, kSampleCount);
	}

	void StatisticsPanel::Draw(EditorContext& context)
	{
		if (m_sampleCount == 0)
		{
			DrawEmptyState(context.GetMascot(EditorMascotPose::Sleep), "No statistics collected yet");
			return;
		}

		const EditorThemeColorsConfig& colors = *context.themeColors;
		m_sortedFrameTimesMs.assign(m_frameTimesMs.begin(), m_frameTimesMs.begin() + m_sampleCount);
		std::sort(m_sortedFrameTimesMs.begin(), m_sortedFrameTimesMs.end());

		const float averageMs = std::accumulate(m_sortedFrameTimesMs.begin(), m_sortedFrameTimesMs.end(), 0.0f) /
		                        static_cast<float>(m_sampleCount);
		const float lastMs = m_frameTimesMs[(m_nextSample + kSampleCount - 1) % kSampleCount];
		const float maxMs = m_sortedFrameTimesMs.back();
		const size_t firstSample = m_sampleCount < kSampleCount ? 0 : m_nextSample;

		if (ImGui::Button(NENE_ICON_REFRESH "##ResetStatistics")) Reset();
		ImGui::SetItemTooltip("Reset statistics");
		ImGui::SameLine();
		const float averageFps = averageMs > 0.0f ? 1000.0f / averageMs : 0.0f;
		const std::string summary = std::format("Last {} frames, {:.0f} FPS on average", m_sampleCount, averageFps);
		StatusText(colors, EditorStatusKind::Muted, summary.c_str());

		ImGui::PushStyleColor(ImGuiCol_PlotLines, colors.accent);
		ImGui::PlotLines("##FrameTimes", m_frameTimesMs.data(), static_cast<int>(m_sampleCount),
		                 static_cast<int>(firstSample), nullptr, 0.0f, (std::max)(maxMs, kMinPlotScaleMs),
		                 ImVec2(-FLT_MIN, ScaledSize(kPlotHeight)));
		ImGui::PopStyleColor();

		if (ImGui::BeginTable("##FrameTimeStats", 2, ImGuiTableFlags_SizingStretchProp))
		{
			DrawStatRow(context, "Last frame", lastMs);
			DrawStatRow(context, "Average", averageMs);
			DrawStatRow(context, "Median", Percentile(m_sortedFrameTimesMs, 0.5f));
			DrawStatRow(context, "95th percentile", Percentile(m_sortedFrameTimesMs, 0.95f));
			DrawStatRow(context, "99th percentile", Percentile(m_sortedFrameTimesMs, 0.99f));
			DrawStatRow(context, "Max", maxMs);
			ImGui::EndTable();
		}
	}

	void StatisticsPanel::Reset()
	{
		m_nextSample = 0;
		m_sampleCount = 0;
	}

} // namespace NeneEngine
