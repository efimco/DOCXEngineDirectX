#include "gltfImporter.hpp"
#include <iostream>
GLTFModel::GLTFModel(std::string path)
{
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

void GLTFModel::processGlb(tinygltf::Model& model)
{
	for (const auto mesh : model.meshes)
	{
		for (const auto primitive : mesh.primitives)
		{
			if (primitive.attributes.find("POSITION") == primitive.attributes.end())
			{
				std::cerr << "No POSITION attribute found in primitive " << mesh.name << std::endl;
			}
			const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.at("POSITION")];
			const tinygltf::BufferView& posBufferView = model.bufferViews[posAccessor.bufferView];
			const tinygltf::Buffer& posBuffer = model.buffers[posBufferView.buffer];


		}
	}

}
