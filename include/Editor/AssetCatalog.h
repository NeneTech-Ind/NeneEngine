// AssetCatalog.h

#pragma once

#include <EASTL/vector.h>
#include <filesystem>
#include <string>

namespace NeneEngine
{
	enum class AssetKind
	{
		Model,
		Texture,
		Shader,
		Prefab
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
		[[nodiscard]] const eastl::vector<AssetEntry>& Entries() const { return m_entries; }
		[[nodiscard]] const std::string& Error() const { return m_error; }

	  private:
		eastl::vector<AssetEntry> m_entries;
		std::string m_error;
	};

	inline constexpr const char* ModelAssetPayload = "NENE_MODEL_ASSET";
	inline constexpr const char* TextureAssetPayload = "NENE_TEXTURE_ASSET";
	inline constexpr const char* ShaderAssetPayload = "NENE_SHADER_ASSET";
	inline constexpr const char* PrefabAssetPayload = "NENE_PREFAB_ASSET";
} // namespace NeneEngine
