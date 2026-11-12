// StatisticsPanel.cpp

#include "Editor/Panels/StatisticsPanel.h"

#include "ECS/MovementSettings.h"
#include "ECS/PhysicsSettings.h"
#include "ECS/RenderSettings.h"
#include "ECS/World.h"
#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IRenderAdapter.h"

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

		void DrawCountRow(const EditorContext& context, const char* label, uint32_t value)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(label);
			ImGui::TableNextColumn();
			if (context.monospaceFont != nullptr) ImGui::PushFont(context.monospaceFont, 0.0f);
			ImGui::Text("%u", value);
			if (context.monospaceFont != nullptr) ImGui::PopFont();
		}

		void DrawRenderPreparation(EditorContext& context)
		{
			auto& registryContext = context.world.GetRegistry().ctx();
			auto& settings = registryContext.emplace<ECS::RenderSettings>();

			ImGui::SeparatorText("Render preparation");
			ImGui::Checkbox("Use job system", &settings.useJobSystem);
			ImGui::SetItemTooltip("Compute world and render matrices in parallel on the job system workers");

			const auto* stats = registryContext.find<ECS::RenderStats>();
			if (stats == nullptr) return;

			if (ImGui::BeginTable("##RenderPreparationStats", 2, ImGuiTableFlags_SizingStretchProp))
			{
				DrawStatRow(context, stats->usedJobSystem ? "CPU time (parallel)" : "CPU time (serial)",
				            stats->preparationMs);
				DrawCountRow(context, "Transforms", stats->transformCount);
				DrawCountRow(context, "Hierarchy levels", stats->hierarchyDepth);
				DrawCountRow(context, "Render items", stats->itemCount);
				ImGui::EndTable();
			}
		}

		void DrawPhysicsSimulation(EditorContext& context)
		{
			auto& registryContext = context.world.GetRegistry().ctx();
			auto& settings = registryContext.emplace<ECS::PhysicsSettings>();

			ImGui::SeparatorText("Physics simulation");
			ImGui::Checkbox("Use job system##Physics", &settings.useJobSystem);
			ImGui::SetItemTooltip("Run Jolt physics jobs on the job system workers instead of the main thread");

			const auto* stats = registryContext.find<ECS::PhysicsStats>();
			if (stats == nullptr) return;

			if (ImGui::BeginTable("##PhysicsSimulationStats", 2, ImGuiTableFlags_SizingStretchProp))
			{
				DrawStatRow(context, stats->usedJobSystem ? "Step time (parallel)" : "Step time (serial)",
				            stats->stepMs);
				DrawCountRow(context, "Bodies", stats->bodyCount);
				ImGui::EndTable();
			}
		}

		void DrawMovement(EditorContext& context)
		{
			auto& registryContext = context.world.GetRegistry().ctx();
			auto& settings = registryContext.emplace<ECS::MovementSettings>();

			ImGui::SeparatorText("Movement");
			ImGui::Checkbox("Use job system##Movement", &settings.useJobSystem);
			ImGui::SetItemTooltip("Update moving entities in parallel on the job system workers");

			const auto* stats = registryContext.find<ECS::MovementStats>();
			if (stats == nullptr) return;

			if (ImGui::BeginTable("##MovementStats", 2, ImGuiTableFlags_SizingStretchProp))
			{
				DrawStatRow(context, stats->usedJobSystem ? "Update time (parallel)" : "Update time (serial)",
				            stats->updateMs);
				DrawCountRow(context, "Moving entities", stats->entityCount);
				ImGui::EndTable();
			}
		}

		void DrawResourceUploads(EditorContext& context)
		{
			if (context.renderer == nullptr) return;

			ImGui::SeparatorText("Resource uploads");
			bool enabled = context.renderer->IsTransferQueueEnabled();
			ImGui::BeginDisabled(!context.renderer->HasTransferQueue());
			if (ImGui::Checkbox("Use transfer queue", &enabled)) context.renderer->SetTransferQueueEnabled(enabled);
			ImGui::EndDisabled();
			ImGui::SetItemTooltip("Upload textures and meshes on the GPU copy queue from streaming workers instead of "
			                      "the main thread");
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

		DrawRenderPreparation(context);
		DrawPhysicsSimulation(context);
		DrawMovement(context);
		DrawResourceUploads(context);
	}

	void StatisticsPanel::Reset()
	{
		m_nextSample = 0;
		m_sampleCount = 0;
	}

} // namespace NeneEngine
