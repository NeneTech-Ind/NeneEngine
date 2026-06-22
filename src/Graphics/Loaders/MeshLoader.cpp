// MeshLoader.cpp

#include "Graphics/Loaders/MeshLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <filesystem>
#include <stdexcept>
#include <string>

namespace NeneEngine
{
	namespace
	{
		constexpr unsigned kMeshImportFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
		                                      aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals |
		                                      aiProcess_ImproveCacheLocality | aiProcess_ValidateDataStructure |
		                                      aiProcess_FlipUVs;
		constexpr unsigned kMeshPartsImportFlags = aiProcess_Triangulate | aiProcess_JoinIdenticalVertices |
		                                           aiProcess_PreTransformVertices | aiProcess_GenSmoothNormals |
		                                           aiProcess_ImproveCacheLocality | aiProcess_ValidateDataStructure;

		void AppendAssimpMesh(const aiMesh& mesh, MeshData& meshData)
		{
			const uint32_t baseVertex = static_cast<uint32_t>(meshData.vertices.size());
			meshData.vertices.reserve(meshData.vertices.size() + mesh.mNumVertices);

			for (unsigned vertexIndex = 0; vertexIndex < mesh.mNumVertices; ++vertexIndex)
			{
				Vertex vertex{};

				if (mesh.HasPositions())
				{
					const aiVector3D& position = mesh.mVertices[vertexIndex];
					vertex.position = {position.x, position.y, position.z};
				}

				if (mesh.HasNormals())
				{
					const aiVector3D& normal = mesh.mNormals[vertexIndex];
					vertex.normal = {normal.x, normal.y, normal.z};
				}

				if (mesh.HasTextureCoords(0))
				{
					const aiVector3D& uv = mesh.mTextureCoords[0][vertexIndex];
					vertex.uv = {uv.x, uv.y};
				}

				meshData.vertices.push_back(vertex);
			}

			for (unsigned faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex)
			{
				const aiFace& face = mesh.mFaces[faceIndex];
				for (unsigned index = 0; index < face.mNumIndices; ++index)
					meshData.indices.push_back(baseVertex + face.mIndices[index]);
			}
		}

		std::filesystem::path ReadDiffuseTexturePath(const aiScene& scene, const aiMesh& mesh,
		                                             const std::filesystem::path& modelPath)
		{
			if (mesh.mMaterialIndex >= scene.mNumMaterials) return {};

			const aiMaterial* material = scene.mMaterials[mesh.mMaterialIndex];
			if (material == nullptr) return {};

			aiString texturePath;
			if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS) return {};
			if (texturePath.length == 0) return {};

			return modelPath.parent_path() / std::filesystem::path{texturePath.C_Str()};
		}

		std::string ReadMeshPartName(const aiMesh& mesh, unsigned meshIndex)
		{
			if (mesh.mName.length > 0) return mesh.mName.C_Str();
			return "MeshPart" + std::to_string(meshIndex);
		}
	} // namespace

	MeshData LoadMeshDataFromFile(const std::string& path)
	{
		if (!std::filesystem::exists(path)) throw std::runtime_error("Mesh file does not exist: " + path);

		Assimp::Importer importer;
		// PreTransformVertices bakes Assimp node transforms into vertices for the current single-mesh draw path.
		const aiScene* scene = importer.ReadFile(path, kMeshImportFlags);

		if (scene == nullptr || scene->mRootNode == nullptr)
			throw std::runtime_error("Assimp failed to load mesh '" + path + "': " + importer.GetErrorString());

		if (scene->mNumMeshes == 0) throw std::runtime_error("Assimp loaded scene without meshes: " + path);

		MeshData meshData{};

		for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (mesh == nullptr) continue;
			AppendAssimpMesh(*mesh, meshData);
		}

		if (meshData.vertices.empty() || meshData.indices.empty())
			throw std::runtime_error("Mesh data is empty after loading: " + path);

		return meshData;
	}

	std::vector<MeshPart> LoadMeshPartsFromFile(const std::string& path)
	{
		const std::filesystem::path modelPath{path};
		if (!std::filesystem::exists(modelPath)) throw std::runtime_error("Mesh file does not exist: " + path);

		Assimp::Importer importer;
		const aiScene* scene = importer.ReadFile(path, kMeshImportFlags);

		if (scene == nullptr || scene->mRootNode == nullptr)
			throw std::runtime_error("Assimp failed to load mesh '" + path + "': " + importer.GetErrorString());

		if (scene->mNumMeshes == 0) throw std::runtime_error("Assimp loaded scene without meshes: " + path);

		std::vector<MeshPart> meshParts;
		meshParts.reserve(scene->mNumMeshes);

		for (unsigned meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex)
		{
			const aiMesh* mesh = scene->mMeshes[meshIndex];
			if (mesh == nullptr) continue;

			MeshPart meshPart{};
			meshPart.name = ReadMeshPartName(*mesh, meshIndex);
			meshPart.diffuseTexturePath = ReadDiffuseTexturePath(*scene, *mesh, modelPath);
			AppendAssimpMesh(*mesh, meshPart.data);

			if (!meshPart.data.vertices.empty() && !meshPart.data.indices.empty())
				meshParts.push_back(std::move(meshPart));
		}

		if (meshParts.empty()) throw std::runtime_error("Mesh parts are empty after loading: " + path);

		return meshParts;
	}

	Mesh LoadMeshFromFile(const std::string& path)
	{
		Mesh mesh{};
		mesh.data = LoadMeshDataFromFile(path);
		return mesh;
	}

} // namespace NeneEngine
