// RenderTypes.h

#pragma once

#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace NeneEngine
{

	struct MeshId
	{
		uint32_t value = 0;

		constexpr bool IsValid() const noexcept { return value != 0; }
		auto operator<=>(const MeshId&) const = default;
	};

	struct MaterialId
	{
		uint32_t value = 0;

		constexpr bool IsValid() const noexcept { return value != 0; }
		auto operator<=>(const MaterialId&) const = default;
	};

	struct TextureId
	{
		uint32_t value = 0;

		constexpr bool IsValid() const noexcept { return value != 0; }
		auto operator<=>(const TextureId&) const = default;
	};

	struct ShaderId
	{
		uint32_t value = 0;

		constexpr bool IsValid() const noexcept { return value != 0; }
		auto operator<=>(const ShaderId&) const = default;
	};

	struct GPUBufferId
	{
		uint32_t value = 0;

		constexpr bool IsValid() const noexcept { return value != 0; }
		auto operator<=>(const GPUBufferId&) const = default;
	};

	enum class PrimitiveType : uint8_t
	{
		Line,
		Triangle,
		Quad,
		Cube
	};

	struct Vertex
	{
		glm::vec3 position = {0.0f, 0.0f, 0.0f};
		glm::vec3 normal = {0.0f, 0.0f, 1.0f};
		glm::vec2 uv = {0.0f, 0.0f};
	};

	struct MeshData
	{
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
	};

	struct GPUMesh
	{
		MeshId meshId{};
		GPUBufferId vertexBufferId{};
		GPUBufferId indexBufferId{};
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;

		constexpr bool IsValid() const noexcept { return meshId.IsValid(); }
	};

	struct GPUBuffer
	{
		GPUBufferId bufferId{};
		uint64_t sizeBytes = 0;
		uint32_t elementCount = 0;

		constexpr bool IsValid() const noexcept { return bufferId.IsValid(); }
	};

	enum class TextureFilterMode : uint8_t
	{
		Linear,
		Nearest
	};

	enum class TextureAddressMode : uint8_t
	{
		Wrap,
		Clamp
	};

	struct TextureResource
	{
		std::string path;
		bool isSrgb = true;
		TextureFilterMode filterMode = TextureFilterMode::Linear;
		TextureAddressMode addressMode = TextureAddressMode::Wrap;
	};

	struct GPUTexture
	{
		TextureId textureId{};
		uint32_t width = 0;
		uint32_t height = 0;

		constexpr bool IsValid() const noexcept { return textureId.IsValid(); }
	};

	struct ShaderProgramResource
	{
		std::string vertexPath;
		std::string pixelPath;
		std::string vertexSource;
		std::string pixelSource;
	};

	struct GPUShaderProgram
	{
		ShaderId shaderId{};

		constexpr bool IsValid() const noexcept { return shaderId.IsValid(); }
	};

	struct Mesh
	{
		MeshData data;
		std::optional<GPUMesh> gpuMesh;
	};

	struct Material
	{
		MaterialId materialId{};
		ShaderId shaderId{};
		TextureId textureId{};
		glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
	};

	struct RenderItem
	{
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		glm::mat4 viewMatrix = glm::mat4(1.0f);
		glm::mat4 projectionMatrix = glm::mat4(1.0f);
		glm::mat4 viewProjectionMatrix = glm::mat4(1.0f);
		glm::mat4 modelViewProjectionMatrix = glm::mat4(1.0f);
		PrimitiveType primitiveType = PrimitiveType::Triangle;
		MeshId meshId{};
		MaterialId materialId{};
		ShaderId shaderId{};
		TextureId textureId{};
		glm::vec4 tint = {1.0f, 1.0f, 1.0f, 1.0f};
	};

} // namespace NeneEngine
