#include "Scene/Prefab/PrefabLibrary.h"

#include "Core/PathResolver.h"
#include "Core/Profiler.h"
#include "ECS/Components/ScriptComponent.h"
#include "ECS/Components/TransformComponent.h"
#include "ECS/World.h"
#include "Scene/EntitySerializer.h"
#include "Scene/SceneSerializer.h"

#include <EASTL/optional.h>
#include <EASTL/unordered_map.h>
#include <EASTL/unordered_set.h>
#include <EASTL/vector.h>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace NeneEngine
{

	namespace
	{
		constexpr const char* kPrefabSuffix = ".prefab.json";
		enum class PrefabWriteMode
		{
			Create,
			Replace
		};

		nlohmann::json ReadDefinition(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file) throw std::runtime_error("Could not open prefab: " + GenericPathToUtf8(path));
			nlohmann::json definition;
			file >> definition;
			return definition;
		}

		void WriteDefinition(const std::filesystem::path& path, const std::string& contents, PrefabWriteMode mode)
		{
			wchar_t tempPath[MAX_PATH]{};
			if (GetTempFileNameW(path.parent_path().c_str(), L"NNE", 0, tempPath) == 0)
				throw std::runtime_error("Could not create a temporary file for the prefab");

			try
			{
				std::ofstream file(std::filesystem::path(tempPath), std::ios::binary | std::ios::trunc);
				file << contents;
				file.close();
				if (!file) throw std::runtime_error("Could not write the prefab");
				const DWORD flags =
				    MOVEFILE_WRITE_THROUGH | (mode == PrefabWriteMode::Replace ? MOVEFILE_REPLACE_EXISTING : 0);
				if (!MoveFileExW(tempPath, path.c_str(), flags))
				{
					const auto error = GetLastError();
					if (mode == PrefabWriteMode::Create &&
					    (error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS))
						throw std::runtime_error("Prefab already exists: " + GenericPathToUtf8(path.filename()));
					throw std::runtime_error("Could not save prefab: " + GenericPathToUtf8(path));
				}
			}
			catch (...)
			{
				DeleteFileW(tempPath);
				throw;
			}
		}

		uint32_t ReadId(const nlohmann::json& value)
		{
			if (!value.is_number_integer() || (!value.is_number_unsigned() && value.get<int64_t>() < 0) ||
			    value.get<uint64_t>() > (std::numeric_limits<uint32_t>::max)())
				throw std::runtime_error("Prefab entity IDs must be unsigned 32-bit integers");
			return value.get<uint32_t>();
		}

		void ValidateNumbers(const nlohmann::json& value)
		{
			if (value.is_number_float() && !std::isfinite(value.get<double>()))
				throw std::runtime_error("Prefab values must be finite");
			if (value.is_structured())
				for (const auto& child : value) ValidateNumbers(child);
		}

		uint32_t NormalizeHierarchy(nlohmann::json& entities, const nlohmann::json& definition)
		{
			struct Node
			{
				uint32_t id;
				eastl::optional<uint32_t> parent;
				bool hasParent = false;
				bool hasChildren = false;
				eastl::vector<uint32_t> children;
			};
			eastl::vector<Node> nodes;
			eastl::unordered_map<uint32_t, size_t> indices;
			for (auto& entity : entities)
			{
				const uint32_t id = ReadId(entity.at("id"));
				if (!indices.emplace(id, nodes.size()).second)
					throw std::runtime_error("Duplicate prefab entity id: " + std::to_string(id));
				Node node{id};
				const auto& components = entity.at("components");
				if (!components.is_object()) throw std::runtime_error("Prefab components must be an object");
				if (components.contains("HierarchyComponent"))
				{
					const auto& hierarchy = components.at("HierarchyComponent");
					if (!hierarchy.is_object()) throw std::runtime_error("Prefab hierarchy must be an object");
					node.hasParent = hierarchy.contains("parent");
					if (node.hasParent && !hierarchy.at("parent").is_null())
						node.parent = ReadId(hierarchy.at("parent"));
					node.hasChildren = hierarchy.contains("children");
					if (node.hasChildren)
					{
						if (!hierarchy.at("children").is_array())
							throw std::runtime_error("Prefab hierarchy children must be an array");
						eastl::unordered_set<uint32_t> unique;
						for (const auto& child : hierarchy.at("children"))
						{
							const auto childId = ReadId(child);
							if (!unique.insert(childId).second)
								throw std::runtime_error("Duplicate prefab hierarchy child");
							node.children.push_back(childId);
						}
					}
				}
				nodes.push_back(std::move(node));
			}

			for (const auto& node : nodes)
				for (uint32_t childId : node.children)
				{
					const auto childIt = indices.find(childId);
					if (childIt == indices.end()) throw std::runtime_error("Prefab child references an unknown entity");
					auto& child = nodes[childIt->second];
					if ((child.hasParent || child.parent) && child.parent != node.id)
						throw std::runtime_error("Prefab parent and child references disagree");
					child.parent = node.id;
				}

			eastl::vector<eastl::vector<uint32_t>> expectedChildren(nodes.size());
			eastl::optional<uint32_t> root;
			for (const auto& node : nodes)
			{
				if (!node.parent)
				{
					if (root) throw std::runtime_error("Prefab must have exactly one root entity");
					root = node.id;
					continue;
				}
				const auto parentIt = indices.find(*node.parent);
				if (parentIt == indices.end()) throw std::runtime_error("Prefab parent references an unknown entity");
				expectedChildren[parentIt->second].push_back(node.id);
			}
			if (!root) throw std::runtime_error("Prefab hierarchy has no root entity");
			if (definition.contains("root") && ReadId(definition.at("root")) != *root)
				throw std::runtime_error("Prefab root does not match the hierarchy");

			for (size_t index = 0; index < nodes.size(); ++index)
			{
				auto& node = nodes[index];
				if (node.hasChildren)
				{
					auto actual = node.children;
					auto expected = expectedChildren[index];
					std::sort(actual.begin(), actual.end());
					std::sort(expected.begin(), expected.end());
					if (actual != expected) throw std::runtime_error("Prefab hierarchy omits a child entity");
				}
				else
					node.children = std::move(expectedChildren[index]);
			}
			eastl::vector<uint32_t> pending{*root};
			eastl::unordered_set<uint32_t> visited;
			for (size_t next = 0; next < pending.size(); ++next)
			{
				const auto id = pending[next];
				if (!visited.insert(id).second) throw std::runtime_error("Prefab hierarchy contains a cycle");
				const auto& children = nodes[indices.at(id)].children;
				pending.insert(pending.end(), children.begin(), children.end());
			}
			if (visited.size() != nodes.size())
				throw std::runtime_error("Prefab hierarchy contains disconnected entities or a cycle");

			for (size_t index = 0; index < nodes.size(); ++index)
			{
				const auto& node = nodes[index];
				auto& components = entities[index]["components"];
				if (components.contains("HierarchyComponent") || node.parent || !node.children.empty())
				{
					auto children = nlohmann::json::array();
					for (uint32_t child : node.children) children.push_back(child);
					components["HierarchyComponent"] = {
					    {"parent", node.parent ? nlohmann::json(*node.parent) : nlohmann::json(nullptr)},
					    {"children", std::move(children)}};
				}
			}
			return *root;
		}

		nlohmann::json PreparePrefab(const nlohmann::json& definition)
		{
			if (!definition.is_object()) throw std::runtime_error("Prefab must be a JSON object");
			if (definition.contains("version") && (!definition.at("version").is_number_integer() ||
			                                       definition.at("version") != SceneSerializer::CurrentVersion))
				throw std::runtime_error("Unsupported prefab version");
			if (definition.contains("entities") == definition.contains("components"))
				throw std::runtime_error("Prefab must contain either entities or a single components object");
			auto entities = definition.contains("entities")
			                    ? definition.at("entities")
			                    : nlohmann::json::array({{{"id", definition.value("id", nlohmann::json(1))},
			                                              {"components", definition.at("components")}}});
			if (!entities.is_array() || entities.empty())
				throw std::runtime_error("Prefab entities must be a nonempty array");
			const auto root = NormalizeHierarchy(entities, definition);
			ECS::World parsed;
			EntityDeserializationMap targets;
			EntitySerializationMap ids;
			for (const auto& entity : entities)
			{
				const auto id = ReadId(entity.at("id"));
				const auto target = parsed.GetRegistry().create();
				targets.emplace(id, target);
				ids.emplace(target, id);
			}
			for (const auto& entity : entities) DeserializeEntity(entity, parsed, targets);
			for (auto& entity : entities)
			{
				auto normalized = SerializeEntity(parsed, targets.at(ReadId(entity.at("id"))), ids);
				for (const auto& [component, value] : entity.at("components").items())
					if (!normalized.at("components").contains(component))
						throw std::runtime_error("Unknown prefab component: " + component);
				ValidateNumbers(normalized);
				entity = std::move(normalized);
			}
			return {{"version", SceneSerializer::CurrentVersion}, {"root", root}, {"entities", std::move(entities)}};
		}
	}

	PrefabLibrary::PrefabLibrary(ECS::World& world, std::filesystem::path directory)
	    : m_world(world), m_directory(std::move(directory))
	{
		if (m_directory.empty())
		{
			m_directory = ResolveFromExecutionRoots("assets/prefabs", true);
			if (m_directory.empty()) m_directory = GetExecutableDirectory() / "assets/prefabs";
		}
		m_directory = std::filesystem::absolute(m_directory).lexically_normal();
	}

	std::string PrefabLibrary::NormalizeName(const std::string& name) const
	{
		if (name.empty() || name.find('\0') != std::string::npos)
			throw std::runtime_error("Invalid prefab name: " + name);
		const auto relative = PathFromUtf8(name);
		if (relative.has_root_path() || !relative.has_filename())
			throw std::runtime_error("Invalid prefab name: " + name);
		for (const auto& part : relative)
			if (part == "..") throw std::runtime_error("Prefab name must stay inside the prefab directory");
		auto key = GenericPathToUtf8(relative.lexically_normal());
		if (relative.filename() == "." || relative.filename() == kPrefabSuffix)
			throw std::runtime_error("Invalid prefab name: " + name);
		if (!key.ends_with(kPrefabSuffix)) key += kPrefabSuffix;
		return key;
	}

	std::filesystem::path PrefabLibrary::ResolvePath(const std::string& name) const
	{
		const auto root = std::filesystem::weakly_canonical(m_directory);
		const auto path = std::filesystem::canonical(root / PathFromUtf8(name));
		const auto relative = path.lexically_relative(root);
		if (relative.empty() || relative.has_root_path() || *relative.begin() == "..")
			throw std::runtime_error("Prefab path leaves the prefab directory");
		return path;
	}

	nlohmann::json PrefabLibrary::Read(const std::string& name) const
	{
		return PreparePrefab(ReadDefinition(ResolvePath(name)));
	}

	const nlohmann::json& PrefabLibrary::Load(const std::string& name)
	{
		const auto key = NormalizeName(name);
		if (const auto cached = m_prefabs.find(key); cached != m_prefabs.end()) return cached->second;
		return Reload(key);
	}

	const nlohmann::json& PrefabLibrary::Reload(const std::string& name)
	{
		const auto key = NormalizeName(name);
		try
		{
			auto definition = Read(key);
			return m_prefabs.insert_or_assign(key, std::move(definition)).first->second;
		}
		catch (const std::exception& exception)
		{
			throw std::runtime_error("Prefab '" + key + "': " + exception.what());
		}
	}

	void PrefabLibrary::ClearCache()
	{
		m_prefabs.clear();
	}

	std::filesystem::path PrefabLibrary::CreateFromEntity(const std::string& name, ECS::Entity root)
	{
		const auto key = NormalizeName(name);
		const auto relativePath = PathFromUtf8(key);
		for (const auto& part : relativePath)
		{
			const auto& component = part.native();
			if (component.find_first_of(L"<>:\"|?*") != std::wstring::npos || component.ends_with(L'.') ||
			    component.ends_with(L' '))
				throw std::runtime_error("Invalid prefab name: " + name);
		}
		const auto directory = std::filesystem::weakly_canonical(m_directory);
		const auto parent = std::filesystem::weakly_canonical(directory / relativePath.parent_path());
		const auto relative = parent.lexically_relative(directory);
		if (relative.empty() || relative.has_root_path() || *relative.begin() == "..")
			throw std::runtime_error("Prefab path leaves the prefab directory");

		auto hierarchy = SerializeEntityHierarchy(m_world, root);
		auto definition = PreparePrefab({{"version", SceneSerializer::CurrentVersion},
		                                 {"root", hierarchy.entities.front().at("id")},
		                                 {"entities", std::move(hierarchy.entities)}});
		const auto contents = definition.dump(4) + "\n";
		const auto createdPath = (relative / relativePath.filename()).lexically_normal();
		std::filesystem::create_directories(parent);
		WriteDefinition(parent / relativePath.filename(), contents, PrefabWriteMode::Create);
		m_prefabs.erase(key);
		return createdPath;
	}

	void PrefabLibrary::SaveScriptFields(const std::string& name, const ECS::ScriptComponent& script)
	{
		const auto key = NormalizeName(name);
		const auto path = ResolvePath(key);
		auto definition = ReadDefinition(path);
		const auto saved = SerializeScriptComponent(script);
		const auto scriptPath = std::filesystem::path(script.scriptPath).lexically_normal();

		nlohmann::json* target = nullptr;
		const auto findTarget = [&](nlohmann::json& entity)
		{
			auto& components = entity.at("components");
			const auto candidate = components.find("ScriptComponent");
			if (candidate == components.end()) return;
			const auto candidatePath = std::filesystem::path(candidate->at("scriptPath").get<std::string>());
			if (candidatePath.lexically_normal() != scriptPath) return;
			if (target != nullptr) throw std::runtime_error("The prefab has multiple entities with this script");
			target = &*candidate;
		};
		if (definition.contains("entities"))
			for (auto& entity : definition.at("entities")) findTarget(entity);
		else
			findTarget(definition);

		if (target == nullptr) throw std::runtime_error("The prefab does not contain this script");
		if (!target->contains("fields")) (*target)["fields"] = nlohmann::json::object();
		target->at("fields").update(saved.at("fields"));
		(void)PreparePrefab(definition);
		WriteDefinition(path, definition.dump(4) + "\n", PrefabWriteMode::Replace);
		ClearCache();
	}

	ECS::Entity PrefabLibrary::Instantiate(const std::string& name, const glm::vec3& position,
	                                       const nlohmann::json& overrides)
	{
		NENE_PROFILE_SCOPE("Prefab Instantiate");
		if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
			throw std::runtime_error("Prefab position must be finite");
		if (!overrides.is_object()) throw std::runtime_error("Prefab overrides must be a component object");
		const auto& cached = Load(name);
		nlohmann::json modified;
		const auto* definition = &cached;
		if (!overrides.empty())
		{
			modified = cached;
			const auto root = ReadId(modified.at("root"));
			for (auto& entity : modified["entities"])
				if (ReadId(entity.at("id")) == root) entity["components"].merge_patch(overrides);
			modified = PreparePrefab(modified);
			definition = &modified;
		}
		const auto& entities = definition->at("entities");
		EntityDeserializationMap targets;
		eastl::vector<ECS::Entity> created;
		created.reserve(entities.size());
		try
		{
			for (const auto& entity : entities)
			{
				const auto target = m_world.GetRegistry().create();
				created.push_back(target);
				targets.emplace(ReadId(entity.at("id")), target);
			}
			for (const auto& entity : entities) DeserializeEntity(entity, m_world, targets);
			const auto root = targets.at(ReadId(definition->at("root")));
			auto* transform = m_world.GetComponent<ECS::TransformComponent>(root);
			if (!transform) transform = &m_world.AddComponent<ECS::TransformComponent>(root);
			transform->position = position;
			return root;
		}
		catch (...)
		{
			for (auto it = created.rbegin(); it != created.rend(); ++it)
				if (m_world.GetRegistry().valid(*it)) m_world.GetRegistry().destroy(*it);
			throw;
		}
	}

}
