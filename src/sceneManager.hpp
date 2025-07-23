#pragma once
#include "primitiveData.hpp"
#include <vector>
#include "primitive.hpp"

namespace SceneManager
{
	void addPrimitive(Primitive&& primitive);
	inline static std::vector<Primitive> primitives;
};