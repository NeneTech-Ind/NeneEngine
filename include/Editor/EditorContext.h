// EditorContext.h

#pragma once

#include "ECS/Entity.h"
#include "Editor/AssetCatalog.h"
#include "Graphics/Runtime/SceneAssetResolver.h"

namespace NeneEngine
{
	class EditorHistory;
	class IRenderAdapter;

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
		IRenderAdapter* renderer = nullptr;
		AssetCatalog assets;
		SceneAssetResolver assetResolver;
		std::string assetError;
	};

} // namespace NeneEngine
