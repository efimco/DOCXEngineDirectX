#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "gltfImporter.hpp"
#include <iostream>
#include "primitive.hpp"
#include "sceneManager.hpp"
GLTFModel::GLTFModel(std::string path, ComPtr<ID3D11Device>& device) : m_device(device)
{
	const tinygltf::Model model = readGlb(path);
	processGlb(model);
}

GLTFModel::~GLTFModel()
{
}

tinygltf::Model GLTFModel::readGlb(const std::string& path)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err;
	std::string warn;
	printf("Loading...%s\n ", path.c_str());
	bool ret = false;
	if (path.ends_with("gltf"))
	{
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
	}
	else
	{
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
	}

	if (!warn.empty())
		printf("Warn: %s\n", warn.c_str());
	if (!err.empty())
		printf("Err: %s\n", err.c_str());
	if (!ret)
		printf("Failed to parse glTF\n");
	printf("Loaded %s\n", path.c_str());
	return model;
}

void GLTFModel::processGlb(const tinygltf::Model& model)
{
	for (const auto mesh : model.meshes)
	{
		for (const auto gltfPrimitive : mesh.primitives)
		{
			processPosAttribute(model, mesh, gltfPrimitive);
			processTexCoordAttribute(model, mesh, gltfPrimitive);
			processIndexAttrib(model, mesh, gltfPrimitive);
			std::vector<InterleavedData> vertexData;
			const auto numVert = m_posBuffer.size();
			for (int i = 0; i < numVert; i++)
			{
				InterleavedData interData;
				interData.vertex = m_posBuffer[i];
				interData.texCoords = m_texCoordsBuffer[i];
				vertexData.push_back(interData);
			}
			Primitive primitive(m_device);
			primitive.setVertexData(std::move(vertexData));
			primitive.setIndexData(std::move(m_indices));
			SceneManager::addPrimitive(std::move(primitive));
		}
	}

}

void GLTFModel::processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive)
{
	m_posBuffer.clear();
	if (primitive.attributes.find("POSITION") == primitive.attributes.end())
	{
		std::cerr << "No POSITION attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
	const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
	const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

	const unsigned char* posDataPtr = posBuffer.data.data() + posAccessor.byteOffset + posBufferView.byteOffset;
	const float* posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	size_t vertexCount = posAccessor.count;
	int components = (posAccessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		Vertex vertex(-INFINITY, -INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) vertex.x = posFloatPtr[i * components + j];
			else if (j == 1) vertex.y = posFloatPtr[i * components + j];
			else if (j == 2) vertex.z = posFloatPtr[i * components + j];
		}
		m_posBuffer.push_back(vertex);
	}
}

void GLTFModel::processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive)
{
	m_texCoordsBuffer.clear();
	if (primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end())
	{
		std::cerr << "No TEXCOORD_0 attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
	const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
	const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

	const unsigned char* posDataPtr = posBuffer.data.data() + posAccessor.byteOffset + posBufferView.byteOffset;
	const float* posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	size_t vertexCount = posAccessor.count;
	int components = (posAccessor.type == TINYGLTF_TYPE_VEC2) ? 2 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		TexCoords texCoords(-INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) texCoords.u = posFloatPtr[i * components + j];
			else if (j == 1) texCoords.v = posFloatPtr[i * components + j];
		}
		m_texCoordsBuffer.push_back(texCoords);
	}
}

void GLTFModel::processIndexAttrib(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive)
{
	m_indices.clear();
	if (primitive.indices < 0)
	{
		std::cerr << "No indices found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& indexAccessor = model.accessors[primitive.indices];
	const tinygltf::BufferView& indexBufferView = model.bufferViews[indexAccessor.bufferView];
	const tinygltf::Buffer& indexBuffer = model.buffers[indexBufferView.buffer];


	const void* pIndexData = indexBuffer.data.data() + indexBufferView.byteOffset + indexAccessor.byteOffset;
	size_t indexCount = indexAccessor.count;
	if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
	{
		const uint16_t* indices = reinterpret_cast<const uint16_t*>(pIndexData);
		m_indices.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
	{
		const uint32_t* indices = reinterpret_cast<const uint32_t*>(pIndexData);
		m_indices.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
	{
		const uint8_t* indices = reinterpret_cast<const uint8_t*>(pIndexData);
		m_indices.assign(indices, indices + indexCount);
	}
}
