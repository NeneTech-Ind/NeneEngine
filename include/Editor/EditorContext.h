// EditorContext.h

#pragma once

#include "ECS/Entity.h"
#include "Graphics/Backend/IImGuiBackend.h"

#include <EASTL/array.h>
#include <cstddef>

struct ImFont;

namespace NeneEngine
{
	class EditorHistory;
	struct EditorThemeColorsConfig;

	namespace ECS
	{
		class World;
	}

	enum class EditorMascotPose
	{
		Sleep,
		Search,
		Happy,
		Scared,
		Count
	};

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
		const EditorThemeColorsConfig* themeColors = nullptr;
		eastl::array<ImGuiTexture, static_cast<size_t>(EditorMascotPose::Count)> mascots{};

		[[nodiscard]] const ImGuiTexture& GetMascot(EditorMascotPose pose) const
		{
			return mascots[static_cast<size_t>(pose)];
		}
	};

} // namespace NeneEngine
