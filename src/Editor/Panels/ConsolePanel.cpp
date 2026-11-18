// ConsolePanel.cpp

#include "Editor/Panels/ConsolePanel.h"

#include "Core/NeneLogger.h"
#include "ECS/Components/ScriptComponent.h"
#include "ECS/World.h"
#include "Editor/EditorContext.h"
#include "Editor/Theme/EditorThemeConfig.h"
#include "Scripting/ScriptEngine.h"

#include <format>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace NeneEngine
{
	namespace
	{
		constexpr size_t kMaxLines = 4000;
		constexpr size_t kMaxCommands = 100;
		constexpr const char* kContinuationIndent = "           ";
	} // namespace

	void ConsolePanel::Update(EditorContext& /*context*/)
	{
		LogHistorySink* history = NeneLogger::GetInstance().GetHistory();
		if (history == nullptr) return;

		m_incoming.clear();
		m_lastSequence = history->CopySince(m_lastSequence, m_incoming);
		for (const LogEntry& entry : m_incoming) Append(entry);
	}

	void ConsolePanel::Append(const LogEntry& entry)
	{
		Severity severity = Severity::Info;
		if (entry.level >= spdlog::level::err)
			severity = Severity::Error;
		else if (entry.level == spdlog::level::warn)
			severity = Severity::Warning;
		++m_counts[static_cast<size_t>(severity)];

		size_t start = 0;
		bool isFirstLine = true;
		while (start <= entry.message.size())
		{
			const size_t end = entry.message.find('\n', start);
			const size_t length = (end == std::string::npos ? entry.message.size() : end) - start;
			std::string text = isFirstLine ? std::format("[{}] ", entry.time) : std::string(kContinuationIndent);
			text.append(entry.message, start, length);
			if (!text.empty() && text.back() == '\r') text.pop_back();
			m_lines.push_back({severity, isFirstLine, std::move(text)});
			isFirstLine = false;
			if (end == std::string::npos) break;
			start = end + 1;
		}

		while (m_lines.size() > kMaxLines)
		{
			if (m_lines.front().isFirstLine) --m_counts[static_cast<size_t>(m_lines.front().severity)];
			m_lines.pop_front();
		}
	}

	void ConsolePanel::Clear()
	{
		m_lines.clear();
		for (size_t& count : m_counts) count = 0;
	}

	bool ConsolePanel::IsVisible(const Line& line) const
	{
		return m_shown[static_cast<size_t>(line.severity)] && m_filter.PassFilter(line.text.c_str());
	}

	void ConsolePanel::Draw(EditorContext& context)
	{
		DrawToolbar();
		ImGui::Separator();
		DrawLines(context);
		ImGui::Separator();
		const bool canExecute = context.mode == EditorMode::Play;
		const char* commandHint = canExecute ? "Lua command (Enter to run)" : "Enter Play mode to run Lua commands";
		ImGui::BeginDisabled(!canExecute);
		ImGui::SetNextItemWidth(-FLT_MIN);
		const bool submitted = ImGui::InputTextWithHint(
		    "##LuaCommand", commandHint, &m_command,
		    ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory, NavigateCommandHistory, this);
		ImGui::EndDisabled();
		if (!canExecute && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("%s", commandHint);
		else if (canExecute)
			ImGui::SetItemTooltip("Up/Down: command history. Use selected_fields to read the selected entity's script settings.");
		if (canExecute && submitted)
		{
			if (m_command.find_first_not_of(" \t\r\n") != std::string::npos &&
			    (m_commandHistory.empty() || m_commandHistory.back() != m_command))
			{
				m_commandHistory.push_back(m_command);
				if (m_commandHistory.size() > kMaxCommands) m_commandHistory.pop_front();
			}
			m_historyPosition = m_commandHistory.size();
			m_commandDraft.clear();

			const auto* selectedScript = context.world.GetRegistry().valid(context.selectedEntity)
			                                 ? context.world.GetComponent<ECS::ScriptComponent>(context.selectedEntity)
			                                 : nullptr;
			m_scriptEngine.ExecuteConsoleCommand(m_command, selectedScript);
			m_command.clear();
			ImGui::SetKeyboardFocusHere(-1);
		}
	}

	int ConsolePanel::NavigateCommandHistory(ImGuiInputTextCallbackData* data)
	{
		auto& console = *static_cast<ConsolePanel*>(data->UserData);
		const size_t end = console.m_commandHistory.size();
		auto& position = console.m_historyPosition;
		if (data->EventKey == ImGuiKey_UpArrow && position > 0)
		{
			if (position == end) console.m_commandDraft.assign(data->Buf, data->BufTextLen);
			--position;
		}
		else if (data->EventKey == ImGuiKey_DownArrow && position < end)
		{
			++position;
		}
		else
		{
			return 0;
		}

		const auto& command = position == end ? console.m_commandDraft : console.m_commandHistory[position];
		data->DeleteChars(0, data->BufTextLen);
		data->InsertChars(0, command.data(), command.data() + command.size());
		return 0;
	}

	void ConsolePanel::DrawToolbar()
	{
		if (ImGui::Button("Clear")) Clear();
		ImGui::SetItemTooltip("Remove all messages from the console. The log file keeps them");
		ImGui::SameLine();
		if (ImGui::Button("Copy")) CopyVisibleLines();
		ImGui::SetItemTooltip("Copy the visible messages to the clipboard");

		constexpr const char* kSeverityNames[] = {"Info", "Warnings", "Errors"};
		for (size_t index = 0; index < static_cast<size_t>(Severity::Count); ++index)
		{
			ImGui::SameLine();
			const std::string label = std::format("{} {}###Severity{}", kSeverityNames[index], m_counts[index], index);
			ImGui::Checkbox(label.c_str(), &m_shown[index]);
		}

		ImGui::SameLine();
		ImGui::Checkbox("Auto-scroll", &m_autoScroll);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputTextWithHint("##ConsoleFilter", NENE_ICON_SEARCH " Filter messages", m_filter.InputBuf,
		                             IM_ARRAYSIZE(m_filter.InputBuf)))
			m_filter.Build();
	}

	void ConsolePanel::DrawLines(const EditorContext& context)
	{
		const float commandHeight = ImGui::GetFrameHeight() + 2.0f * ImGui::GetStyle().ItemSpacing.y + 1.0f;
		if (!ImGui::BeginChild("##ConsoleLines", ImVec2(0.0f, -commandHeight), ImGuiChildFlags_None,
		                       ImGuiWindowFlags_HorizontalScrollbar))
		{
			ImGui::EndChild();
			return;
		}

		const EditorThemeColorsConfig& colors = *context.themeColors;
		const ImVec4 severityColors[] = {colors.text, colors.accentPlay, colors.danger};

		m_visible.clear();
		for (size_t index = 0; index < m_lines.size(); ++index)
		{
			if (IsVisible(m_lines[index])) m_visible.push_back(index);
		}

		if (context.monospaceFont != nullptr) ImGui::PushFont(context.monospaceFont, 0.0f);
		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(m_visible.size()));
		while (clipper.Step())
		{
			for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
			{
				const Line& line = m_lines[m_visible[static_cast<size_t>(row)]];
				ImGui::PushStyleColor(ImGuiCol_Text, severityColors[static_cast<size_t>(line.severity)]);
				ImGui::TextUnformatted(line.text.c_str(), line.text.c_str() + line.text.size());
				ImGui::PopStyleColor();
			}
		}
		clipper.End();
		if (context.monospaceFont != nullptr) ImGui::PopFont();

		if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);

		ImGui::EndChild();
	}

	void ConsolePanel::CopyVisibleLines() const
	{
		std::string text;
		for (const Line& line : m_lines)
		{
			if (!IsVisible(line)) continue;
			text += line.text;
			text += '\n';
		}
		ImGui::SetClipboardText(text.c_str());
	}
} // namespace NeneEngine
