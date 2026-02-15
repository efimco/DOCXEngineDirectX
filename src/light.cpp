#include "light.hpp"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>

#include "scene.hpp"

Light::Light(const LightType type, const glm::vec3 position, const std::string_view nodeName)
	: SceneNode(nodeName)
	, type(type)
{
	transform.position = position;
	transform.updateMatrix();
}

LightData Light::getLightData()
{
	LightData data;
	data.type = type;
	glm::mat4 worldMatrix = getWorldMatrix();
	data.position = glm::vec3(worldMatrix[3]);
	data.intensity = getVisibility() ? intensity : 0.0f;
	data.color = color;
	data.direction = glm::rotate(direction, glm::radians(transform.rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	data.direction = glm::rotate(data.direction, glm::radians(transform.rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	data.direction = glm::rotate(data.direction, glm::radians(transform.rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));
	data.spotParams = spotParams;
	data.attenuations = attenuation;
	data.radius = radius;
	return data;
}

void Light::onCommitTransaction(Scene& scene)
{
	scene.setLightsDirty(true);
}

void Light::copyFrom(const SceneNode& node)
{
	SceneNode::copyFrom(node);
	if (const auto lightNode = dynamic_cast<const Light*>(&node))
	{
		intensity = lightNode->intensity;
		color = lightNode->color;
		direction = lightNode->direction;
		attenuation = lightNode->attenuation;
		spotParams = lightNode->spotParams;
		radius = lightNode->radius;
		type = lightNode->type;
	}
}

bool Light::differsFrom(const SceneNode& node) const
{
	if (!SceneNode::differsFrom(node))
	{
		if (const auto lightNode = dynamic_cast<const Light*>(&node))
		{
			return intensity != lightNode->intensity ||
				color != lightNode->color ||
				direction != lightNode->direction ||
				attenuation != lightNode->attenuation ||
				spotParams != lightNode->spotParams ||
				radius != lightNode->radius ||
				type != lightNode->type;
		}
	}
	return true;
}

std::unique_ptr<SceneNode> Light::clone() const
{
	auto lightNode = std::make_unique<Light>(type, transform.position, name);
	lightNode->copyFrom(*this);
	return lightNode;
}
