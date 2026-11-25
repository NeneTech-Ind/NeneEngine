// VisibilityRegistry.cpp

#include "ECS/VisibilityRegistry.h"

namespace NeneEngine::ECS
{

	bool VisibilityRegistry::Register(Entity entity, std::optional<WorldAabb> bounds)
	{
		if (entity == NullEntity) return false;
		return m_boundsByEntity.insert({entity, bounds}).second;
	}

	bool VisibilityRegistry::UpdateBounds(Entity entity, std::optional<WorldAabb> bounds)
	{
		const auto entry = m_boundsByEntity.find(entity);
		if (entry == m_boundsByEntity.end()) return false;
		entry->second = bounds;
		return true;
	}

	bool VisibilityRegistry::Remove(Entity entity)
	{
		return m_boundsByEntity.erase(entity) != 0;
	}

	void VisibilityRegistry::Clear()
	{
		m_boundsByEntity.clear();
	}

	const std::optional<WorldAabb>* VisibilityRegistry::FindBounds(Entity entity) const
	{
		const auto entry = m_boundsByEntity.find(entity);
		return entry != m_boundsByEntity.end() ? &entry->second : nullptr;
	}

} // namespace NeneEngine::ECS
