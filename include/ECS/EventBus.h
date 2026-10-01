#pragma once

#include "Core/EASTLStdHash.h"

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
		template <typename Event, typename Handler> void Subscribe(Handler&& handler)
		{
			auto& subscribers = m_subscribers[std::type_index(typeid(Event))];
			subscribers.push_back(
			    [callback = std::forward<Handler>(handler)](const void* eventData) { callback(*static_cast<const Event*>(eventData)); });
		}

		template <typename Event> void Publish(const Event& event) const
		{
			const auto iterator = m_subscribers.find(std::type_index(typeid(Event)));
			if (iterator == m_subscribers.end()) return;

			for (const auto& subscriber : iterator->second)
			{
				subscriber(&event);
			}
		}

		void Clear() { m_subscribers.clear(); }

	  private:
		using UntypedSubscriber = eastl::function<void(const void*)>;

		eastl::unordered_map<std::type_index, eastl::vector<UntypedSubscriber>> m_subscribers;
	};

} // namespace NeneEngine::ECS
