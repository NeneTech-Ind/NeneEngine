#pragma once

#include "ECS/Systems/ISystem.h"

#include <EASTL/unique_ptr.h>

namespace NeneEngine
{
	class ScriptEngine;
}

namespace NeneEngine::ECS
{

	class ScriptSystem final : public ISystem
	{
	  public:
		ScriptSystem(ScriptEngine& scriptEngine, World& world);
		~ScriptSystem() override;

		ScriptSystem(const ScriptSystem&) = delete;
		ScriptSystem& operator=(const ScriptSystem&) = delete;

		void Update(World& world, float deltaTime) override;
		void OnSceneRestored(World& world) override;
		void OnPlayStopping(World& world) override;
		void OnEntityDestroying(World& world, Entity entity) override;

	  private:
		struct Impl;
		eastl::unique_ptr<Impl> m_impl;
	};

} // namespace NeneEngine
