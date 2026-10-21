// ISystem.h

#pragma once

namespace NeneEngine::ECS
{

	class World;

	class ISystem
	{
	  public:
		virtual ~ISystem() = default;

		virtual void Update(World& world, float deltaTime) = 0;
	};

} // namespace NeneEngine::ECS
