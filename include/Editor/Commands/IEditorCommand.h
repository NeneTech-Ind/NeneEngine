// IEditorCommand.h

#pragma once

namespace NeneEngine
{
	namespace ECS
	{
		class World;
	}

	class IEditorCommand
	{
	  public:
		virtual ~IEditorCommand() = default;

		[[nodiscard]] virtual const char* GetName() const = 0;

		virtual void Execute(ECS::World& world) = 0;
		virtual void Undo(ECS::World& world) = 0;
	};

} // namespace NeneEngine
