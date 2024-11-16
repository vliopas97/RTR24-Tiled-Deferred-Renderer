#pragma once
#include "Core/Core.h"
#include "Actor.h"

class Model : public Actor
{
public:
	Model(ID3D12Device5Ptr device, const class Camera& camera);

};

