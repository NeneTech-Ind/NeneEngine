// EditorHistory.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/Commands/ComponentChangeCommand.h"
#include "Editor/Commands/IEditorCommand.h"

#include <EASTL/unique_ptr.h>
#include <EASTL/vector.h>
#include <concepts>
#include <cstddef>
#include <string>
#include <utility>

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class EditorHistory final
	{
	  public:
		static constexpr size_t DefaultCapacity = 128;

		explicit EditorHistory(size_t capacity = DefaultCapacity);

		void Execute(eastl::unique_ptr<IEditorCommand> command, ECS::World& world);
		void Record(eastl::unique_ptr<IEditorCommand> command);

		template <typename Component>
		void RecordComponentChange(std::string name, ECS::Entity entity, const Component& before,
		                           const Component& after)
		{
			if constexpr (std::equality_comparable<Component>)
			{
				if (before == after) return;
			}

			Record(eastl::make_unique<ComponentChangeCommand<Component>>(std::move(name), entity, before, after));
		}

		bool Undo(ECS::World& world);
		bool Redo(ECS::World& world);
		void Clear();

		[[nodiscard]] bool CanUndo() const { return !m_undoStack.empty(); }
		[[nodiscard]] bool CanRedo() const { return !m_redoStack.empty(); }
		[[nodiscard]] const char* GetUndoName() const;
		[[nodiscard]] const char* GetRedoName() const;

	  private:
		using CommandStack = eastl::vector<eastl::unique_ptr<IEditorCommand>>;

		void PushUndo(eastl::unique_ptr<IEditorCommand> command);

		size_t m_capacity;
		CommandStack m_undoStack;
		CommandStack m_redoStack;
	};

} // namespace NeneEngine
