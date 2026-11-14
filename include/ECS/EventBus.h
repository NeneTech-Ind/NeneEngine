#pragma once

#include "Core/Delegate.h"
#include "Core/EASTLStdHash.h"

#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>
#include <cstdint>
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

			const DelegateHandle handle = subscriber.handle;
			const std::type_index type(typeid(Event));
			if (IsPublishing())
				m_pendingSubscribers.push_back({type, std::move(subscriber)});
			else
				m_subscribers[type].push_back(std::move(subscriber));
			return handle;
		}

		bool Unsubscribe(DelegateHandle handle)
		{
			if (!handle.IsValid()) return false;

			for (auto& entry : m_subscribers)
			{
				for (auto& subscriber : entry.second)
				{
					if (subscriber.handle != handle || subscriber.pendingRemove) continue;

					subscriber.pendingRemove = true;
					if (!IsPublishing()) RemovePendingSubscribers();
					return true;
				}
			}

			const auto pending =
			    eastl::find_if(m_pendingSubscribers.begin(), m_pendingSubscribers.end(),
			                   [handle](const PendingSubscriber& entry) { return entry.subscriber.handle == handle; });
			if (pending == m_pendingSubscribers.end()) return false;

			m_pendingSubscribers.erase(pending);
			return true;
		}

		template <typename Event> void Publish(const Event& event)
		{
			const auto iterator = m_subscribers.find(std::type_index(typeid(Event)));
			if (iterator == m_subscribers.end()) return;

			++m_publishDepth;
			for (const auto& subscriber : iterator->second)
			{
				if (!subscriber.pendingRemove) subscriber.callback(&event);
			}
			--m_publishDepth;

			if (!IsPublishing())
			{
				AddPendingSubscribers();
				RemovePendingSubscribers();
			}
		}

		void Clear()
		{
			m_subscribers.clear();
			m_pendingSubscribers.clear();
		}

	  private:
		struct Subscriber
		{
			DelegateHandle handle;
			eastl::function<void(const void*)> callback;
			bool pendingRemove = false;
		};

		struct PendingSubscriber
		{
			std::type_index type;
			Subscriber subscriber;
		};

		[[nodiscard]] bool IsPublishing() const { return m_publishDepth > 0; }

		void AddPendingSubscribers()
		{
			for (auto& pending : m_pendingSubscribers)
				m_subscribers[pending.type].push_back(std::move(pending.subscriber));
			m_pendingSubscribers.clear();
		}

		void RemovePendingSubscribers()
		{
			for (auto& entry : m_subscribers)
				eastl::erase_if(entry.second, [](const Subscriber& subscriber) { return subscriber.pendingRemove; });
		}

		eastl::unordered_map<std::type_index, eastl::vector<Subscriber>> m_subscribers;
		eastl::vector<PendingSubscriber> m_pendingSubscribers;
		uint32_t m_publishDepth = 0;
	};

} // namespace NeneEngine::ECS
