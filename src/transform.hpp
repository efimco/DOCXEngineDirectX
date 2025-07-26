#pragma once
#include <glm/glm.hpp>

struct Transform
{
	glm::vec3 position = glm::vec3(0.0f);
	glm::vec3 rotation = glm::vec3(0.0f);
	glm::vec3 scale = glm::vec3(1.0f);
	glm::mat4 matrix = glm::mat4(1.0f);
	glm::mat4 prevMatrix = glm::mat4(1.0f);

	void updateMatrix();

	glm::mat4 getWorldMatrix(const Transform* parentTransform = nullptr) const;

	void translate(const glm::vec3& translation);
	void rotate(const glm::vec3& rotation);
	void setScale(const glm::vec3& scale);

	void decompose();
};