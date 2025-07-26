#ifndef GLM_ENABLE_EXPERIMENTAL
#define GLM_ENABLE_EXPERIMENTAL
#endif

#include "transform.hpp"
#include <glm/gtx/matrix_decompose.hpp>

void Transform::updateMatrix()
{
	prevMatrix = matrix;

	// Create transformation matrix: Scale -> Rotate -> Translate
	glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), glm::vec3(0, 0, 1)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), glm::vec3(0, 1, 0)) *
		glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), glm::vec3(1, 0, 0));
	glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), scale);

	matrix = translationMatrix * rotationMatrix * scaleMatrix;
}

glm::mat4 Transform::getWorldMatrix(const Transform* parentTransform) const
{
	if (parentTransform)
	{
		return parentTransform->matrix * matrix;
	}
	return matrix;
}

void Transform::translate(const glm::vec3& translation)
{
	position += translation;
	updateMatrix();
}

void Transform::rotate(const glm::vec3& eulerAngles)
{
	rotation += eulerAngles;
	updateMatrix();
}

void Transform::setScale(const glm::vec3& newScale)
{
	scale = newScale;
	updateMatrix();
}

void Transform::decompose()
{
	glm::vec3 skew;
	glm::vec4 perspective;
	glm::quat orientation;
	glm::decompose(matrix, scale, orientation, position, skew, perspective);
	rotation = glm::degrees(glm::eulerAngles(orientation));
}