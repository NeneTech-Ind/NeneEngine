#pragma once

#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace NeneEngine
{

	class ScriptExecutionContext
	{
	  public:
		class LoadScope
		{
		  public:
			explicit LoadScope(ScriptExecutionContext& context) noexcept
			    : m_context(context), m_wasLoading(std::exchange(context.m_loading, true))
			{
			}

			~LoadScope() { m_context.m_loading = m_wasLoading; }

			LoadScope(const LoadScope&) = delete;
			LoadScope& operator=(const LoadScope&) = delete;

		  private:
			ScriptExecutionContext& m_context;
			bool m_wasLoading;
		};

		void RequireWorldMutationAllowed(std::string_view operation) const
		{
			if (m_loading)
				throw std::runtime_error(std::string(operation) + " cannot change the world during script loading");
		}

	  private:
		bool m_loading = false;
	};

} // namespace NeneEngine
