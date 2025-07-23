#pragma once

#include "tiny_gltf.h"
#include <string>
#include <vector>
#include "primitiveData.hpp"

#include "wrl.h"
#include "d3d11.h"

using namespace Microsoft::WRL;
class GLTFModel
{
public:
	std::string path;
	GLTFModel(std::string path, ComPtr<ID3D11Device>& device);
	~GLTFModel();

private:
	tinygltf::Model readGlb(const std::string& path);
	void processGlb(const tinygltf::Model& model);
	void processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive);
	void processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive);
	void processIndexAttrib(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive);
	// void processNormalsAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive);
	// void processTangentAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive);
	ComPtr<ID3D11Device>& m_device;
	std::vector<Vertex> m_posBuffer;
	std::vector<TexCoords> m_texCoordsBuffer;
	std::vector<Normals> m_normalBuffer;
	std::vector<Tangents> m_tangentBuffer;
	std::vector<uint32_t> m_indices;

};
