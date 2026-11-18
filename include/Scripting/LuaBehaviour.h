#pragma once

#include "Scripting/ScriptBehaviour.h"
#include "Scripting/ScriptFieldSchema.h"

#include <glm/vec3.hpp>
#include <map>
#include <memory>
#include <sol/forward.hpp>
#include <string>
#include <string_view>

namespace NeneEngine
{

	class LuaBehaviour final : public ScriptBehaviour
	{
	  public:
		LuaBehaviour(const sol::table& scriptClass, std::string_view scriptPath, const ScriptFieldSchema& fieldSchema,
		             const std::map<std::string, ScriptFieldValue>& fields, const sol::object& entityApi);
		~LuaBehaviour() override;

		LuaBehaviour(const LuaBehaviour&) = delete;
		LuaBehaviour& operator=(const LuaBehaviour&) = delete;

		void OnStart() override;
		void OnUpdate(float deltaTime) override;
		void OnDestroy() override;
		void OnCollision(const sol::object& other, const glm::vec3& normal) override;
		void Reload(const sol::table& scriptClass, const ScriptFieldSchema& fieldSchema,
		            const std::map<std::string, ScriptFieldValue>& fields);

	  private:
		struct Impl;
		std::unique_ptr<Impl> m_impl;
	};

} // namespace NeneEngine
