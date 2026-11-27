#pragma once

#include "Core/EASTLStdHash.h"
#include "ECS/Entity.h"

#include <EASTL/unordered_map.h>
#include <filesystem>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <string>

namespace NeneEngine
{

	namespace ECS
	{
		struct ScriptComponent;
		class World;
	}

	class PrefabLibrary final
	{
	  public:
		explicit PrefabLibrary(ECS::World& world, std::filesystem::path directory = {});

		// Prefab names are UTF-8 paths relative to the prefab directory.

		[[nodiscard]] const nlohmann::json& Load(const std::string& name);
		const nlohmann::json& Reload(const std::string& name);
		void ClearCache();
		std::filesystem::path CreateFromEntity(const std::string& name, ECS::Entity root);
		void SaveScriptFields(const std::string& name, const ECS::ScriptComponent& script);
		[[nodiscard]] ECS::Entity Instantiate(const std::string& name, const glm::vec3& position = glm::vec3(0.0f),
		                                      const nlohmann::json& overrides = nlohmann::json::object());

	  private:
		[[nodiscard]] std::string NormalizeName(const std::string& name) const;
		[[nodiscard]] std::filesystem::path ResolvePath(const std::string& name) const;
		[[nodiscard]] nlohmann::json Read(const std::string& name) const;

		ECS::World& m_world;
		std::filesystem::path m_directory;
		eastl::unordered_map<std::string, nlohmann::json> m_prefabs;
	};

}
