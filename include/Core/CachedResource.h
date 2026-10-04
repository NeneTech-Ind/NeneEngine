// CachedResource.h

#pragma once

#include <string>
#include <utility>

namespace NeneEngine
{

	template <typename T> class CachedResource
	{
	  public:
		CachedResource(std::string path, T data) : m_path(std::move(path)), m_data(std::move(data)) {}

		const std::string& GetPath() const noexcept { return m_path; }
		T& GetData() noexcept { return m_data; }
		const T& GetData() const noexcept { return m_data; }

	  private:
		std::string m_path;
		T m_data;
	};

} // namespace NeneEngine
