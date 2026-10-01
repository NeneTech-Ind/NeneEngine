// Delegate.h

#pragma once

#include <EASTL/algorithm.h>
#include <EASTL/functional.h>
#include <EASTL/vector.h>
#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <type_traits>
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

	template <typename RetVal, typename... Args> class Delegate
	{
	  public:
		using Callback = eastl::function<RetVal(Args...)>;

		Delegate() = default;
		explicit Delegate(Callback callback) : m_callback(std::move(callback)) {}

		void Bind(Callback callback) { m_callback = std::move(callback); }

		template <typename T> void BindRaw(T* object, RetVal (T::*method)(Args...))
		{
			Bind([object, method](Args... args) -> RetVal { return (object->*method)(std::forward<Args>(args)...); });
		}

		template <typename T> void BindRaw(const T* object, RetVal (T::*method)(Args...) const)
		{
			Bind([object, method](Args... args) -> RetVal { return (object->*method)(std::forward<Args>(args)...); });
		}

		void Clear() { m_callback = nullptr; }

		[[nodiscard]] bool IsBound() const { return static_cast<bool>(m_callback); }

		RetVal Execute(Args... args) const
		{
			if (!m_callback) throw std::runtime_error("Delegate is not bound");

			return m_callback(std::forward<Args>(args)...);
		}

		RetVal ExecuteIfBound(Args... args) const
		{
			if (m_callback)
			{
				if constexpr (std::is_void_v<RetVal>)
				{
					m_callback(std::forward<Args>(args)...);
					return;
				}
				else
				{
					return m_callback(std::forward<Args>(args)...);
				}
			}

			if constexpr (!std::is_void_v<RetVal>) return RetVal{};
		}

	  private:
		Callback m_callback;
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
			m_listeners.push_back(std::move(listener));
			return m_listeners.back().handle;
		}

		template <typename Lambda> [[nodiscard]] DelegateHandle AddLambda(Lambda&& lambda)
		{
			return Add(Callback(std::forward<Lambda>(lambda)));
		}

		template <typename T> [[nodiscard]] DelegateHandle AddRaw(T* object, void (T::*method)(Args...))
		{
			return Add([object, method](Args... args) { (object->*method)(std::forward<Args>(args)...); });
		}

		template <typename T> [[nodiscard]] DelegateHandle AddRaw(const T* object, void (T::*method)(Args...) const)
		{
			return Add([object, method](Args... args) { (object->*method)(std::forward<Args>(args)...); });
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

			return false;
		}

		void RemoveAll()
		{
			if (IsBroadcasting())
			{
				for (auto& listener : m_listeners) listener.pendingRemove = true;
				return;
			}

			m_listeners.clear();
		}

		[[nodiscard]] bool IsBoundTo(DelegateHandle handle) const
		{
			if (!handle.IsValid()) return false;

			return eastl::any_of(m_listeners.begin(), m_listeners.end(), [handle](const Listener& listener)
			                     { return !listener.pendingRemove && listener.handle == handle; });
		}

		[[nodiscard]] size_t GetSize() const
		{
			return static_cast<size_t>(eastl::count_if(m_listeners.begin(), m_listeners.end(),
			                                           [](const Listener& listener)
			                                           { return !listener.pendingRemove; }));
		}

		void Broadcast(Args... args)
		{
			++m_broadcastDepth;
			const size_t listenerCount = m_listeners.size();

			// New listeners added during broadcast are handled by the next broadcast, keeping iteration stable.
			for (size_t index = 0; index < listenerCount; ++index)
			{
				auto& listener = m_listeners[index];
				if (!listener.pendingRemove && listener.callback) listener.callback(args...);
			}

			--m_broadcastDepth;

			if (!IsBroadcasting()) Compact();
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
		uint32_t m_broadcastDepth = 0;
	};

} // namespace NeneEngine
