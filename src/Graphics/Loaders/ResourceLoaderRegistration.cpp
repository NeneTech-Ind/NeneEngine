// ResourceLoaderRegistration.cpp

#include "Graphics/Loaders/ResourceLoaderRegistration.h"

#include "Core/ResourceManager.h"
#include "Graphics/Loaders/MeshLoader.h"
#include "Graphics/Loaders/ShaderLoader.h"
#include "Graphics/Loaders/TextureLoader.h"

namespace NeneEngine
{

	void RegisterGraphicsResourceLoaders(ResourceManager& resourceManager)
	{
		resourceManager.RegisterLoader<Mesh>([](const std::string& path) { return LoadMeshFromFile(path); });
		resourceManager.RegisterLoader<TextureResource>([](const std::string& path)
		                                                { return LoadTextureResourceFromFile(path); });
		resourceManager.RegisterLoader<ShaderProgramResource>([](const std::string& path)
		                                                      { return LoadShaderProgramResourceFromFile(path); });
	}

} // namespace NeneEngine
