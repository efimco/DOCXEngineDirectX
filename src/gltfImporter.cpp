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
	std::cout << "Processing GLTF model with " << model.meshes.size() << " meshes" << std::endl;

	for (const auto mesh : model.meshes)
	{
		std::cout << "Processing mesh: " << mesh.name << " with " << mesh.primitives.size() << " primitives" << std::endl;

		for (const auto gltfPrimitive : mesh.primitives)
		{
			std::vector<Vertex> posBuffer;
			std::vector<TexCoords> texCoordsBuffer;
			std::vector<Normals> normalBuffer;
			std::vector<Tangents> tangentBuffer;
			std::vector<uint32_t> indices;

			processPosAttribute(model, mesh, gltfPrimitive, posBuffer);
			processTexCoordAttribute(model, mesh, gltfPrimitive, texCoordsBuffer);
			processIndexAttrib(model, mesh, gltfPrimitive, indices);
			processNormalsAttribute(model, mesh, gltfPrimitive, normalBuffer);

			std::cout << "Vertex buffer size: " << posBuffer.size() << std::endl;
			std::cout << "TexCoord buffer size: " << texCoordsBuffer.size() << std::endl;
			std::cout << "Index buffer size: " << indices.size() << std::endl;

			std::vector<InterleavedData> vertexData;
			const auto numVert = posBuffer.size();
			for (int i = 0; i < numVert; i++)
			{
				InterleavedData interData;
				interData.vertex = posBuffer[i];
				interData.texCoords = texCoordsBuffer[i];
				interData.normals = normalBuffer[i];
				vertexData.push_back(interData);
			}

			std::cout << "Creating primitive with " << vertexData.size() << " vertices and " << indices.size() << " indices" << std::endl;

			Primitive primitive(m_device);
			primitive.setVertexData(std::move(vertexData));
			primitive.setIndexData(std::move(indices));
			SceneManager::addPrimitive(std::move(primitive));

			std::cout << "Added primitive. Total primitives now: " << SceneManager::getPrimitiveCount() << std::endl;
		}
	}
}

void GLTFModel::processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Vertex>& verticies)
{
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
		verticies.push_back(vertex);
	}
}

void GLTFModel::processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<TexCoords>& texCoords)
{
	if (primitive.attributes.find("TEXCOORD_0") == primitive.attributes.end())
	{
		std::cerr << "No TEXCOORD_0 attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* posDataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
	const float* posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	size_t vertexCount = accessor.count;
	int components = (accessor.type == TINYGLTF_TYPE_VEC2) ? 2 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		TexCoords texCoord(-INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) texCoord.u = posFloatPtr[i * components + j];
			else if (j == 1) texCoord.v = posFloatPtr[i * components + j];
		}
		texCoords.push_back(texCoord);
	}
}

void GLTFModel::processNormalsAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Normals>& normals)
{
	if (primitive.attributes.find("NORMAL") == primitive.attributes.end())
	{
		std::cerr << "No NORMAL attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* dataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
	const float* floatPtr = reinterpret_cast<const float*>(dataPtr);
	size_t vertexCount = accessor.count;
	int components = (accessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		Normals normal(-INFINITY, -INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0) normal.nx = floatPtr[i * components + j];
			else if (j == 1) normal.ny = floatPtr[i * components + j];
			else if (j == 2) normal.nz = floatPtr[i * components + j];
		}
		normals.push_back(normal);
	}
}

void GLTFModel::processIndexAttrib(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<uint32_t>& indicies)
{
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
		indicies.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
	{
		const uint32_t* indices = reinterpret_cast<const uint32_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
	{
		const uint8_t* indices = reinterpret_cast<const uint8_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
}
