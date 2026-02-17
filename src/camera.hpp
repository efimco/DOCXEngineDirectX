#pragma once

#include <glm/glm.hpp>

#include "sceneNode.hpp"

class Primitive;

class Camera : public SceneNode
{
public:
	glm::vec3 front;
	glm::vec3 up{};
	glm::vec3 right{};
	glm::vec3 worldUp{};
	glm::vec3 orbitPivot{};

	float fov = 45.0f;
	float distanceToOrbitPivot = 0.f;
	float nearPlane = 0.1f;
	float farPlane = 1000.0f;

	explicit Camera(glm::vec3 pos, std::string_view nodeName = "Camera");
	void processMovementControls(SceneNode* activeNode);
	glm::mat4 getViewMatrix() const;

	void updateCameraVectors();

private:
	void processZoom();
	void processPanning();
	void processOrbit();

	void focusOn(Primitive* primitive);

	void onCommitTransaction(Scene& scene) override;
	void copyFrom(const SceneNode& node) override;
	bool differsFrom(const SceneNode& node) const override;
	std::unique_ptr<SceneNode> clone() const override;

protected:
	virtual std::string_view getNodeTypename() const;
	virtual void load(Nodes::LoadContext& loadContext, nlohmann::json& json);
	virtual void save(Nodes::SaveContext& saveContext, nlohmann::json& json) const;

};
