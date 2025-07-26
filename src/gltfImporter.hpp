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
	void processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Vertex>& verticies);
	void processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<TexCoords>& texCoords);
	void processIndexAttrib(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<uint32_t>& indicies);
	void processNormalsAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Normals>& normals);
	// void processTangentAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive);
	ComPtr<ID3D11Device>& m_device;

};
