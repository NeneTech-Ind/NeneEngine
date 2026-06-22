// ShaderLoader.cpp

#include "Graphics/Loaders/ShaderLoader.h"

#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>

namespace NeneEngine
{
	namespace
	{
		std::string ReadTextFile(const std::filesystem::path& path)
		{
			std::ifstream file(path, std::ios::binary);
			if (!file) throw std::runtime_error("Shader file does not exist: " + path.string());

			std::ostringstream stream;
			stream << file.rdbuf();
			return stream.str();
		}

		std::filesystem::path ResolveRelativeTo(const std::filesystem::path& ownerPath, const std::string& path)
		{
			const std::filesystem::path value{path};
			if (value.is_absolute()) return value;

			return ownerPath.parent_path() / value;
		}
	} // namespace

	ShaderProgramResource LoadShaderProgramResourceFromFile(const std::string& path)
	{
		const std::filesystem::path programPath{path};
		if (!std::filesystem::exists(programPath))
			throw std::runtime_error("Shader program file does not exist: " + path);

		const nlohmann::json description = nlohmann::json::parse(ReadTextFile(programPath));
		// Shader descriptors are portable: source paths are resolved next to the .shader file.
		const std::filesystem::path vertexPath =
		    ResolveRelativeTo(programPath, description.at("vertex").get<std::string>());
		const std::filesystem::path pixelPath =
		    ResolveRelativeTo(programPath, description.at("pixel").get<std::string>());

		ShaderProgramResource resource{};
		resource.vertexPath = vertexPath.string();
		resource.pixelPath = pixelPath.string();
		resource.vertexSource = ReadTextFile(vertexPath);
		resource.pixelSource = ReadTextFile(pixelPath);
		return resource;
	}

} // namespace NeneEngine
