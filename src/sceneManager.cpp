#include "sceneManager.hpp"

void SceneManager::addPrimitive(Primitive&& primitive)
{
	SceneManager::primitives.push_back(std::move(primitive));
};
