// Delegate.h

#pragma once

#include <EASTL/functional.h>
#include <EASTL/vector.h>
#include <atomic>
#include <cstdint>
#include <utility>

namespace NeneEngine
{

	class DelegateHandle
	{
	  public:
		constexpr DelegateHandle() noexcept = default;

		[[nodiscard]] bool IsValid() const noexcept { return m_id != 0; }
		void Reset() noexcept { m_id = 0; }

		[[nodiscard]] friend bool operator==(DelegateHandle lhs, DelegateHandle rhs) noexcept
		{
			return lhs.m_id == rhs.m_id;
		}

		[[nodiscard]] friend bool operator!=(DelegateHandle lhs, DelegateHandle rhs) noexcept { return !(lhs == rhs); }

	  private:
		friend class DelegateHandleFactory;
		constexpr explicit DelegateHandle(uint64_t id) noexcept : m_id(id) {}

		uint64_t m_id = 0;
	};

	class DelegateHandleFactory
	{
	  public:
		[[nodiscard]] static DelegateHandle Create()
		{
			static std::atomic_uint64_t nextId{1};
			return DelegateHandle(nextId.fetch_add(1, std::memory_order_relaxed));
		}
	};

	template <typename... Args> class MulticastDelegate
	{
	  public:
		using Callback = eastl::function<void(Args...)>;

		[[nodiscard]] DelegateHandle Add(Callback callback)
		{
			if (!callback) return {};

			Listener listener;
			listener.handle = DelegateHandleFactory::Create();
			listener.callback = std::move(callback);
			const DelegateHandle handle = listener.handle;
			if (IsBroadcasting())
				m_pendingListeners.push_back(std::move(listener));
			else
				m_listeners.push_back(std::move(listener));
			return handle;
		}

		template <typename Lambda> [[nodiscard]] DelegateHandle AddLambda(Lambda&& lambda)
		{
			return Add(Callback(std::forward<Lambda>(lambda)));
		}

		bool Remove(DelegateHandle handle)
		{
			if (!handle.IsValid()) return false;

			for (auto& listener : m_listeners)
			{
				if (listener.handle == handle)
				{
					// Removal can be requested from inside a callback, so compaction is deferred until broadcast ends.
					listener.pendingRemove = true;
					if (!IsBroadcasting()) Compact();
					return true;
				}
			}

			for (auto it = m_pendingListeners.begin(); it != m_pendingListeners.end(); ++it)
			{
				if (it->handle != handle) continue;
				m_pendingListeners.erase(it);
				return true;
			}
			return false;
		}

		void Broadcast(Args... args)
		{
			++m_broadcastDepth;
			const size_t listenerCount = m_listeners.size();

			// Pending listeners become active after the outermost broadcast finishes.
			for (size_t index = 0; index < listenerCount; ++index)
			{
				auto& listener = m_listeners[index];
				if (!listener.pendingRemove && listener.callback) listener.callback(args...);
			}

			--m_broadcastDepth;

			if (!IsBroadcasting())
			{
				Compact();
				for (auto& listener : m_pendingListeners) m_listeners.push_back(std::move(listener));
				m_pendingListeners.clear();
			}
		}

	  private:
		struct Listener
		{
			DelegateHandle handle;
			Callback callback;
			bool pendingRemove = false;
		};

		[[nodiscard]] bool IsBroadcasting() const { return m_broadcastDepth > 0; }

		void Compact()
		{
			eastl::erase_if(m_listeners, [](const Listener& listener) { return listener.pendingRemove; });
		}

		eastl::vector<Listener> m_listeners;
		eastl::vector<Listener> m_pendingListeners;
		uint32_t m_broadcastDepth = 0;
	};

} // namespace NeneEngine
