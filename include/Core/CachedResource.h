// CachedResource.h

#pragma once

#include <utility>

namespace NeneEngine
{

	template <typename T> class CachedResource
	{
	  public:
		explicit CachedResource(T data) : m_data(std::move(data)) {}

		T& GetData() noexcept { return m_data; }
		const T& GetData() const noexcept { return m_data; }

	  private:
		T m_data;
	};

} // namespace NeneEngine
