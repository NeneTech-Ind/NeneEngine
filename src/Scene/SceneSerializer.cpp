// SceneSerializer.cpp

#include "Scene/SceneSerializer.h"
#include "Core/Profiler.h"
#include "Scene/EntitySerializer.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/ColliderComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/PlayerControllerComponent.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/RigidbodyComponent.h"
#include "ECS/Components/ScriptComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/DebugDrawSettings.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/MovementSettings.h"
#include "ECS/PhysicsSettings.h"
#include "ECS/RenderSettings.h"

#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <Windows.h>

#include <fstream>
#include <stdexcept>
#include <string>

namespace NeneEngine
{

	nlohmann::json SceneSerializer::Serialize(const ECS::World& world)
	{
		nlohmann::json sceneJson{{"version", CurrentVersion}, {"entities", nlohmann::json::array()}};
		EntitySerializationMap sceneIdsByEntity;

		const auto allEntities = world.GetRegistry().view<entt::entity>();
		uint32_t nextSceneId = 1;
		for (auto entity : allEntities) sceneIdsByEntity.emplace(entity, nextSceneId++);

		for (auto entity : allEntities)
			sceneJson["entities"].push_back(SerializeEntity(world, entity, sceneIdsByEntity));

		return sceneJson;
	}

	void SceneSerializer::DeserializeUnchecked(const nlohmann::json& sceneJson, ECS::World& world)
	{
		const int version = sceneJson.at("version").get<int>();
		// Keep loading strict so scene migrations are explicit instead of silently dropping component data.
		if (version != CurrentVersion)
			throw std::runtime_error("Unsupported scene version: " + std::to_string(version));

		world.GetRegistry().clear();
		EntityDeserializationMap entitiesBySceneId;

		for (const auto& entityJson : sceneJson.at("entities"))
		{
			const ECS::Entity entity = world.GetRegistry().create();
			const uint32_t sceneId = entityJson.at("id").get<uint32_t>();
			const auto [_, inserted] = entitiesBySceneId.emplace(sceneId, entity);
			if (!inserted) throw std::runtime_error("Duplicate scene entity id: " + std::to_string(sceneId));
		}

		for (const auto& entityJson : sceneJson.at("entities")) DeserializeEntity(entityJson, world, entitiesBySceneId);
	}

	namespace
	{
		template <typename... Components>
		void CopyComponents(const entt::registry& source, entt::registry& target, ECS::Entity from, ECS::Entity to,
		                    bool replace)
		{
			(
			    [&]
			    {
				    const auto* component = source.try_get<Components>(from);
				    if (component && (replace || !target.all_of<Components>(to)))
					    target.emplace_or_replace<Components>(to, *component);
			    }(),
			    ...);
		}

		void CopyEntity(const entt::registry& source, entt::registry& target, ECS::Entity from, ECS::Entity to,
		                bool replace = true)
		{
			CopyComponents<ECS::TagComponent, ECS::TransformComponent, ECS::CameraComponent,
			               ECS::CameraControllerComponent, ECS::MeshRendererComponent, ECS::MovementComponent,
			               ECS::PlayerControllerComponent, ECS::PrimitiveControlComponent, ECS::RigidbodyComponent,
			               ECS::ColliderComponent, ECS::ScriptComponent>(source, target, from, to, replace);
		}
	} // namespace

	void SceneSerializer::Deserialize(const nlohmann::json& sceneJson, ECS::World& world, bool preserveCameras)
	{
		Apply(Prepare(sceneJson, preserveCameras ? CaptureCameras(world) : eastl::vector<SceneCamera>{}), world);
	}

	eastl::vector<SceneCamera> SceneSerializer::CaptureCameras(const ECS::World& world)
	{
		eastl::vector<SceneCamera> cameras;
		const auto& registry = world.GetRegistry();
		for (const auto [entity, camera] : registry.view<const ECS::CameraComponent>().each())
		{
			const auto* tag = registry.try_get<ECS::TagComponent>(entity);
			cameras.push_back({entity, tag ? eastl::optional<std::string>(tag->name) : eastl::nullopt, camera.isPrimary});
		}
		return cameras;
	}

	PreparedScene SceneSerializer::Prepare(const nlohmann::json& sceneJson, const eastl::vector<SceneCamera>& cameras)
	{
		NENE_PROFILE_SCOPE("Scene Prepare");
		ECS::World parsed;
		DeserializeUnchecked(sceneJson, parsed);
		PreparedScene prepared{eastl::make_shared<ECS::World>(), {}};
		auto& source = parsed.GetRegistry();
		auto& target = prepared.world->GetRegistry();
		eastl::unordered_map<ECS::Entity, ECS::Entity> remap;
		eastl::unordered_set<ECS::Entity> usedCameras;
		for (const auto& camera : cameras)
		{
			if (target.create(camera.entity) != camera.entity)
				throw std::runtime_error("Could not preserve application camera");
			prepared.cameras.push_back(camera.entity);
		}
		for (const auto entity : source.view<ECS::CameraComponent>())
		{
			const auto* tag = source.try_get<ECS::TagComponent>(entity);
			const bool isPrimary = source.get<ECS::CameraComponent>(entity).isPrimary;
			ECS::Entity match = ECS::NullEntity;
			for (const auto& camera : cameras)
			{
				if (usedCameras.contains(camera.entity) || camera.isPrimary != isPrimary) continue;
				if (match == ECS::NullEntity) match = camera.entity;
				if (tag && camera.name && tag->name == *camera.name)
				{
					match = camera.entity;
					break;
				}
			}
			if (match != ECS::NullEntity)
			{
				remap.emplace(entity, match);
				usedCameras.insert(match);
			}
		}
		for (const auto entity : source.view<entt::entity>())
		{
			if (!remap.contains(entity)) remap.emplace(entity, target.create());
			CopyEntity(source, target, entity, remap.at(entity));
		}
		for (const auto [entity, hierarchy] : source.view<ECS::HierarchyComponent>().each())
		{
			auto copy = hierarchy;
			if (copy.parent != ECS::NullEntity) copy.parent = remap.at(copy.parent);
			for (auto& child : copy.children) child = remap.at(child);
			target.emplace<ECS::HierarchyComponent>(remap.at(entity), std::move(copy));
		}
		return prepared;
	}

	void SceneSerializer::Apply(const PreparedScene& scene, ECS::World& world)
	{
		NENE_PROFILE_SCOPE("Scene Apply");
		auto& source = world.GetRegistry();
		auto& target = scene.world->GetRegistry();
		for (const auto camera : scene.cameras)
			if (source.valid(camera)) CopyEntity(source, target, camera, camera, false);
		if (const auto* settings = source.ctx().find<ECS::DebugDrawSettings>())
			target.ctx().emplace<ECS::DebugDrawSettings>(*settings);
		if (const auto* settings = source.ctx().find<ECS::RenderSettings>())
			target.ctx().emplace<ECS::RenderSettings>(*settings);
		if (const auto* settings = source.ctx().find<ECS::PhysicsSettings>())
			target.ctx().emplace<ECS::PhysicsSettings>(*settings);
		if (const auto* settings = source.ctx().find<ECS::MovementSettings>())
			target.ctx().emplace<ECS::MovementSettings>(*settings);
		source.swap(target);
		world.GetEventBus().Publish(ECS::SceneRestoredEvent{});
	}

	void SceneSerializer::SaveToFile(const ECS::World& world, const std::filesystem::path& path)
	{
		const std::string serializedScene = Serialize(world).dump(4);

		std::filesystem::path tempPath = path;
		tempPath += ".tmp";
		{
			std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
			if (!file.is_open())
				throw std::runtime_error("Failed to open scene file for writing: " + tempPath.string());

			file << serializedScene;
			file.flush();
			if (!file) throw std::runtime_error("Failed to write scene file: " + tempPath.string());
		}

		std::error_code errorCode;
		if (!MoveFileExW(tempPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
		{
			std::filesystem::remove(tempPath, errorCode);
			throw std::runtime_error("Failed to replace scene file: " + path.string());
		}
	}

	void SceneSerializer::LoadFromFile(const std::filesystem::path& path, ECS::World& world, bool preserveCameras)
	{
		Deserialize(ReadFromFile(path), world, preserveCameras);
	}

	nlohmann::json SceneSerializer::ReadFromFile(const std::filesystem::path& path)
	{
		NENE_PROFILE_SCOPE("Scene File Load");
		std::ifstream file(path);
		if (!file.is_open()) throw std::runtime_error("Failed to open scene file for reading: " + path.string());

		nlohmann::json sceneJson;
		file >> sceneJson;
		return sceneJson;
	}

} // namespace NeneEngine
