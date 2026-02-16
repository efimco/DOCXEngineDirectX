#include "gltfImporter.hpp"

#include <iostream>
#include <memory>

#include <glm/gtc/type_ptr.hpp>

#include "material.hpp"
#include "primitive.hpp"
#include "scene.hpp"
#include "texture.hpp"


GLTFModel::GLTFModel(const std::string& path, ComPtr<ID3D11Device> device, Scene* scene)
	: m_device(device)
	, m_deferredContext(nullptr)
	, m_scene(scene)
	, m_progress(nullptr)
{
	const tinygltf::Model model = readGlb(path);
	processGlb(model);
}



std::future<AsyncImportResult> GLTFModel::importModelAsync(
	const std::string& path, ComPtr<ID3D11Device> device, std::shared_ptr<ImportProgress> progress)
{
	return std::async(std::launch::async, [path, device, progress]() -> AsyncImportResult
		{
			AsyncImportResult result;
			result.progress = progress;
			try
			{
				progress->setStage("Creating deferred context...");

				ComPtr<ID3D11DeviceContext> deferredCtx;
				HRESULT hr = device->CreateDeferredContext(0, &deferredCtx);
				if (FAILED(hr))
				{
					progress->hasFailed = true;
					progress->message = "Failed to create deferred context";
					return result;
				}

				GLTFModel importer(path, device, deferredCtx, progress);

				result.primitives = std::move(importer.m_pendingPrimitives);
				result.textures = std::move(importer.m_pendingTextures);
				result.materials = std::move(importer.m_pendingMaterials);

				progress->setStage("Finishing command list...");
				hr = deferredCtx->FinishCommandList(FALSE, &result.commandList);
				if (FAILED(hr))
				{
					progress->hasFailed = true;
					progress->message = "Failed to finish command list";
					return result;
				}

				progress->progress = 1.0f;
				progress->isCompleted = true;
				progress->setStage("Ready");

			}
			catch (const std::exception& e)
			{
				progress->hasFailed = true;
				progress->message = e.what();
			}

			return result; });
}

void GLTFModel::finalizeAsyncImport(
	AsyncImportResult&& importResult, ComPtr<ID3D11DeviceContext> immediateContext, Scene* scene)
{
	if (importResult.commandList)
	{
		immediateContext->ExecuteCommandList(importResult.commandList.Get(), TRUE);
	}

	for (auto& prim : importResult.primitives)
	{
		scene->addChild(std::move(prim));
	}

	for (auto& tex : importResult.textures)
	{
		scene->addTexture(std::move(tex));
	}

	for (auto& mat : importResult.materials)
	{
		scene->addMaterial(std::move(mat));
	}
}

GLTFModel::GLTFModel(const std::string& path, ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deferredContext, std::shared_ptr<ImportProgress> progress)
	: m_device(device)
	, m_deferredContext(deferredContext)
	, m_scene(nullptr)
	, m_progress(progress)
{
	const tinygltf::Model model = readGlb(path);
	processGlb(model);
}


tinygltf::Model GLTFModel::readGlb(const std::string& path)
{
	tinygltf::TinyGLTF loader;
	tinygltf::Model model;
	std::string err;
	std::string warn;
	std::cout << "Loading glTF file: " << path << std::endl;
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
		std::cout << "Warn: " << warn << std::endl;
	if (!err.empty())
		std::cout << "Err: " << err << std::endl;
	if (!ret)
		std::cout << "Failed to parse glTF" << std::endl;
	std::cout << "Loaded " << path << std::endl;
	return model;
}

void GLTFModel::processGlb(const tinygltf::Model& model)
{
	std::cout << "Processing GLTF model with " << model.meshes.size() << " meshes" << std::endl;

	if (m_progress)
		m_progress->setStage("Processing Textures...");
	if (m_progress)
		m_progress->progress = 0.1f;
	processTextures(model);

	if (m_progress)
		m_progress->setStage("Processing Images...");
	if (m_progress)
		m_progress->progress = 0.2f;
	processImages(model);

	if (m_progress)
		m_progress->setStage("Processing Materials...");
	if (m_progress)
		m_progress->progress = 0.3f;
	processMaterials(model);

	if (m_progress)
		m_progress->setStage("Processing Meshes...");
	if (m_progress)
		m_progress->progress = 0.4f;
	size_t totalPrimitives = 0;
	for (const auto& mesh : model.meshes)
	{
		totalPrimitives += mesh.primitives.size();
	}
	size_t processedPrimitives = 0;

	for (const auto& mesh : model.meshes)
	{
		for (const auto& gltfPrimitive : mesh.primitives)
		{
			std::vector<Position> posBuffer;
			std::vector<TexCoords> texCoordsBuffer;
			std::vector<Normal> normalBuffer;
			std::vector<uint32_t> indices;

			processPosAttribute(model, mesh, gltfPrimitive, posBuffer);
			processTexCoordAttribute(model, mesh, gltfPrimitive, texCoordsBuffer);
			processIndexAttrib(model, mesh, gltfPrimitive, indices);
			processNormalsAttribute(model, mesh, gltfPrimitive, normalBuffer);
			std::vector<Vertex> vertexData;
			const auto numVert = posBuffer.size();
			for (int i = 0; i < numVert; i++)
			{
				Vertex interData{};
				interData.position = posBuffer[i];
				interData.texCoords = i < texCoordsBuffer.size() ? texCoordsBuffer[i] : TexCoords(0, 0);
				interData.normal = i < normalBuffer.size() ? normalBuffer[i] : Normal(0, 1, 0);
				vertexData.push_back(interData);
			}
			size_t meshIndex = &mesh - &model.meshes[0];
			Transform transform = getTransformFromNode(meshIndex, model);
			auto primitive = std::make_unique<Primitive>(m_device);

			primitive->transform = transform;
			primitive->name = model.nodes[meshIndex].name;
			if (primitive->name.empty())
			{
				primitive->name = "Primitive";
			}

			primitive->setVertexData(std::move(vertexData));
			primitive->setIndexData(std::move(indices));
			primitive->fillTriangles();

			if (gltfPrimitive.material >= 0)
			{
				primitive->material = m_materialIndex[gltfPrimitive.material];
			}
			else
			{
				primitive->material = std::make_shared<Material>();

				primitive->material->name = primitive->name + "_defmat";
				if (m_scene)
				{
					m_scene->addMaterial(primitive->material);
				}
				else
				{
					m_pendingMaterials.push_back(primitive->material);
				}
			}
			if (m_scene)
			{
				m_scene->addChild(std::move(primitive));
			}
			else
			{
				m_pendingPrimitives.push_back(std::move(primitive));
			}
			processedPrimitives++;
			if (m_progress)
			{
				m_progress->progress = 0.4f + 0.6f * (static_cast<float>(processedPrimitives) / totalPrimitives);
			}

			if (m_scene)
			{
				std::cout << "Added primitive. Total primitives now: " << m_scene->getPrimitiveCount() << std::endl;
			}
			else
			{
				std::cout << "Added primitive to pending list. Count: " << m_pendingPrimitives.size() << std::endl;
			}
		}
	}
}

void GLTFModel::processTextures(const tinygltf::Model& model)
{
	for (int i = 0; i < model.textures.size(); i++)
	{
		m_textureIndex[i] = model.textures[i].source;
	}
}

void GLTFModel::processImages(const tinygltf::Model& model)
{
	for (int i = 0; i < model.images.size(); i++)
	{
		std::string name = model.images[i].name;
		if (name.empty())
		{
			name = model.images[i].uri;
		}

		std::shared_ptr<Texture> texture;
		if (m_scene && m_scene->getTexture(name) != nullptr)
		{
			texture = m_scene->getTexture(name);
		}
		else
		{
			// Pass deferred context for async path, nullptr (immediate) for sync path
			texture = std::make_shared<Texture>(model.images[i], m_device, m_deferredContext);
			if (m_scene)
			{
				m_scene->addTexture(std::shared_ptr<Texture>(texture));
			}
			else
			{
				m_pendingTextures.push_back(texture);
			}
		}
		m_imageIndex[i] = texture;
	}
}

void GLTFModel::processMaterials(const tinygltf::Model& model)
{
	for (int i = 0; i < model.materials.size(); i++)
	{
		auto& material = model.materials[i];
		auto mat = std::make_shared<Material>();
		if (material.pbrMetallicRoughness.baseColorTexture.index != -1)
		{
			mat->albedo = m_imageIndex[m_textureIndex[material.pbrMetallicRoughness.baseColorTexture.index]];
		}

		if (material.pbrMetallicRoughness.metallicRoughnessTexture.index != -1)
		{
			const auto index = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
			mat->metallicRoughness = m_imageIndex[m_textureIndex[index]];
		}
		if (material.normalTexture.index != -1)
		{
			mat->normal = m_imageIndex[m_textureIndex[material.normalTexture.index]];
		}
		mat->name = material.name;
		if (mat->name.empty())
		{
			mat->name = "Material_" + std::to_string(i);
		}
		if (material.pbrMetallicRoughness.baseColorFactor.size() == 4)
		{
			mat->albedoColor = glm::vec4(
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[0]),
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[1]),
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[2]),
				static_cast<float>(material.pbrMetallicRoughness.baseColorFactor[3]));
		}

		mat->roughnessValue = static_cast<float>(material.pbrMetallicRoughness.roughnessFactor);
		mat->metallicValue = static_cast<float>(material.pbrMetallicRoughness.metallicFactor);

		auto name = material.name;

		if (m_scene)
		{
			if (m_scene->getMaterial(name) == nullptr)
			{
				m_scene->addMaterial(mat);
			}
			m_materialIndex[i] = m_scene->getMaterial(name);
		}
		else
		{
			m_pendingMaterials.push_back(mat);
			m_materialIndex[i] = mat;
		}
	}

	if (model.materials.empty())
	{
		std::cerr << "No materials found in the model." << std::endl;
	}
	else
	{
		std::cout << "Processed " << model.materials.size() << " materials." << std::endl;
	}
}

void GLTFModel::processPosAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<Position>& verticies)
{
	if (!primitive.attributes.contains("POSITION"))
	{
		std::cerr << "No POSITION attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
	const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
	const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];

	const unsigned char* posDataPtr = posBuffer.data.data() + posAccessor.byteOffset + posBufferView.byteOffset;
	const auto posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	const size_t vertexCount = posAccessor.count;
	const int components = (posAccessor.type == TINYGLTF_TYPE_VEC3) ? 3 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		Position position(-INFINITY, -INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0)
				position.x = posFloatPtr[i * components + j];
			else if (j == 1)
				position.y = posFloatPtr[i * components + j];
			else if (j == 2)
				position.z = posFloatPtr[i * components + j];
		}
		verticies.push_back(position);
	}
}

void GLTFModel::processTexCoordAttribute(const tinygltf::Model& model, const tinygltf::Mesh& mesh, const tinygltf::Primitive& primitive, std::vector<TexCoords>& texCoords)
{
	if (!primitive.attributes.contains("TEXCOORD_0"))
	{
		std::cerr << "No TEXCOORD_0 attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("TEXCOORD_0")];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* posDataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
	const auto posFloatPtr = reinterpret_cast<const float*>(posDataPtr);
	const size_t vertexCount = accessor.count;
	const int components = (accessor.type == TINYGLTF_TYPE_VEC2) ? 2 : 0;

	for (size_t i = 0; i < vertexCount; i++)
	{
		TexCoords texCoord(-INFINITY, -INFINITY);
		for (int j = 0; j < components; j++)
		{
			if (j == 0)
				texCoord.x = posFloatPtr[i * components + j];
			else if (j == 1)
				texCoord.y = posFloatPtr[i * components + j];
		}
		texCoords.push_back(texCoord);
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
	const size_t indexCount = indexAccessor.count;
	if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
	{
		const auto indices = static_cast<const uint16_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT)
	{
		const auto indices = static_cast<const uint32_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
	else if (indexAccessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
	{
		const auto indices = static_cast<const uint8_t*>(pIndexData);
		indicies.assign(indices, indices + indexCount);
	}
}

void GLTFModel::processNormalsAttribute(const tinygltf::Model& model,
	const tinygltf::Mesh& mesh,
	const tinygltf::Primitive& primitive,
	std::vector<Normal>& normals)
{
	if (!primitive.attributes.contains("NORMAL"))
	{
		std::cerr << "No NORMAL attribute found in primitive " << mesh.name << std::endl;
		return;
	}
	const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.at("NORMAL")];
	const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
	const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

	const unsigned char* dataPtr = buffer.data.data() + accessor.byteOffset + bufferView.byteOffset;
	const size_t vertexCount = accessor.count;

	// Use byteStride if specified, otherwise assume tightly packed vec3 floats
	size_t byteStride = bufferView.byteStride;
	if (byteStride == 0)
		byteStride = sizeof(float) * 3;  // tightly packed

	for (size_t i = 0; i < vertexCount; i++)
	{
		const float* floatPtr = reinterpret_cast<const float*>(dataPtr + i * byteStride);
		Normal normal;
		normal.x = floatPtr[0];
		normal.y = floatPtr[1];
		normal.z = floatPtr[2];
		normals.push_back(normal);
	}
}

Transform GLTFModel::getTransformFromNode(const size_t meshIndex, const tinygltf::Model& model)
{
	Transform transform;
	transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
	tinygltf::Node node;
	for (const auto& n : model.nodes)
	{
		if (n.mesh == static_cast<int>(meshIndex))
		{
			node = n;
			break;
		}
	}
	if (node.translation.size() != 0)
	{
		transform.position = glm::vec3(node.translation[0], node.translation[1],
			node.translation[2]);
	}

	transform.rotation = glm::vec3(0.0f, 0.0f, 0.0f);
	if (node.rotation.size() >= 4)
	{
		const glm::quat quatRot(
			static_cast<float>(node.rotation[3]),  // w
			static_cast<float>(node.rotation[0]),  // x
			static_cast<float>(node.rotation[1]),  // y
			static_cast<float>(node.rotation[2])); // z

		transform.rotation = glm::degrees(glm::eulerAngles(quatRot));
	}

	transform.scale = glm::vec3(1.0f, 1.0f, 1.0f);
	if (node.scale.size() != 0)
	{
		transform.scale = glm::vec3(node.scale[0], node.scale[1], node.scale[2]);
	}
	transform.matrix = glm::mat4(1.0f);
	transform.updateMatrix();
	return transform;
}
