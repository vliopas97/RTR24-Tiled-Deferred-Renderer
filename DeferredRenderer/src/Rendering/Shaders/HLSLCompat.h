#ifndef HLSLCOMPAT_H
#define HLSLCOMPAT_H

#ifdef HLSL
typedef float2 vec2;
typedef float3 vec3;
typedef float4 vec4;
typedef float4x4 mat4x4;
typedef uint UINT;
typedef uint2 uvec2;
typedef uint3 uvec3;
typedef uint4 uvec4;
typedef int BOOL;

#define ALIGNAS(x)
#define constexpr const

#else

#define ALIGNAS(x) alignas(x)
using namespace glm;

#endif

struct PipelineConstants
{
	vec3 CameraPosition;
	ALIGNAS(16) mat4x4 View;
	mat4x4 ViewProjection;
	mat4x4 Projection;
	BOOL SSAOEnabled;
	float RadiusSSAO;
	float IntensitySSAO;
};

struct ALIGNAS(16) MaterialData
{
	float3 MatericalColor;
	float SpecularIntensity;
	float Shininess;
};

struct ActorData
{
	mat4x4 Model;
	mat4x4 ModelView;
	ALIGNAS(16) int KdID;
	int KnID;
	int KsID;
	ALIGNAS(16) MaterialData Material;
};

struct DirLightData
{
	vec3 Position;
	ALIGNAS(16) vec3 Direction;
	ALIGNAS(16) vec3 Ambient;
	ALIGNAS(16) vec3 DiffuseColor;
	float DiffuseIntensity;
};

struct BlurPassControls
{
	BOOL IsHorizontal;
};

static constexpr uint MaxRadius = 16;
struct Kernel
{
	float Coeffs[2 * MaxRadius + 1];
};

#endif // HLSLCOMPAT_H