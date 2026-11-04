// EditorContext.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/AssetCatalog.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IImGuiBackend.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

#include <EASTL/array.h>
#include <cstddef>
#include <string>
#include <utility>

struct ImFont;

namespace NeneEngine
{
	class EditorHistory;
	class IRenderAdapter;
	struct EditorThemeColorsConfig;

	namespace ECS
	{
		class World;
	}

	inline constexpr const char* kProductName = "NeneEngine";

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

	struct EditorStatus
	{
		EditorStatusKind kind = EditorStatusKind::Muted;
		std::string message;
		double time = 0.0;
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
		IRenderAdapter* renderer = nullptr;
		AssetCatalog assets;
		SceneAssetResolver assetResolver;
		EditorStatus status;
		ImFont* monospaceFont = nullptr;
		const EditorThemeColorsConfig* themeColors = nullptr;
		eastl::array<ImGuiTexture, static_cast<size_t>(EditorMascotPose::Count)> mascots{};

		void SetStatus(EditorStatusKind kind, std::string message)
		{
			status = {kind, std::move(message), ImGui::GetTime()};
		}

		[[nodiscard]] const ImGuiTexture& GetMascot(EditorMascotPose pose) const
		{
			return mascots[static_cast<size_t>(pose)];
		}
	};

} // namespace NeneEngine
