// SceneSerializer.cpp

#include "Scene/SceneSerializer.h"

#include "ECS/Components/CameraComponent.h"
#include "ECS/Components/CameraControllerComponent.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/MovementComponent.h"
#include "ECS/Components/PrimitiveControlComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"

#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace NeneEngine
{
	namespace
	{

		nlohmann::json ToJson(const glm::vec3& value)
		{
			return {{"x", value.x}, {"y", value.y}, {"z", value.z}};
		}

		nlohmann::json ToJson(const glm::vec4& value)
		{
			return {{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
		}

		nlohmann::json ToJson(const glm::quat& value)
		{
			return {{"x", value.x}, {"y", value.y}, {"z", value.z}, {"w", value.w}};
		}

		glm::vec3 ReadVec3(const nlohmann::json& value)
		{
			return {value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>()};
		}

		glm::vec4 ReadVec4(const nlohmann::json& value)
		{
			return {value.at("x").get<float>(), value.at("y").get<float>(), value.at("z").get<float>(),
			        value.at("w").get<float>()};
		}

		glm::quat ReadQuat(const nlohmann::json& value)
		{
			return glm::quat(value.at("w").get<float>(), value.at("x").get<float>(), value.at("y").get<float>(),
			                 value.at("z").get<float>());
		}

		std::string ToString(PrimitiveType primitiveType)
		{
			switch (primitiveType)
			{
			case PrimitiveType::Line:
				return "Line";
			case PrimitiveType::Triangle:
				return "Triangle";
			case PrimitiveType::Quad:
				return "Quad";
			case PrimitiveType::Cube:
				return "Cube";
			default:
				return "Triangle";
			}
		}

		PrimitiveType ReadPrimitiveType(const std::string& value)
		{
			if (value == "Line") return PrimitiveType::Line;
			if (value == "Triangle") return PrimitiveType::Triangle;
			if (value == "Quad") return PrimitiveType::Quad;
			if (value == "Cube") return PrimitiveType::Cube;

			throw std::runtime_error("Unknown PrimitiveType: " + value);
		}

		uint32_t ToEntityKey(ECS::Entity entity)
		{
			return static_cast<uint32_t>(entt::to_integral(entity));
		}

		nlohmann::json SerializeTag(const ECS::TagComponent& tag)
		{
			return {{"name", tag.name}};
		}

		nlohmann::json SerializeTransform(const ECS::TransformComponent& transform)
		{
			return {{"position", ToJson(transform.position)},
			        {"rotation", ToJson(transform.rotation)},
			        {"scale", ToJson(transform.scale)}};
		}

		nlohmann::json SerializeCamera(const ECS::CameraComponent& camera)
		{
			return {{"forward", ToJson(camera.forward)},
			        {"up", ToJson(camera.up)},
			        {"fovDegrees", camera.fovDegrees},
			        {"nearPlane", camera.nearPlane},
			        {"farPlane", camera.farPlane},
			        {"aspectRatio", camera.aspectRatio},
			        {"isPrimary", camera.isPrimary}};
		}

		nlohmann::json SerializeCameraController(const ECS::CameraControllerComponent& controller)
		{
			return {{"moveSpeed", controller.moveSpeed},
			        {"sprintMultiplier", controller.sprintMultiplier},
			        {"lookSensitivity", controller.lookSensitivity},
			        {"yawRadians", controller.yawRadians},
			        {"pitchRadians", controller.pitchRadians},
			        {"maxPitchRadians", controller.maxPitchRadians},
			        {"rotateWithRightMouse", controller.rotateWithRightMouse}};
		}

		nlohmann::json SerializeMeshRenderer(const ECS::MeshRendererComponent& renderer)
		{
			return {{"primitiveType", ToString(renderer.primitiveType)},
			        {"visible", renderer.visible},
			        {"cullingRadius", renderer.cullingRadius},
			        {"material", {{"tint", ToJson(renderer.tint)}}}};
		}

		nlohmann::json SerializeMovement(const ECS::MovementComponent& movement)
		{
			return {{"velocity", ToJson(movement.velocity)},
			        {"origin", ToJson(movement.origin)},
			        {"oscillationAxis", ToJson(movement.oscillationAxis)},
			        {"oscillationAmplitude", movement.oscillationAmplitude},
			        {"oscillationSpeed", movement.oscillationSpeed},
			        {"elapsedTime", movement.elapsedTime},
			        {"useOscillation", movement.useOscillation}};
		}

		nlohmann::json SerializeHierarchy(const ECS::HierarchyComponent& hierarchy,
		                                  const std::unordered_map<uint32_t, uint32_t>& sceneIdsByEntity)
		{
			nlohmann::json children = nlohmann::json::array();

			for (ECS::Entity child : hierarchy.children)
			{
				const auto childIt = sceneIdsByEntity.find(ToEntityKey(child));
				if (childIt == sceneIdsByEntity.end())
					throw std::runtime_error("Hierarchy child points to entity not present in serialized scene");

				children.push_back(childIt->second);
			}

			nlohmann::json parent = nullptr;
			if (hierarchy.parent != ECS::NullEntity)
			{
				const auto parentIt = sceneIdsByEntity.find(ToEntityKey(hierarchy.parent));
				if (parentIt == sceneIdsByEntity.end())
					throw std::runtime_error("Hierarchy parent points to entity not present in serialized scene");

				parent = parentIt->second;
			}

			return {{"parent", std::move(parent)}, {"children", std::move(children)}};
		}

		nlohmann::json SerializePrimitiveControl(const ECS::PrimitiveControlComponent& control)
		{
			return {{"moveSpeed", control.moveSpeed},
			        {"rotationStepRadians", control.rotationStepRadians},
			        {"scaleSmoothing", control.scaleSmoothing},
			        {"rotationSmoothing", control.rotationSmoothing},
			        {"currentScaleLevel", control.currentScaleLevel},
			        {"targetRotationRadians", control.targetRotationRadians},
			        {"targetScale", ToJson(control.targetScale)}};
		}

		void DeserializeTag(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& tag = world.HasComponent<ECS::TagComponent>(entity) ? *world.GetComponent<ECS::TagComponent>(entity)
			                                                          : world.AddComponent<ECS::TagComponent>(entity);
			tag.name = value.at("name").get<std::string>();
		}

		void DeserializeTransform(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& transform = world.AddComponent<ECS::TransformComponent>(entity);
			transform.position = ReadVec3(value.at("position"));
			transform.rotation = ReadQuat(value.at("rotation"));
			transform.scale = ReadVec3(value.at("scale"));
		}

		void DeserializeCamera(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& camera = world.AddComponent<ECS::CameraComponent>(entity);
			if (value.contains("forward")) camera.forward = ReadVec3(value.at("forward"));
			if (value.contains("up")) camera.up = ReadVec3(value.at("up"));
			camera.fovDegrees = value.at("fovDegrees").get<float>();
			camera.nearPlane = value.at("nearPlane").get<float>();
			camera.farPlane = value.at("farPlane").get<float>();
			camera.aspectRatio = value.at("aspectRatio").get<float>();
			camera.isPrimary = value.at("isPrimary").get<bool>();
		}

		void DeserializeCameraController(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& controller = world.AddComponent<ECS::CameraControllerComponent>(entity);
			controller.moveSpeed = value.at("moveSpeed").get<float>();
			if (value.contains("sprintMultiplier"))
				controller.sprintMultiplier = value.at("sprintMultiplier").get<float>();
			controller.lookSensitivity = value.at("lookSensitivity").get<float>();
			controller.yawRadians = value.at("yawRadians").get<float>();
			controller.pitchRadians = value.at("pitchRadians").get<float>();
			controller.maxPitchRadians = value.at("maxPitchRadians").get<float>();
			controller.rotateWithRightMouse = value.at("rotateWithRightMouse").get<bool>();
		}

		void DeserializeMeshRenderer(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& renderer = world.AddComponent<ECS::MeshRendererComponent>(entity);
			renderer.primitiveType = ReadPrimitiveType(value.at("primitiveType").get<std::string>());
			renderer.visible = value.at("visible").get<bool>();
			renderer.cullingRadius = value.value("cullingRadius", renderer.cullingRadius);

			const auto& material = value.at("material");
			renderer.tint = ReadVec4(material.at("tint"));
		}

		void DeserializeMovement(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& movement = world.AddComponent<ECS::MovementComponent>(entity);
			movement.velocity = ReadVec3(value.at("velocity"));
			movement.origin = ReadVec3(value.at("origin"));
			movement.oscillationAxis = ReadVec3(value.at("oscillationAxis"));
			movement.oscillationAmplitude = value.at("oscillationAmplitude").get<float>();
			movement.oscillationSpeed = value.at("oscillationSpeed").get<float>();
			movement.elapsedTime = value.at("elapsedTime").get<float>();
			movement.useOscillation = value.at("useOscillation").get<bool>();
		}

		void DeserializePrimitiveControl(const nlohmann::json& value, ECS::World& world, ECS::Entity entity)
		{
			auto& control = world.AddComponent<ECS::PrimitiveControlComponent>(entity);
			control.moveSpeed = value.at("moveSpeed").get<float>();
			control.rotationStepRadians = value.at("rotationStepRadians").get<float>();
			control.scaleSmoothing = value.at("scaleSmoothing").get<float>();
			control.rotationSmoothing = value.at("rotationSmoothing").get<float>();
			if (value.contains("currentScaleLevel"))
				control.currentScaleLevel = value.at("currentScaleLevel").get<size_t>();
			control.targetRotationRadians = value.at("targetRotationRadians").get<float>();
			control.targetScale = ReadVec3(value.at("targetScale"));
		}

		void DeserializeHierarchy(const nlohmann::json& value, ECS::World& world, ECS::Entity entity,
		                          const std::unordered_map<uint32_t, ECS::Entity>& entitiesBySceneId)
		{
			auto& hierarchy = world.AddComponent<ECS::HierarchyComponent>(entity);
			if (value.contains("parent") && !value.at("parent").is_null())
			{
				const uint32_t parentSceneId = value.at("parent").get<uint32_t>();
				const auto parentIt = entitiesBySceneId.find(parentSceneId);
				if (parentIt == entitiesBySceneId.end())
					throw std::runtime_error("Hierarchy parent references unknown scene entity id " +
					                         std::to_string(parentSceneId));

				hierarchy.parent = parentIt->second;
			}

			hierarchy.children.clear();
			if (value.contains("children"))
			{
				for (const auto& childValue : value.at("children"))
				{
					const uint32_t childSceneId = childValue.get<uint32_t>();
					const auto childIt = entitiesBySceneId.find(childSceneId);
					if (childIt == entitiesBySceneId.end())
						throw std::runtime_error("Hierarchy child references unknown scene entity id " +
						                         std::to_string(childSceneId));

					hierarchy.children.push_back(childIt->second);
				}
			}
		}

	} // namespace

	nlohmann::json SceneSerializer::Serialize(const ECS::World& world)
	{
		nlohmann::json sceneJson{{"version", CurrentVersion}, {"entities", nlohmann::json::array()}};
		std::unordered_map<uint32_t, uint32_t> sceneIdsByEntity;

		const auto allEntities = world.GetRegistry().view<entt::entity>();
		uint32_t nextSceneId = 1;
		for (auto entity : allEntities) sceneIdsByEntity.emplace(ToEntityKey(entity), nextSceneId++);

		for (auto entity : allEntities)
		{
			const auto sceneIdIt = sceneIdsByEntity.find(ToEntityKey(entity));
			if (sceneIdIt == sceneIdsByEntity.end())
				throw std::runtime_error("Serialized entity is missing a scene-local id");

			nlohmann::json entityJson{{"id", sceneIdIt->second}, {"components", nlohmann::json::object()}};

			const auto* tag = world.GetRegistry().try_get<ECS::TagComponent>(entity);
			if (tag != nullptr) entityJson["components"]["Tag"] = SerializeTag(*tag);

			const auto* transform = world.GetRegistry().try_get<ECS::TransformComponent>(entity);
			if (transform != nullptr) entityJson["components"]["Transform"] = SerializeTransform(*transform);

			const auto* camera = world.GetRegistry().try_get<ECS::CameraComponent>(entity);
			if (camera != nullptr) entityJson["components"]["CameraComponent"] = SerializeCamera(*camera);

			const auto* cameraController = world.GetRegistry().try_get<ECS::CameraControllerComponent>(entity);
			if (cameraController != nullptr)
				entityJson["components"]["CameraControllerComponent"] = SerializeCameraController(*cameraController);

			const auto* hierarchy = world.GetRegistry().try_get<ECS::HierarchyComponent>(entity);
			if (hierarchy != nullptr)
				entityJson["components"]["HierarchyComponent"] = SerializeHierarchy(*hierarchy, sceneIdsByEntity);

			const auto* meshRenderer = world.GetRegistry().try_get<ECS::MeshRendererComponent>(entity);
			if (meshRenderer != nullptr)
				entityJson["components"]["MeshRenderer"] = SerializeMeshRenderer(*meshRenderer);

			const auto* movement = world.GetRegistry().try_get<ECS::MovementComponent>(entity);
			if (movement != nullptr) entityJson["components"]["MovementComponent"] = SerializeMovement(*movement);

			const auto* primitiveControl = world.GetRegistry().try_get<ECS::PrimitiveControlComponent>(entity);
			if (primitiveControl != nullptr)
				entityJson["components"]["PrimitiveControlComponent"] = SerializePrimitiveControl(*primitiveControl);

			sceneJson["entities"].push_back(std::move(entityJson));
		}

		return sceneJson;
	}

	void SceneSerializer::Deserialize(const nlohmann::json& sceneJson, ECS::World& world)
	{
		const int version = sceneJson.at("version").get<int>();
		// Keep loading strict so scene migrations are explicit instead of silently dropping component data.
		if (version != CurrentVersion)
			throw std::runtime_error("Unsupported scene version: " + std::to_string(version));

		world.GetRegistry().clear();
		std::unordered_map<uint32_t, ECS::Entity> entitiesBySceneId;

		for (const auto& entityJson : sceneJson.at("entities"))
		{
			const ECS::Entity entity = world.GetRegistry().create();
			const uint32_t sceneId = entityJson.at("id").get<uint32_t>();
			const auto [_, inserted] = entitiesBySceneId.emplace(sceneId, entity);
			if (!inserted) throw std::runtime_error("Duplicate scene entity id: " + std::to_string(sceneId));
		}

		for (const auto& entityJson : sceneJson.at("entities"))
		{
			const uint32_t sceneId = entityJson.at("id").get<uint32_t>();
			const auto entityIt = entitiesBySceneId.find(sceneId);
			if (entityIt == entitiesBySceneId.end())
				throw std::runtime_error("Missing ECS entity for scene entity id: " + std::to_string(sceneId));

			const ECS::Entity entity = entityIt->second;
			const auto& components = entityJson.at("components");

			if (components.contains("Tag")) DeserializeTag(components.at("Tag"), world, entity);
			if (components.contains("Transform")) DeserializeTransform(components.at("Transform"), world, entity);
			if (components.contains("CameraComponent"))
				DeserializeCamera(components.at("CameraComponent"), world, entity);
			if (components.contains("CameraControllerComponent"))
				DeserializeCameraController(components.at("CameraControllerComponent"), world, entity);
			if (components.contains("HierarchyComponent"))
				DeserializeHierarchy(components.at("HierarchyComponent"), world, entity, entitiesBySceneId);
			if (components.contains("MeshRenderer"))
				DeserializeMeshRenderer(components.at("MeshRenderer"), world, entity);
			if (components.contains("MovementComponent"))
				DeserializeMovement(components.at("MovementComponent"), world, entity);
			if (components.contains("PrimitiveControlComponent"))
				DeserializePrimitiveControl(components.at("PrimitiveControlComponent"), world, entity);
		}
	}

	void SceneSerializer::SaveToFile(const ECS::World& world, const std::filesystem::path& path)
	{
		std::ofstream file(path);
		if (!file.is_open()) throw std::runtime_error("Failed to open scene file for writing: " + path.string());

		file << Serialize(world).dump(4);
	}

	void SceneSerializer::LoadFromFile(const std::filesystem::path& path, ECS::World& world)
	{
		std::ifstream file(path);
		if (!file.is_open()) throw std::runtime_error("Failed to open scene file for reading: " + path.string());

		nlohmann::json sceneJson;
		file >> sceneJson;
		Deserialize(sceneJson, world);
	}

} // namespace NeneEngine
