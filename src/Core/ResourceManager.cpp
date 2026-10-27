// ResourceManager.cpp

#include "Core/ResourceManager.h"

namespace NeneEngine
{

	ResourceManager& ResourceManager::GetInstance()
	{
		static ResourceManager instance;
		return instance;
	}

} // namespace NeneEngine
