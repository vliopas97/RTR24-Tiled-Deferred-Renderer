#pragma once
#include "Core/Core.h"
#include "Rendering/Buffer.h"

namespace Primitives
{
	template<typename Vertex>
	concept IsVertexElement = std::is_base_of_v<VertexElement, Vertex>;

	template<IsVertexElement Vertex>
	struct IndexedVertices
	{
		IndexedVertices() = default;
		IndexedVertices(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices)
			: Vertices(std::move(vertices)), Indices(std::move(indices))
		{}
		void Transform(const glm::mat4x4& matrix)
		{
			for (auto& v : Vertices)
			{
				const auto& position = v.Position;
				v.Position = matrix * position;
			}
		}
	public:
		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
	};

	class Cube
	{
	public:

		template<typename Vertex>
		requires std::is_base_of_v<VertexElement, Vertex>
		static IndexedVertices<Vertex> Create()
		{
			constexpr float side = 0.5f;
			std::vector<glm::vec3> positions = {
			{ glm::vec3(-side, -side, -side) },
			{ glm::vec3(side, -side, -side) },
			{ glm::vec3(-side, side, -side) },
			{ glm::vec3(side, side, -side) },
			{ glm::vec3(-side, -side, side) },
			{ glm::vec3(side, -side, side) },
			{ glm::vec3(-side, side, side) },
			{ glm::vec3(side, side, side) }
			};

			std::vector<Vertex> vertices(positions.size());
			for (size_t i = 0; i < positions.size(); i++)
			{
				vertices[i].Position = positions[i];
			}
			return { std::move(vertices),
						{
							0,2,1, 2,3,1,
							1,3,5, 3,7,5,
							2,6,3, 3,6,7,
							4,5,7, 4,7,6,
							0,4,2, 2,4,6,
							0,1,4, 1,5,4
						}
			};
		}

		template<IsVertexElement Vertex>
		static IndexedVertices<Vertex> CreateWNormals()
		{
			constexpr float side = 0.5f;
			std::vector<glm::vec3> positions = {
				glm::vec3(-side, -side, -side),
				glm::vec3(side, -side, -side),
				glm::vec3(-side, side, -side),
				glm::vec3(side, side, -side),
				glm::vec3(-side, -side, side),
				glm::vec3(side, -side, side),
				glm::vec3(-side, side, side),
				glm::vec3(side, side, side),
				glm::vec3(-side, -side, -side),
				glm::vec3(-side, side, -side),
				glm::vec3(-side, -side, side),
				glm::vec3(-side, side, side),
				glm::vec3(side, -side, -side),
				glm::vec3(side, side, -side),
				glm::vec3(side, -side, side),
				glm::vec3(side, side, side),
				glm::vec3(-side, -side, -side),
				glm::vec3(side, -side, -side),
				glm::vec3(-side, -side, side),
				glm::vec3(side, -side, side),
				glm::vec3(-side, side, -side),
				glm::vec3(side, side, -side),
				glm::vec3(-side, side, side),
				glm::vec3(side, side, side)
			};

			std::vector<Vertex> vertices(positions.size());
			for (size_t i = 0; i < positions.size(); i++)
				vertices[i].Position = positions[i];

			std::vector<uint32_t> indices = {
				0, 2, 1, 2, 3, 1,
				4, 5, 7, 4, 7, 6,
				8, 10, 9, 10, 11, 9,
				12, 13, 15, 12, 15, 14,
				16, 17, 18, 18, 17, 19,
				20, 23, 21, 20, 22, 23
			};

			for (size_t i = 0; i < indices.size(); i += 3)
			{
				auto& v0 = vertices[indices[i]];
				auto& v1 = vertices[indices[i + 1]];
				auto& v2 = vertices[indices[i + 2]];

				const glm::vec3& p0 = v0.Position;
				const glm::vec3& p1 = v1.Position;
				const glm::vec3& p2 = v2.Position;

				glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
				n *= -1.0f;  // Flip the normal if necessary

				v0.Normal = n;
				v1.Normal = n;
				v2.Normal = n;
			}

			return { std::move(vertices), std::move(indices) };
		}
	};

	class Sphere
	{
	private:
		template<IsVertexElement Vertex>
		static IndexedVertices<Vertex> CreateTesselated(int divisionPhi, int divisionTheta)
		{
			assert(divisionPhi >= 3);
			assert(divisionTheta >= 3);

			constexpr float radius = 1.0f;
			glm::vec3 base(0.0f, 0.0f, radius);

			const float phiAngle = glm::pi<float>() / divisionPhi;
			const float thetaAngle = 2.0f * glm::pi<float>() / divisionTheta;

			std::vector<Vertex> vertices;
			for (unsigned short arcPhi = 1; arcPhi < divisionPhi; arcPhi++)
			{
				glm::mat4 rotationX = glm::rotate(glm::mat4(1.0f), phiAngle * arcPhi, glm::vec3(1.0f, 0.0f, 0.0f));
				glm::vec3 phiBase = glm::vec3(rotationX * glm::vec4(base, 1.0f));  // Apply the X rotation to the base

				for (unsigned short arcTheta = 0; arcTheta < divisionTheta; arcTheta++)
				{
					glm::mat4 rotationZ = glm::rotate(glm::mat4(1.0f), thetaAngle * arcTheta, glm::vec3(0.0f, 0.0f, 1.0f));
					glm::vec3 v = glm::vec3(rotationZ * glm::vec4(phiBase, 1.0f));  // Apply the Z rotation to the phiBase

					vertices.emplace_back(Vertex{ v });
				}
			}

			// Add poles
			const unsigned short northPole = static_cast<unsigned short>(vertices.size());
			vertices.emplace_back(Vertex{ base });

			const unsigned short southPole = static_cast<unsigned short>(vertices.size());
			vertices.emplace_back(Vertex{ -base });

			// Helper lambda to calculate the index in the vertex array
			auto calcIndex = [divisionTheta](unsigned short arcPhi, unsigned short arcTheta)
				{
					return arcPhi * divisionTheta + arcTheta;
				};

			std::vector<uint32_t> indices;
			// Generate the faces for the middle part of the sphere
			for (unsigned short arcPhi = 0; arcPhi < divisionPhi - 2; arcPhi++)
			{
				for (unsigned short arcTheta = 0; arcTheta < divisionTheta - 1; arcTheta++)
				{
					indices.push_back(calcIndex(arcPhi, arcTheta));
					indices.push_back(calcIndex(arcPhi + 1, arcTheta));
					indices.push_back(calcIndex(arcPhi, arcTheta + 1));

					indices.push_back(calcIndex(arcPhi, arcTheta + 1));
					indices.push_back(calcIndex(arcPhi + 1, arcTheta));
					indices.push_back(calcIndex(arcPhi + 1, arcTheta + 1));
				}

				// Wrap around the theta loop
				indices.push_back(calcIndex(arcPhi, divisionTheta - 1));
				indices.push_back(calcIndex(arcPhi + 1, divisionTheta - 1));
				indices.push_back(calcIndex(arcPhi, 0));

				indices.push_back(calcIndex(arcPhi, 0));
				indices.push_back(calcIndex(arcPhi + 1, divisionTheta - 1));
				indices.push_back(calcIndex(arcPhi + 1, 0));
			}

			// Generate the faces for the poles
			for (unsigned short arcTheta = 0; arcTheta < divisionTheta - 1; arcTheta++)
			{
				indices.push_back(northPole);
				indices.push_back(calcIndex(0, arcTheta));
				indices.push_back(calcIndex(0, arcTheta + 1));

				indices.push_back(calcIndex(divisionPhi - 2, arcTheta + 1));
				indices.push_back(calcIndex(divisionPhi - 2, arcTheta));
				indices.push_back(southPole);
			}

			// Wrap around the poles
			indices.push_back(northPole);
			indices.push_back(calcIndex(0, divisionTheta - 1));
			indices.push_back(calcIndex(0, 0));

			indices.push_back(calcIndex(divisionPhi - 2, 0));
			indices.push_back(calcIndex(divisionPhi - 2, divisionTheta - 1));
			indices.push_back(southPole);

			return { std::move(vertices), std::move(indices) };
		}

	public:
		template<IsVertexElement Vertex>
		static IndexedVertices<Vertex> Create()
		{
			return CreateTesselated<Vertex>(12, 24);
		}
	};
}
