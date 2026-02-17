#pragma once

#include <glm/glm.hpp>

#include "sceneNode.hpp"

enum LightType
{
	POINT_LIGHT = 0, DIRECTIONAL_LIGHT = 1, SPOT_LIGHT = 2
};

struct alignas(16) LightData
{
	LightType type;
	glm::vec3 position;
	float intensity;
	glm::vec3 color;
	glm::vec3 direction;
	float padding1;       // Padding to align to 16 bytes
	glm::vec2 spotParams; // x: inner cone angle, y: outer cone angle
	glm::vec2 padding2;   // Additional padding to align to 16 bytes
	glm::vec3 attenuations;
	float radius; // radius for point lights
	float objectID;
	glm::vec3 padding3; // Padding to maintain 16-byte alignment
};

class Light : public SceneNode
{
public:
	Light(LightType type, glm::vec3 position, std::string_view nodeName = "Light");
	LightData getLightData();

	float intensity = 1.0f;
	glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);
	glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);
	glm::vec3 attenuation = glm::vec3(1.0f, 0.0f, 0.0f);
	glm::vec2 spotParams = glm::vec2(0.0f, 0.0f); // x: inner cone angle, y: outer cone angle
	float radius = 1.0f;
	LightType type = POINT_LIGHT;

	void onCommitTransaction(Scene& scene) override;
	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;

protected:
	virtual std::string_view getNodeTypename() const;
	virtual void load(Nodes::LoadContext& loadContext, nlohmann::json& json);
	virtual void save(Nodes::SaveContext& saveContext, nlohmann::json& json) const;

};
