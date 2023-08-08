#pragma once

#include <fstream>

#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;

const std::vector<const char *> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// Vertex data representation
struct Vertex
{
	glm::vec3 pos;	// Vertex position
	glm::vec3 col;	// Vertex color (r, g, b)
};

// Indices (locations) of Queue Families (if they exist at all)
struct QueueFamilyIndices
{
	int graphicsFamily = -1;		// Location of Graphics Queue Family
	int presentationFamily  = -1;	// Location of Presentation Queue Family
	// Check if queue families are valid
	bool isValid()
	{
		return graphicsFamily >= 0 &&  presentationFamily >= 0;
	}
};

struct SwapChainDetails
{
	VkSurfaceCapabilitiesKHR surfaceCapabilities;		// Surface properties. e.g. image size/extent
	std::vector<VkSurfaceFormatKHR> formats;			// Surface image formats, e.g. RGBA, bit depth etc.
	std::vector<VkPresentModeKHR> presentationModes;	// How images should be presented to the screen
};

struct SwapchainImage
{
	VkImage image;
	VkImageView imageView;
};

static std::vector<char> readFile(const std::string &filename)
{
	// Open stream from given file
	// std::ios:ate tells stream to start reading from the end of file
	std::ifstream file(filename, std::ios::binary | std::ios::ate);

	// Check if file stream successfully opened
	if(!file.is_open())
	{
		throw std::runtime_error("Failed to open a file!");
	}

	// Get current read position -> file size
	size_t fileSize = static_cast<size_t>(file.tellg());
	std::vector<char> fileBuffer(fileSize);
	
	// Go to position 0 of file
	file.seekg(0);

	file.read(fileBuffer.data(), fileSize);

	file.close();

	return fileBuffer;
}