#pragma once

#include <glm/vec3.hpp>
#include <sol/forward.hpp>

namespace NeneEngine
{
	class ScriptBehaviour
	{
	  public:
		virtual ~ScriptBehaviour() = default;

		virtual void OnStart() = 0;
		virtual void OnUpdate(float deltaTime) = 0;
		virtual void OnDestroy() = 0;
		virtual void OnCollision(const sol::object& other, const glm::vec3& normal) = 0;
	};

} // namespace NeneEngine
