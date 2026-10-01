// DebugDrawSettings.h

#pragma once

namespace NeneEngine::ECS
{

	// Stored in the registry context so every window's RenderSystem sees the same toggle.
	struct DebugDrawSettings
	{
		bool drawColliders = false;
	};

} // namespace NeneEngine::ECS
