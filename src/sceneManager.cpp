#include "sceneManager.hpp"

static std::vector<Primitive> primitives;

void SceneManager::addPrimitive(Primitive&& primitive)
{
	primitives.push_back(std::move(primitive));
};

std::vector<Primitive>& SceneManager::getPrimitives()
{
	return primitives;
}

size_t SceneManager::getPrimitiveCount()
{
	return primitives.size();
}