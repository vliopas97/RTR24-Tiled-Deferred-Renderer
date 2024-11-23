#pragma once

#include "Core\Core.h"

class Camera
{
public:
	Camera();

	void SetPosition(const glm::vec3& position);
	void SetRotation(const glm::vec3& rotation);

	inline glm::vec3 GetPosition() const { return Position; }
	inline glm::vec3 GetRotation() const { return Rotation; }

	inline const glm::mat4x4& GetProjection() const { return Projection; }
	inline const glm::mat4x4& GetView() const { return View; }
	inline const glm::mat4x4& GetViewProjection() const { return ViewProjection; }

	void Tick(float delta);

private:
	void UpdateViewMatrix();

private:
	float FovY;
	float AspectRatio;
	float NearZ;
	float FarZ;

	glm::mat4x4 Projection;
	glm::mat4x4 View;
	glm::mat4x4 ViewProjection;
	glm::mat4x4 RotationMatrix;

	glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
	glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };

	float TranslationSpeed = 20.0f, RotationSpeed = 0.05f;
};