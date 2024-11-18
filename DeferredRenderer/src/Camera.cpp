#include "Camera.h"
#include "Application.h"

#include <numbers>

template<typename T>
static T wrap_angle(T theta) noexcept
{
	constexpr T twoPi = static_cast<T>(2.0 * std::numbers::pi);
	const T mod = std::fmod(theta, twoPi);
	if (mod > std::numbers::pi)
	{
		return mod - twoPi;
	}
	else if (mod < -std::numbers::pi)
	{
		return mod + twoPi;
	}
	return mod;
}

Camera::Camera()
	:FovY(2.0f), AspectRatio(16.0f / 9.0f), NearZ(0.4f), FarZ(400.0f),
	Projection(glm::perspectiveLH_NO(glm::degrees(FovY), AspectRatio, NearZ, FarZ)), 
	View(glm::mat4x4(1.0f)), RotationMatrix(1.0f)
{
	ViewProjection = Projection * View;
	Position = { 0.0f, 0.0f, -4.0f };// initializing camera position - might change it later
}


void Camera::SetPosition(const glm::vec3& position)
{
	Position = position;
	UpdateViewMatrix();
}

void Camera::SetRotation(const glm::vec3& rotation)
{
	Rotation = rotation;
	UpdateViewMatrix();
}

void Camera::Tick(float delta)
{

	auto& input = Application::GetApp().GetWindow()->Input;

	glm::vec3 cameraPosition{ 0.0f };
	if (input.IsKeyPressed(0x57)) cameraPosition.z = delta;
	else if (input.IsKeyPressed(0x53)) cameraPosition.z = -delta;

	cameraPosition = RotationMatrix * glm::scale(glm::vec3(TranslationSpeed)) * glm::vec4(cameraPosition, 1.0f);
	SetPosition({ Position.x + cameraPosition.x, Position.y + cameraPosition.y, Position.z + cameraPosition.z });
	cameraPosition = {};

	if (input.IsKeyPressed(0x41))  cameraPosition.x = -delta;
	else if (input.IsKeyPressed(0x44)) cameraPosition.x = delta;

	cameraPosition = RotationMatrix * glm::scale(glm::vec3(TranslationSpeed)) * glm::vec4(cameraPosition, 1.0f);
	SetPosition({ Position.x + cameraPosition.x, Position.y + cameraPosition.y, Position.z + cameraPosition.z });

	if (Application::GetApp().GetWindow()->IsCursorVisible())
		return;

	float pitch = (Rotation.x);
	float yaw = (Rotation.y);
	float roll = (Rotation.z);

	while (auto coords = input.FetchRawInputCoords())
	{
		auto [x, y] = coords.value();
		yaw = wrap_angle(yaw + glm::radians(RotationSpeed * x));
		pitch = std::clamp(pitch + glm::radians(RotationSpeed * y),
						   -(float)(0.995f * std::numbers::pi) / 2.0f,
						   (float)(0.995f * std::numbers::pi) / 2.0f);
	}

	SetRotation({ pitch, yaw, roll });
}

void Camera::UpdateViewMatrix()
{
	glm::quat rotationQuat = glm::quat(Rotation);
	RotationMatrix = glm::mat4_cast(rotationQuat);

	glm::vec3 directionVector = glm::normalize(glm::vec3(RotationMatrix[2]));

	View = glm::lookAtLH(Position, Position + directionVector, glm::vec3(0, 1, 0));
	ViewProjection = Projection * View;
}