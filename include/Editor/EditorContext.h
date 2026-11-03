// EditorContext.h

#pragma once

#include "ECS/Entity.h"

struct ImFont;

namespace NeneEngine
{
	class EditorHistory;

	namespace ECS
	{
		class World;
	}

	enum class EditorMode
	{
		Edit,
		Play
	};

	struct EditorContext
	{
		ECS::World& world;
		EditorHistory& history;
		ECS::Entity selectedEntity = ECS::NullEntity;
		ECS::Entity cameraEntity = ECS::NullEntity;
		EditorMode mode = EditorMode::Edit;
		bool isViewportHovered = false;
		bool isViewportFocused = false;
		ImFont* monospaceFont = nullptr;
	};

} // namespace NeneEngine
