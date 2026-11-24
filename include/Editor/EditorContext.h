// EditorContext.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/AssetCatalog.h"
#include "Editor/Theme/EditorWidgets.h"
#include "Graphics/Backend/IImGuiBackend.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

#include <EASTL/array.h>
#include <EASTL/vector.h>
#include <cstddef>
#include <string>
#include <utility>

struct ImFont;

namespace NeneEngine
{
	class EditorHistory;
	class PrefabLibrary;
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
		std::string assetToReveal;
	};

	struct EditorContext
	{
		ECS::World& world;
		EditorHistory& history;
		ECS::Entity selectedEntity = ECS::NullEntity;
		ECS::Entity cameraEntity = ECS::NullEntity;
		eastl::vector<ECS::Entity> windowCameras;
		EditorMode mode = EditorMode::Edit;
		bool isViewportHovered = false;
		bool isViewportFocused = false;
		IRenderAdapter* renderer = nullptr;
		AssetCatalog assets;
		SceneAssetResolver assetResolver{};
		ResourceLoadQueue* resourceLoads = nullptr;
		PrefabLibrary* prefabs = nullptr;
		uint64_t sceneGeneration = 0;
		bool isSceneLoading = false;
		eastl::vector<ResourceLoadQueue::Completion> pendingAssets;
		EditorStatus status;
		ImFont* monospaceFont = nullptr;
		const EditorThemeColorsConfig* themeColors = nullptr;
		eastl::array<ImGuiTexture, static_cast<size_t>(EditorMascotPose::Count)> mascots{};

		[[nodiscard]] bool CanEdit() const { return mode == EditorMode::Edit && !isSceneLoading; }

		void SetStatus(EditorStatusKind kind, std::string message, std::string assetToReveal = {})
		{
			status = {kind, std::move(message), ImGui::GetTime(), std::move(assetToReveal)};
		}

		[[nodiscard]] const ImGuiTexture& GetMascot(EditorMascotPose pose) const
		{
			return mascots[static_cast<size_t>(pose)];
		}
	};

} // namespace NeneEngine
