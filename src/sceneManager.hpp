#pragma once
#include "primitiveData.hpp"
#include <vector>
#include "primitive.hpp"

namespace SceneManager
{
	void addPrimitive(Primitive&& primitive);
	std::vector<Primitive>& getPrimitives();
	size_t getPrimitiveCount();
};