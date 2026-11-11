// ModelSpawner.cpp

#include "Scene/Instantiation/ModelSpawner.h"

#include "Core/NeneLogger.h"
#include "Core/PathResolver.h"
#include "Core/ResourceLoadQueue.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/Events/SceneRestoredEvent.h"
#include "ECS/World.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Graphics/Runtime/SceneAssetResolver.h"
#include "Scene/Instantiation/ModelInstanceConfig.h"
#include "Scene/Instantiation/ModelSpawnManifest.h"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <glm/gtc/quaternion.hpp>
#include <sstream>
#include <stdexcept>

namespace NeneEngine
{
	namespace
	{
		constexpr size_t kSpawnBatchSize = 256;

		std::filesystem::path FindDiffuseTextureFromObjMaterial(const std::filesystem::path& objPath)
		{
			std::ifstream objFile(objPath);
			if (!objFile) return {};

			std::string line;
			std::filesystem::path materialPath;
			while (std::getline(objFile, line))
			{
				std::istringstream stream(line);
				std::string command;
				stream >> command;
				if (command != "mtllib") continue;

				std::string materialFile;
				std::getline(stream >> std::ws, materialFile);
				if (!materialFile.empty())
				{
					materialPath = objPath.parent_path() / materialFile;
					break;
				}
			}

			if (materialPath.empty()) return {};

			std::ifstream materialFile(materialPath);
			if (!materialFile) return {};

			while (std::getline(materialFile, line))
			{
				std::istringstream stream(line);
				std::string command;
				stream >> command;
				if (command != "map_Kd") continue;

				std::string textureFile;
				std::getline(stream >> std::ws, textureFile);
				if (!textureFile.empty()) return materialPath.parent_path() / textureFile;
			}

			return {};
		}

		std::filesystem::path ResolveOptionalAssetPath(const std::filesystem::path& manifestPath,
		                                               const std::filesystem::path& path)
		{
			if (path.empty()) return {};
			if (path.is_absolute()) return std::filesystem::exists(path) ? path : std::filesystem::path{};

			const std::filesystem::path manifestDirectory = manifestPath.parent_path();
			const std::filesystem::path relativeToManifest = manifestDirectory / path;
			if (std::filesystem::exists(relativeToManifest)) return relativeToManifest;
			if (std::filesystem::exists(path)) return path;

			if (const auto resolvedFromCurrent = ResolveFromAncestors(std::filesystem::current_path(), path);
			    !resolvedFromCurrent.empty())
				return resolvedFromCurrent;

			return ResolveFromAncestors(manifestDirectory, path);
		}

		struct PreparedModelEntity
		{
			std::string name;
			ECS::TransformComponent transform;
			ECS::MeshRendererComponent renderer;
		};

		struct PreparedModel
		{
			std::string name;
			ECS::TransformComponent rootTransform;
			bool hasRoot = false;
			eastl::vector<PreparedModelEntity> parts;
		};

		PreparedModel PrepareModel(const ModelSpawnEntryConfig& model, const std::filesystem::path& shaderPath,
		                           const std::filesystem::path& manifestPath)
		{
			const auto path = ResolveOptionalAssetPath(manifestPath, model.meshPath);
			if (path.empty()) throw std::runtime_error("Model mesh was not found: " + model.meshPath.string());
			const auto config =
			    LoadModelInstanceConfig(ResolveOptionalAssetPath(manifestPath, model.instanceConfigPath));
			const auto resource = ResourceManager::GetInstance().Load<eastl::vector<MeshPart>>(path.generic_string());
			if (!resource) throw std::runtime_error("Could not read model: " + path.string());

			PreparedModel prepared;
			prepared.name = model.entityName;
			prepared.hasRoot = model.splitByMeshParts;
			prepared.rootTransform.position = config.position;
			const auto& parts = resource->GetData();
			const size_t count = model.splitByMeshParts ? parts.size() : 1;
			for (size_t index = 0; index < count; ++index)
			{
				PreparedModelEntity entity;
				entity.name = model.splitByMeshParts ? parts[index].name : model.entityName;
				if (entity.name.empty()) entity.name = model.entityName + "_" + std::to_string(index);
				if (!model.splitByMeshParts) entity.transform.position = config.position;
				entity.transform.scale = config.scale;
				entity.renderer.meshPath = PortableAssetPath(path);
				entity.renderer.meshPart = model.splitByMeshParts ? static_cast<int>(index) : -1;
				entity.renderer.shaderPath = PortableAssetPath(shaderPath);
				auto texturePath =
				    model.splitByMeshParts ? parts[index].diffuseTexturePath : FindDiffuseTextureFromObjMaterial(path);
				if (model.splitByMeshParts)
				{
					if (const auto* part = FindPartOverride(config, parts[index].name))
					{
						entity.transform.position += part->positionOffset;
						entity.transform.scale *= part->scaleMultiplier;
						entity.transform.rotation = glm::quat(glm::radians(part->rotationOffsetDegrees));
						entity.renderer.visible = part->visible;
						if (!part->textureOverride.empty()) texturePath = path.parent_path() / part->textureOverride;
					}
				}
				entity.renderer.texturePath = PortableAssetPath(texturePath);
				prepared.parts.push_back(std::move(entity));
			}
			return prepared;
		}

		void RequestModel(ECS::World& world, ResourceLoadQueue& loads,
		                  const eastl::shared_ptr<std::atomic<bool>>& cancelled, ModelSpawnEntryConfig model,
		                  const std::filesystem::path& shaderPath, const std::filesystem::path& manifestPath)
		{
			const std::string name = model.entityName;
			loads.Request(
			    name,
			    [&world, &loads, cancelled, model = std::move(model), shaderPath,
			     manifestPath]() -> ResourceLoadQueue::Completion
			    {
				    if (*cancelled) return {};
				    auto prepared = eastl::make_shared<PreparedModel>(PrepareModel(model, shaderPath, manifestPath));
				    return [&world, &loads, cancelled, prepared]()
				    {
					    if (*cancelled) return;
					    ECS::Entity root = ECS::NullEntity;
					    if (prepared->hasRoot)
					    {
						    root = world.CreateEntity(prepared->name);
						    world.AddComponent<ECS::TransformComponent>(root) = prepared->rootTransform;
						    world.AddComponent<ECS::HierarchyComponent>(root);
					    }
					    loads.Continue(
					        [&world, cancelled, prepared, root, next = size_t{0}]() mutable
					        {
						        if (*cancelled) return true;
						        if (root != ECS::NullEntity && !world.GetRegistry().valid(root)) return true;
						        const size_t end = (std::min)(next + kSpawnBatchSize, prepared->parts.size());
						        for (; next < end; ++next)
						        {
							        const auto& source = prepared->parts[next];
							        const auto entity = world.CreateEntity(source.name);
							        world.AddComponent<ECS::TransformComponent>(entity) = source.transform;
							        world.AddComponent<ECS::MeshRendererComponent>(entity) = source.renderer;
							        if (root == ECS::NullEntity) continue;
							        world.AddComponent<ECS::HierarchyComponent>(entity).parent = root;
							        world.GetComponent<ECS::HierarchyComponent>(root)->children.push_back(entity);
						        }
						        return next == prepared->parts.size();
					        });
				    };
			    },
			    [name](const std::string& error) { NENE_LOG_ERROR("Model '{}' failed to load: {}", name, error); });
		}
	} // namespace

	void SpawnModelsFromManifest(ECS::World& world, ResourceLoadQueue& loads, const std::filesystem::path& shaderPath,
	                             const std::filesystem::path& manifestPath)
	{
		auto cancelled = eastl::make_shared<std::atomic<bool>>(false);
		world.GetEventBus().Subscribe<ECS::SceneRestoredEvent>([cancelled](const auto&) { *cancelled = true; });
		loads.Request(
		    "Model Manifest",
		    [&world, &loads, cancelled, shaderPath, manifestPath]() -> ResourceLoadQueue::Completion
		    {
			    auto manifest = LoadModelSpawnManifest(manifestPath);
			    return [&world, &loads, cancelled, shaderPath, manifestPath, manifest = std::move(manifest)]()
			    {
				    if (*cancelled) return;
				    for (const auto& model : manifest.models)
					    RequestModel(world, loads, cancelled, model, shaderPath, manifestPath);
			    };
		    },
		    [](const std::string& error) { NENE_LOG_ERROR("Model manifest failed: {}", error); });
	}
} // namespace NeneEngine
