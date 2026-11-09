// EditorAssetActions.cpp

#include "Editor/EditorAssetActions.h"

#include "Core/NeneLogger.h"
#include "Core/ResourceManager.h"
#include "ECS/Components/HierarchyComponent.h"
#include "ECS/Components/MeshRendererComponent.h"
#include "ECS/Components/TagComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "Editor/EditorContext.h"
#include "Editor/EditorHistory.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Scene/Instantiation/ModelInstanceConfig.h"

#include <vector>

namespace NeneEngine
{
	namespace
	{
		struct CreatedEntity
		{
			ECS::Entity entity;
			ECS::TagComponent tag;
			ECS::TransformComponent transform;
			ECS::MeshRendererComponent mesh;
			ECS::HierarchyComponent hierarchy;
			bool hasMesh = true;
		};

		class CreateAssetCommand final : public IEditorCommand
		{
		  public:
			explicit CreateAssetCommand(eastl::shared_ptr<std::vector<CreatedEntity>> entities)
			    : m_entities(std::move(entities))
			{
			}
			const char* GetName() const override { return "Create Asset"; }
			void Execute(ECS::World& world) override
			{
				for (const auto& entry : *m_entities)
				{
					if (!world.GetRegistry().valid(entry.entity))
					{
						const auto recreated = world.GetRegistry().create(entry.entity);
						if (recreated != entry.entity)
						{
							world.GetRegistry().destroy(recreated);
							return;
						}
					}
					world.GetRegistry().emplace_or_replace<ECS::TagComponent>(entry.entity, entry.tag);
					world.GetRegistry().emplace_or_replace<ECS::TransformComponent>(entry.entity, entry.transform);
					world.GetRegistry().emplace_or_replace<ECS::HierarchyComponent>(entry.entity, entry.hierarchy);
					if (entry.hasMesh)
						world.GetRegistry().emplace_or_replace<ECS::MeshRendererComponent>(entry.entity, entry.mesh);
				}
			}
			void Undo(ECS::World& world) override
			{
				for (auto it = m_entities->rbegin(); it != m_entities->rend(); ++it)
					if (world.GetRegistry().valid(it->entity)) world.DestroyEntity(it->entity);
			}

		  private:
			eastl::shared_ptr<std::vector<CreatedEntity>> m_entities;
		};
	} // namespace

	bool CreateSceneAsset(EditorContext& context, const std::string& path, bool isTexture, const glm::vec3& position)
	{
		if (context.mode != EditorMode::Edit || !context.renderer || !context.resourceLoads) return false;
		const auto root = context.world.CreateEntity(std::filesystem::path(path).stem().string());
		auto created = eastl::make_shared<std::vector<CreatedEntity>>();
		CreatedEntity rootEntry{root, ECS::TagComponent(std::filesystem::path(path).stem().string()), {}, {}, {}, true};
		rootEntry.transform.position = position;
		rootEntry.mesh.meshPath = isTexture ? "@quad" : "@cube";
		if (isTexture)
		{
			rootEntry.mesh.texturePath = path;
			rootEntry.mesh.shaderPath = "assets/shaders/textured_mesh.shader";
		}
		created->push_back(rootEntry);
		context.history.Execute(eastl::make_unique<CreateAssetCommand>(created), context.world);
		context.selectedEntity = root;
		if (isTexture)
		{
			context.assetResolver.Texture(path, *context.renderer);
			context.SetStatus(EditorStatusKind::Muted, "Texture added");
			return true;
		}

		const auto generation = context.sceneGeneration;
		context.SetStatus(EditorStatusKind::Muted, "Loading model...");
		const auto fail = [&context, generation, root](const std::string& error)
		{
			if (context.sceneGeneration != generation) return;
			if (auto* mesh = context.world.GetComponent<ECS::MeshRendererComponent>(root))
				mesh->tint = {1.0f, 0.2f, 0.2f, 1.0f};
			context.SetStatus(EditorStatusKind::Danger, error);
		};
		const bool accepted = context.resourceLoads->Request(
		    path,
		    [&context, generation, root, created, path]() -> ResourceLoadQueue::Completion
		    {
			    const auto resolved = ResolveAssetPath(path);
			    const auto resource = ResourceManager::GetInstance().Load<eastl::vector<MeshPart>>(
			        resolved.lexically_normal().generic_string());
			    if (!resource) throw std::runtime_error("Could not load model: " + path);
			    const auto config = LoadModelInstanceConfig(resolved.parent_path() / "model.instance.json");
			    return [&context, generation, root, created, path, resolved, resource, config]()
			    {
				    if (context.sceneGeneration != generation) return;
				    auto apply = [&context, generation, root, created, path, resolved, resource, config]()
				    {
					    if (context.sceneGeneration != generation) return;
					    auto rootEntry = created->front();
					    rootEntry.hasMesh = false;
					    std::vector<CreatedEntity> entries{rootEntry};
					    const auto& parts = resource->GetData();
					    // Keep the root index reserved while assigning stable child ids for redo.
					    const bool exists = context.world.GetRegistry().valid(root);
					    if (!exists)
					    {
						    const auto reserved = context.world.GetRegistry().create(root);
						    if (reserved != root)
						    {
							    context.world.DestroyEntity(reserved);
							    return;
						    }
					    }
					    for (size_t index = 0; index < parts.size(); ++index)
					    {
						    const auto child = context.world.CreateEntity(parts[index].name);
						    CreatedEntity entry{child, ECS::TagComponent(parts[index].name), {}, {}, {}, true};
						    entry.hierarchy.parent = root;
						    entry.transform.scale = config.scale;
						    entry.mesh.meshPath = path;
						    entry.mesh.meshPart = static_cast<int>(index);
						    entry.mesh.texturePath = PortableAssetPath(parts[index].diffuseTexturePath);
						    entry.mesh.shaderPath = "assets/shaders/textured_mesh.shader";
						    if (const auto* part = FindPartOverride(config, parts[index].name))
						    {
							    entry.transform.position = part->positionOffset;
							    entry.transform.scale *= part->scaleMultiplier;
							    entry.transform.rotation = glm::quat(glm::radians(part->rotationOffsetDegrees));
							    entry.mesh.visible = part->visible;
							    if (!part->textureOverride.empty())
								    entry.mesh.texturePath =
								        PortableAssetPath(resolved.parent_path() / part->textureOverride);
						    }
						    entries.front().hierarchy.children.push_back(child);
						    entries.push_back(std::move(entry));
					    }
					    *created = std::move(entries);
					    if (exists)
					    {
						    if (!context.history.IsDirty()) context.history.InvalidateSavedState();
						    const auto transform = *context.world.GetComponent<ECS::TransformComponent>(root);
						    const auto tag = *context.world.GetComponent<ECS::TagComponent>(root);
						    CreateAssetCommand(created).Execute(context.world);
						    context.world.GetRegistry().remove<ECS::MeshRendererComponent>(root);
						    *context.world.GetComponent<ECS::TransformComponent>(root) = transform;
						    *context.world.GetComponent<ECS::TagComponent>(root) = tag;
						    context.SetStatus(EditorStatusKind::Success, "Created: " + resolved.filename().string());
					    }
					    else
					    {
						    for (size_t index = 1; index < created->size(); ++index)
							    context.world.DestroyEntity((*created)[index].entity);
						    context.world.DestroyEntity(root);
					    }
				    };
				    if (context.mode == EditorMode::Edit)
					    apply();
				    else
					    context.pendingAssets.push_back(std::move(apply));
			    };
		    },
		    fail);
		if (!accepted) fail("Resource loader is stopping");
		return accepted;
	}
} // namespace NeneEngine
