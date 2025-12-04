#pragma once

#include <types.h>

namespace trayser
{

struct GBuffer
{
	void Init(u32 width, u32 height);

	AllocatedImage colorImage;
	AllocatedImage depthImage;
};

} // namespace trayser