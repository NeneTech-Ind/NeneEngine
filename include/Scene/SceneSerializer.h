// SceneSerializer.h

#pragma once

#include "ECS/World.h"

#include <filesystem>
#include <nlohmann/json.hpp>

namespace NeneEngine
{

	class SceneSerializer final
	{
	  public:
		static constexpr int CurrentVersion = 2;

		[[nodiscard]] static nlohmann::json Serialize(const ECS::World& world);
		static void Deserialize(const nlohmann::json& sceneJson, ECS::World& world, bool preserveCameras = false);

		static void SaveToFile(const ECS::World& world, const std::filesystem::path& path);
		static void LoadFromFile(const std::filesystem::path& path, ECS::World& world, bool preserveCameras = false);

	  private:
		static void DeserializeUnchecked(const nlohmann::json& sceneJson, ECS::World& world);
		SceneSerializer() = delete;
	};

} // namespace NeneEngine
