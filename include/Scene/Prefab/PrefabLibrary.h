#pragma once

#include "ECS/Entity.h"

#include <filesystem>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace NeneEngine
{

	namespace ECS
	{
		class World;
	}

	class PrefabLibrary final
	{
	  public:
		explicit PrefabLibrary(ECS::World& world, std::filesystem::path directory = {});

		[[nodiscard]] const nlohmann::json& Load(const std::string& name);
		const nlohmann::json& Reload(const std::string& name);
		void ClearCache();
		[[nodiscard]] ECS::Entity Instantiate(const std::string& name, const glm::vec3& position = glm::vec3(0.0f),
		                                      const nlohmann::json& overrides = nlohmann::json::object());

	  private:
		[[nodiscard]] std::string NormalizeName(const std::string& name) const;
		[[nodiscard]] nlohmann::json Read(const std::string& name) const;

		ECS::World& m_world;
		std::filesystem::path m_directory;
		std::unordered_map<std::string, nlohmann::json> m_prefabs;
	};

}
