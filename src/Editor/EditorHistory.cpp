// EditorHistory.cpp

#include "Editor/EditorHistory.h"

#include "Core/NeneLogger.h"

namespace NeneEngine
{

	EditorHistory::EditorHistory(size_t capacity) : m_capacity(capacity > 0 ? capacity : 1)
	{
	}

	void EditorHistory::Execute(eastl::unique_ptr<IEditorCommand> command, ECS::World& world)
	{
		if (!command) return;

		command->Execute(world);
		Record(std::move(command));
	}

	void EditorHistory::Record(eastl::unique_ptr<IEditorCommand> command)
	{
		if (!command) return;

		m_redoStack.clear();
		PushUndo(std::move(command));
	}

	bool EditorHistory::Undo(ECS::World& world)
	{
		if (m_undoStack.empty()) return false;

		eastl::unique_ptr<IEditorCommand> command = std::move(m_undoStack.back());
		m_undoStack.pop_back();

		command->Undo(world);
		NENE_LOG_DEBUG("Editor undo: {}", command->GetName());
		m_redoStack.push_back(std::move(command));
		return true;
	}

	bool EditorHistory::Redo(ECS::World& world)
	{
		if (m_redoStack.empty()) return false;

		eastl::unique_ptr<IEditorCommand> command = std::move(m_redoStack.back());
		m_redoStack.pop_back();

		command->Execute(world);
		NENE_LOG_DEBUG("Editor redo: {}", command->GetName());
		PushUndo(std::move(command));
		return true;
	}

	void EditorHistory::Clear()
	{
		m_undoStack.clear();
		m_redoStack.clear();
	}

	const char* EditorHistory::GetUndoName() const
	{
		return m_undoStack.empty() ? nullptr : m_undoStack.back()->GetName();
	}

	const char* EditorHistory::GetRedoName() const
	{
		return m_redoStack.empty() ? nullptr : m_redoStack.back()->GetName();
	}

	void EditorHistory::PushUndo(eastl::unique_ptr<IEditorCommand> command)
	{
		if (m_undoStack.size() >= m_capacity) m_undoStack.erase(m_undoStack.begin());

		m_undoStack.push_back(std::move(command));
	}

} // namespace NeneEngine
