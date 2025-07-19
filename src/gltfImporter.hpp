#pragma once
#include "tiny_gltf.h"
#include <string>


class GLTFModel
{
public:
	std::string path;
	GLTFModel(std::string path);
	~GLTFModel();

private:

	tinygltf::Model readGlb(const std::string& path);
	void processGlb(tinygltf::Model& model);

};
