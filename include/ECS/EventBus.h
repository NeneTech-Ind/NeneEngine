#pragma once

#include "Core/Delegate.h"
#include "Core/EASTLStdHash.h"

#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <typeindex>
#include <utility>

namespace NeneEngine::ECS
{

	class EventBus
	{
	  public:
		template <typename Event, typename Handler> DelegateHandle Subscribe(Handler&& handler)
		{
			Subscriber subscriber;
			subscriber.handle = DelegateHandleFactory::Create();
			subscriber.callback = [callback = std::forward<Handler>(handler)](const void* eventData)
			{ callback(*static_cast<const Event*>(eventData)); };

			auto& subscribers = m_subscribers[std::type_index(typeid(Event))];
			subscribers.push_back(std::move(subscriber));
			return subscribers.back().handle;
		}

		bool Unsubscribe(DelegateHandle handle)
		{
			if (!handle.IsValid()) return false;

			for (auto& entry : m_subscribers)
			{
				auto& subscribers = entry.second;
				const auto iterator =
				    eastl::find_if(subscribers.begin(), subscribers.end(),
				                   [handle](const Subscriber& subscriber) { return subscriber.handle == handle; });
				if (iterator == subscribers.end()) continue;

				subscribers.erase(iterator);
				return true;
			}

			return false;
		}

		template <typename Event> void Publish(const Event& event) const
		{
			const auto iterator = m_subscribers.find(std::type_index(typeid(Event)));
			if (iterator == m_subscribers.end()) return;

			for (const auto& subscriber : iterator->second)
			{
				subscriber.callback(&event);
			}
		}

		void Clear() { m_subscribers.clear(); }

	  private:
		struct Subscriber
		{
			DelegateHandle handle;
			eastl::function<void(const void*)> callback;
		};

		eastl::unordered_map<std::type_index, eastl::vector<Subscriber>> m_subscribers;
	};

} // namespace NeneEngine::ECS
