// SceneSerializer.h

#pragma once

#include "ECS/World.h"

#include <EASTL/shared_ptr.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

namespace NeneEngine
{

	struct SceneCamera
	{
		ECS::Entity entity = ECS::NullEntity;
		std::optional<std::string> name;
		bool isPrimary = false;
	};

	struct PreparedScene
	{
		eastl::shared_ptr<ECS::World> world;
		std::vector<ECS::Entity> cameras;
	};

	class SceneSerializer final
	{
	  public:
		static constexpr int CurrentVersion = 2;

		[[nodiscard]] static nlohmann::json Serialize(const ECS::World& world);
		static void Deserialize(const nlohmann::json& sceneJson, ECS::World& world, bool preserveCameras = false);

		[[nodiscard]] static std::vector<SceneCamera> CaptureCameras(const ECS::World& world);
		[[nodiscard]] static PreparedScene Prepare(const nlohmann::json& sceneJson,
		                                           const std::vector<SceneCamera>& cameras = {});
		static void Apply(const PreparedScene& scene, ECS::World& world);

		static void SaveToFile(const ECS::World& world, const std::filesystem::path& path);
		static void LoadFromFile(const std::filesystem::path& path, ECS::World& world, bool preserveCameras = false);
		[[nodiscard]] static nlohmann::json ReadFromFile(const std::filesystem::path& path);

	  private:
		static void DeserializeUnchecked(const nlohmann::json& sceneJson, ECS::World& world);
		SceneSerializer() = delete;
	};

} // namespace NeneEngine
