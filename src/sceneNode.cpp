#include "sceneNode.hpp"
#include <iostream>
#include "sceneManager.hpp"

SceneNode::SceneNode(SceneNode&& other) noexcept
	: transform(other.transform), children(std::move(other.children)), visible(other.visible), dirty(other.dirty),
	movable(other.movable)
{
	this->parent = other.parent;
	other.parent = nullptr;
	other.visible = false;
	other.dirty = false;
	other.movable = false;
	other.transform = Transform();
	other.children.clear();
}

SceneNode::SceneNode() : parent(nullptr) {};

SceneNode::~SceneNode() = default;

void SceneNode::addChild(std::unique_ptr<SceneNode>&& child)
{
	child->parent = this;
	children.push_back(std::move(child));
}

void SceneNode::removeChild(SceneNode* child)
{
	if (child->parent == this)
	{
		child->parent = nullptr;
		auto it = std::remove_if(children.begin(), children.end(),
			[child](const std::unique_ptr<SceneNode>& c) { return c.get() == child; });
		if (it != children.end())
		{
			children.erase(it, children.end());
		}
		else
		{
			std::cerr << "Error: Child not found in parent's children list." << std::endl;
		}
	}
}

glm::mat4 SceneNode::getWorldMatrix() const
{
	if (parent)
	{
		return parent->getWorldMatrix() * transform.matrix;
	}
	return transform.matrix;
}

void SceneNode::markDirty()
{
	dirty = true;
	// Mark all children as dirty too
	for (auto& child : children)
	{
		child->markDirty();
	}
}

void SceneNode::updateTransform()
{
	if (dirty)
	{
		transform.updateMatrix();
		dirty = false;
	}

	// Update all children
	for (auto& child : children)
	{
		child->updateTransform();
	}
}

std::pair<glm::vec3, glm::vec3> SceneNode::getWorldBounds() const
{
	// This would calculate world-space bounding box
	// Implementation depends on your specific needs
	glm::vec3 worldPos = glm::vec3(getWorldMatrix()[3]);
	return { worldPos - glm::vec3(1.0f), worldPos + glm::vec3(1.0f) };
}
