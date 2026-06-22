// TextureLoader.cpp

#include "Graphics/Loaders/TextureLoader.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace NeneEngine
{
	namespace
	{
		nlohmann::json ReadMetadata(const std::filesystem::path& metadataPath)
		{
			if (!std::filesystem::exists(metadataPath)) return {};

			std::ifstream file(metadataPath);
			if (!file) return {};

			return nlohmann::json::parse(file);
		}

		TextureFilterMode ReadFilterMode(const nlohmann::json& metadata)
		{
			const std::string filter = metadata.value("filter", "linear");
			return filter == "nearest" || filter == "point" ? TextureFilterMode::Nearest : TextureFilterMode::Linear;
		}

		TextureAddressMode ReadAddressMode(const nlohmann::json& metadata)
		{
			const std::string addressMode = metadata.value("addressMode", "wrap");
			return addressMode == "clamp" ? TextureAddressMode::Clamp : TextureAddressMode::Wrap;
		}
	} // namespace

	TextureResource LoadTextureResourceFromFile(const std::string& path)
	{
		if (!std::filesystem::exists(path)) throw std::runtime_error("Texture file does not exist: " + path);

		const std::filesystem::path texturePath{path};
		// The loader keeps CPU metadata only; backend adapters perform the actual GPU upload.
		const std::filesystem::path metadataPath =
		    texturePath.parent_path() / (texturePath.stem().string() + ".texture.json");
		const nlohmann::json metadata = ReadMetadata(metadataPath);
		return TextureResource{path, true, ReadFilterMode(metadata), ReadAddressMode(metadata)};
	}

} // namespace NeneEngine
