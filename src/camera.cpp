#include "camera.hpp"


#include <glm/gtc/matrix_transform.hpp>
#include "inputEventsHandler.hpp"
#include "primitive.hpp"

static constexpr float YAW = 0.0f;
static constexpr float PITCH = 0.0f;
static constexpr float FOV = 45.f;
static constexpr auto WORLD_UP = glm::vec3(0, 1, 0);

Camera::Camera(const glm::vec3 pos, const std::string_view nodeName)
	: SceneNode(nodeName)
	, front(0.0f, 0.0f, -1.0f)
	, fov(FOV)
{
	// Initialize transform using SceneNode's properties
	transform.position = pos;
	transform.rotation = glm::vec3(PITCH, YAW, 0.0f); // pitch (X), yaw (Y)
	transform.updateMatrix();

	orbitPivot = glm::vec3(0.0f); // point to orbit around
	distanceToOrbitPivot = glm::length(transform.position - orbitPivot);
	updateCameraVectors();
}

void Camera::processMovementControls(SceneNode* activeNode)
{
	if (activeNode)
	{
		const auto prim = dynamic_cast<Primitive*>(activeNode);
		if (prim && InputEvents::isKeyDown(KeyButtons::KEY_F))
		{
			focusOn(prim);
		}
	}

	if (!InputEvents::getMouseInViewport())
		return;
	processZoom();
	if (InputEvents::isMouseDown(MouseButtons::MIDDLE_BUTTON) && InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		processPanning();
	}
	if (InputEvents::isMouseDown(MouseButtons::MIDDLE_BUTTON) && !InputEvents::isKeyDown(KeyButtons::KEY_LSHIFT))
	{
		processOrbit();
	}
	updateCameraVectors();
}

glm::mat4 Camera::getViewMatrix() const
{
	return glm::lookAtLH(transform.position, orbitPivot, WORLD_UP);
}

void Camera::processZoom()
{
	// Blender-style zoom: more responsive and distance-independent
	const float zoomSpeed = 0.1f;
	const float zoomFactor = 1.0f + (InputEvents::getMouseWheel() * zoomSpeed);

	// Clamp to prevent getting too close or too far
	const float newDistance = glm::clamp(distanceToOrbitPivot / zoomFactor, 0.01f, 1000.0f);
	distanceToOrbitPivot = newDistance;
}

void Camera::processPanning()
{
	float deltaX = 0;
	float deltaY = 0;
	InputEvents::getMouseDelta(deltaX, deltaY);

	constexpr float panSpeed = 0.001f; // Adjust this factor for desired sensitivity
	const glm::vec3 rightMove = -right * deltaX * distanceToOrbitPivot * panSpeed;
	const glm::vec3 upMove = up * deltaY * distanceToOrbitPivot * panSpeed;

	orbitPivot += rightMove + upMove;
}

void Camera::updateCameraVectors()
{
	// Use SceneNode's transform.rotation (degrees) as the source of yaw/pitch
	const float yawDeg = transform.rotation.y;
	const float pitchDeg = transform.rotation.x;

	glm::vec3 offset;
	offset.x = distanceToOrbitPivot * cos(glm::radians(pitchDeg)) * sin(glm::radians(yawDeg));
	offset.y = distanceToOrbitPivot * sin(glm::radians(pitchDeg));
	offset.z = distanceToOrbitPivot * cos(glm::radians(pitchDeg)) * cos(glm::radians(yawDeg));

	transform.position = orbitPivot + offset;
	transform.updateMatrix();

	// Derive camera basis from world axes and target
	front = glm::normalize(orbitPivot - transform.position);
	right = glm::normalize(glm::cross(WORLD_UP, front));
	up = glm::normalize(glm::cross(front, right));
}

void Camera::processOrbit()
{
	float deltaX = 0;
	float deltaY = 0;
	InputEvents::getMouseDelta(deltaX, deltaY);
	// Blender-style orbit: consistent angular speed
	constexpr float orbitSensitivity = 0.5f; // Fixed sensitivity like Blender

	// Apply deltas to SceneNode's transform.rotation (degrees)
	transform.rotation.y += deltaX * orbitSensitivity; // yaw
	transform.rotation.x += deltaY * orbitSensitivity; // pitch

	// Clamp pitch to avoid gimbal lock
	if (transform.rotation.x > 89.0f)
		transform.rotation.x = 89.0f;
	if (transform.rotation.x < -89.0f)
		transform.rotation.x = -89.0f;
}

void Camera::onCommitTransaction(Scene& scene)
{
	updateCameraVectors();
}

void Camera::copyFrom(const SceneNode& node)
{
	SceneNode::copyFrom(node);
	if (const auto cameraNode = dynamic_cast<const Camera*>(&node))
	{
		fov = cameraNode->fov;
	}
}

bool Camera::differsFrom(const SceneNode& node) const
{
	if (!SceneNode::differsFrom(node))
	{
		if (const auto cameraNode = dynamic_cast<const Camera*>(&node))
		{
			return fov != cameraNode->fov;
		}
	}
	return true;
}

std::unique_ptr<SceneNode> Camera::clone() const
{
	auto cameraNode = std::make_unique<Camera>(transform.position);
	cameraNode->copyFrom(*this);
	return cameraNode;
}

void Camera::focusOn(Primitive* primitive)
{
	const auto bbox = primitive->getWorldBBox();
	const auto minPos = bbox.min;
	const auto maxPos = bbox.max;
	const glm::vec3 center = (minPos + maxPos) * 0.5f;
	const float radius = glm::length(maxPos - minPos);
	orbitPivot = center;

	// Position camera to see the entire bounding box (Blender-style framing)
	const float distance = radius; // More breathing room like Blender
	distanceToOrbitPivot = distance;

	updateCameraVectors();
}

std::string_view Camera::getNodeTypename() const
{
	return "Camera";
}

void Camera::load(Nodes::LoadContext& loadContext, nlohmann::json& json)
{
	SceneNode::load(loadContext, json);
}

void Camera::save(Nodes::SaveContext& saveContext, nlohmann::json& json) const
{
	SceneNode::save(saveContext, json);
}
