#pragma once

#include <types.h>

namespace trayser
{

struct GBuffer
{
	AllocatedImage colorBuffer;
	AllocatedImage depthBuffer;
};

} // namespace trayser