// ConsolePanel.h

#pragma once

#include "Core/LogHistorySink.h"
#include "Editor/IEditorPanel.h"
#include "Editor/Theme/EditorIcons.h"

#include <EASTL/deque.h>
#include <EASTL/vector.h>
#include <cstddef>
#include <cstdint>
#include <string>

namespace NeneEngine
{
	class ScriptEngine;

	class ConsolePanel final : public IEditorPanel
	{
	  public:
		explicit ConsolePanel(ScriptEngine& scriptEngine) : m_scriptEngine(scriptEngine) {}

		[[nodiscard]] const char* GetName() const override { return "Console"; }
		[[nodiscard]] const char* GetIcon() const override { return NENE_ICON_FILE_CODE; }
		[[nodiscard]] EditorDockSlot GetDefaultDockSlot() const override { return EditorDockSlot::Bottom; }
		void Update(EditorContext& context) override;
		void Draw(EditorContext& context) override;

	  private:
		enum class Severity : uint8_t
		{
			Info,
			Warning,
			Error,
			Count
		};

		struct Line
		{
			Severity severity = Severity::Info;
			bool isFirstLine = true;
			std::string text;
		};

		void Append(const LogEntry& entry);
		void Clear();
		void DrawToolbar();
		void DrawLines(const EditorContext& context);
		void CopyVisibleLines() const;
		static int NavigateCommandHistory(ImGuiInputTextCallbackData* data);
		[[nodiscard]] bool IsVisible(const Line& line) const;

		ScriptEngine& m_scriptEngine;
		std::string m_command;
		eastl::deque<std::string> m_commandHistory;
		size_t m_historyPosition = 0;
		std::string m_commandDraft;
		eastl::deque<Line> m_lines;
		eastl::vector<LogEntry> m_incoming;
		eastl::vector<size_t> m_visible;
		uint64_t m_lastSequence = 0;
		size_t m_counts[static_cast<size_t>(Severity::Count)]{};
		bool m_shown[static_cast<size_t>(Severity::Count)]{true, true, true};
		bool m_autoScroll = true;
		ImGuiTextFilter m_filter;
	};
} // namespace NeneEngine
