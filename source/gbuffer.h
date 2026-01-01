#pragma once

#include <device.h>
#include <types.h>

namespace trayser
{

struct GBuffer
{
	void Init(u32 width, u32 height);

	Device::Image colorImage;
	VkImageView colorView;
	Device::Image accumulatorImage;
	VkImageView accumulatorView;
	Device::Image depthImage;
	VkImageView depthView;
	VkExtent2D extent;

	//AllocatedImage colorImage;
	//AllocatedImage accumulatorImage; // TODO TEMP
	//AllocatedImage depthImage;
};

} // namespace trayser