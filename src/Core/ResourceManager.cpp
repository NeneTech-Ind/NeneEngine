// ResourceManager.cpp

#include "Core/ResourceManager.h"

namespace NeneEngine
{

	ResourceManager& ResourceManager::GetInstance()
	{
		static ResourceManager instance;
		return instance;
	}

	void ResourceManager::Clear()
	{
		std::scoped_lock lock(m_mutex);
		m_resourceCaches.clear();
		NENE_LOG_INFO("ResourceManager: cleared all cached resources");
	}

} // namespace NeneEngine
