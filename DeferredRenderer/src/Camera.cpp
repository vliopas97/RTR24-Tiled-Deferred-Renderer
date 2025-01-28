#include "Camera.h"
#include "Application.h"

#include <algorithm>
#include <numbers>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

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

void SaveStateToFile(const std::string& filename, glm::vec3 position, glm::vec3 rotation)
{
	auto path = std::filesystem::current_path().parent_path().string() + "\\Content\\" + filename;

	static std::ofstream file;
	static bool init = true;

	if (init)
	{
		file.open(path, std::ios::out | std::ios::app);
		init = false;
	}

	if (file.is_open())
	{
		file << "Position: " << position.x << " " << position.y << " " << position.z << "\n";
		file << "Rotation: " << rotation.x << " " << rotation.y << " " << rotation.z << "\n";
		file.flush();
		std::cout << "Camera state saved to " << filename << std::endl;
	}
	else
	{
		std::cerr << "Failed to open file for saving: " << filename << std::endl;
	}
}

float InterpolateAngle(float start, float end, float t)
{
	// Wrap angles to the range [-π, π]
	float diff = glm::mod(end - start + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();

	return start + t * diff;
}

glm::vec3 InterpolateAngles(const glm::vec3& start, const glm::vec3& end, float t)
{
	return glm::vec3(
		InterpolateAngle(start.x, end.x, t),
		InterpolateAngle(start.y, end.y, t),
		InterpolateAngle(start.z, end.z, t)
	);
}

void LoadStateFromFile(const std::string& filename, glm::vec3& currentPosition, glm::vec3& currentRotation, float deltaTime)
{
	static glm::vec3 targetPosition = currentPosition;
	static glm::vec3 targetRotation = currentRotation;

	// Build the file path
	auto path = std::filesystem::current_path().parent_path().string() + "\\Content\\" + filename;

	static std::ifstream file(path);
	static bool init = true;

	if (init)
	{
		file.open(path, std::ios::out | std::ios::app);
		init = false;
	}

	static std::string fileContents((std::istreambuf_iterator<char>(file)),
									std::istreambuf_iterator<char>());

	file.close(); // File is no longer needed

	static std::istringstream iss(fileContents);
	std::string line;

	static bool readPosition = false;
	static bool readRotation = false;

	if (!readPosition)
	{
		std::getline(iss, line);
		std::istringstream lineStream(line);
		std::string type;

		if (lineStream >> type && type == "Position:")
		{
			if (!(lineStream >> targetPosition.x >> targetPosition.y >> targetPosition.z))
			{
				std::cerr << "Failed to parse Position values on line: " << line << '\n';
			}
			readPosition = true;
		}
	}

	if (readPosition && !readRotation)
	{
		std::getline(iss, line);
		std::istringstream lineStream(line);
		std::string type;

		if (lineStream >> type && type == "Rotation:")
		{
			if (!(lineStream >> targetRotation.x >> targetRotation.y >> targetRotation.z))
			{
				std::cerr << "Failed to parse Rotation values on line: " << line << '\n';
			}
			readRotation = true;
		}
	}

	if (readPosition && readRotation)
	{
		readPosition = false;
		readRotation = false;
	}

	// Interpolate towards the target position and rotation
	float translationSpeed = 3.0f; // Adjust this value to control the speed
	float rotationSpeed = 1.0f;
	float t = deltaTime * translationSpeed;
	if (t > 1.0f) t = 1.0f; // Clamp t to avoid overshooting

	currentPosition = glm::lerp(currentPosition, targetPosition, t);

	t = deltaTime * rotationSpeed;
	if (t > 1.0f) t = 1.0f;
	currentRotation = InterpolateAngles(currentRotation, targetRotation, t);
}


Camera::Camera()
	:FovY(2.0f), AspectRatio(16.0f / 9.0f), NearZ(0.2f), FarZ(400.0f),
	Projection(glm::perspectiveLH_NO(glm::degrees(FovY), AspectRatio, NearZ, FarZ)), 
	View(glm::mat4x4(1.0f)), RotationMatrix(1.0f)
{
	ViewProjection = Projection * View;
	Position = { 0.0f, 8.0f, -4.0f };// initializing camera position - might change it later
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

	static bool startup = true;
	if (startup)
	{
		LoadStateFromFile("cameraPath.txt", Position, Rotation, delta);
		UpdateViewMatrix();
		if (!Application::GetApp().GetWindow()->IsCursorVisible())
			startup = false;
	}

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

	//SaveStateToFile("cameraPath.txt", Position, Rotation);
}