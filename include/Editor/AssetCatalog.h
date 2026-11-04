// AssetCatalog.h

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace NeneEngine
{
	enum class AssetKind
	{
		Model,
		Texture,
		Shader
	};

	struct AssetEntry
	{
		std::string path;
		AssetKind kind;
	};

	class AssetCatalog final
	{
	  public:
		void Refresh();
		[[nodiscard]] const std::vector<AssetEntry>& Entries() const { return m_entries; }
		[[nodiscard]] const std::string& Error() const { return m_error; }

	  private:
		std::vector<AssetEntry> m_entries;
		std::string m_error;
	};

	inline constexpr const char* ModelAssetPayload = "NENE_MODEL_ASSET";
	inline constexpr const char* TextureAssetPayload = "NENE_TEXTURE_ASSET";
	inline constexpr const char* ShaderAssetPayload = "NENE_SHADER_ASSET";
} // namespace NeneEngine
