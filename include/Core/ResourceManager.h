// ResourceManager.h

#pragma once

#include "Core/CachedResource.h"
#include "Core/EASTLStdHash.h"
#include "Core/NeneLogger.h"

#include <EASTL/any.h>
#include <EASTL/functional.h>
#include <EASTL/shared_ptr.h>
#include <EASTL/unordered_map.h>
#include <mutex>
#include <string>
#include <typeindex>

namespace NeneEngine
{

	class ResourceManager final
	{
	  public:
		template <typename T> using ResourcePtr = eastl::shared_ptr<CachedResource<T>>;

		template <typename T> using LoaderFn = eastl::function<T(const std::string&)>;

		static ResourceManager& GetInstance();

		template <typename T> void RegisterLoader(LoaderFn<T> loader)
		{
			std::scoped_lock lock(m_mutex);
			m_loaders[std::type_index(typeid(T))] = std::move(loader);
			NENE_LOG_INFO("ResourceManager: registered loader for type '{}'", typeid(T).name());
		}

		template <typename T> ResourcePtr<T> Load(const std::string& path)
		{
			std::scoped_lock lock(m_mutex);

			auto& cache = GetOrCreateCache<T>();
			if (const auto cached = cache.find(path); cached != cache.end())
			{
				NENE_LOG_DEBUG("ResourceManager: cache hit for '{}' ({})", path, typeid(T).name());
				return cached->second;
			}

			LoaderFn<T>* loader = GetLoader<T>();
			if (loader == nullptr)
			{
				NENE_LOG_ERROR("ResourceManager: no loader registered for '{}' ({})", path, typeid(T).name());
				return nullptr;
			}

			try
			{
				auto resource = eastl::make_shared<CachedResource<T>>(path, (*loader)(path));
				cache.emplace(path, resource);
				NENE_LOG_INFO("ResourceManager: loaded '{}' ({})", path, typeid(T).name());
				return resource;
			}
			catch (const std::exception& exception)
			{
				NENE_LOG_ERROR("ResourceManager: failed to load '{}' ({}): {}", path, typeid(T).name(),
				               exception.what());
			}
			catch (...)
			{
				NENE_LOG_ERROR("ResourceManager: failed to load '{}' ({}) with unknown error", path, typeid(T).name());
			}

			return nullptr;
		}

		void RegisterDefaultLoaders();
		void Clear();

	  private:
		ResourceManager() = default;

		template <typename T> using CacheMap = eastl::unordered_map<std::string, ResourcePtr<T>>;

		template <typename T> CacheMap<T>& GetOrCreateCache()
		{
			const std::type_index type = std::type_index(typeid(T));
			auto cacheIt = m_resourceCaches.find(type);
			if (cacheIt == m_resourceCaches.end()) cacheIt = m_resourceCaches.emplace(type, CacheMap<T>{}).first;

			return *eastl::any_cast<CacheMap<T>>(&cacheIt->second);
		}

		template <typename T> LoaderFn<T>* GetLoader()
		{
			const auto loaderIt = m_loaders.find(std::type_index(typeid(T)));
			if (loaderIt == m_loaders.end()) return nullptr;

			return eastl::any_cast<LoaderFn<T>>(&loaderIt->second);
		}

		std::mutex m_mutex;
		// Type-erased registries keep the public API templated while storing one typed loader/cache per resource type.
		eastl::unordered_map<std::type_index, eastl::any> m_loaders;
		eastl::unordered_map<std::type_index, eastl::any> m_resourceCaches;
	};

} // namespace NeneEngine
