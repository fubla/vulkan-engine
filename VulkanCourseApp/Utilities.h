#pragma once

#include <fstream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

const int MAX_FRAME_DRAWS = 2;
const int MAX_OBJECTS = 2;

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

static uint32_t findMemoryTypeIndex(VkPhysicalDevice physicalDevice, uint32_t allowedTypes, VkMemoryPropertyFlags properties)
{
	// Get properties of physical device memory
	VkPhysicalDeviceMemoryProperties memoryProperties;
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++)
	{
		if ((allowedTypes & (1 << i))														// Index of memory type must match corresponding bit in allowedTypes
			&& (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)	// Desired property bit flags are part of memory type's property flags		
		{
			// This memory type is valid, return its index
			return i;
		}
	}
}

static void createBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsageFlags,
	VkMemoryPropertyFlags bufferPropertyFlags, VkBuffer *buffer, VkDeviceMemory *bufferMemory)
{
	// CREATE VERTEX BUFFER
	// Information to create buffer (doesn't include assigning memory)
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = bufferSize;									// Size of buffer
	bufferCreateInfo.usage = bufferUsageFlags;							// Multiple types of buffer possible
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Similar to Swapchain images, can share vertex buffers

	VkResult result = vkCreateBuffer(device, &bufferCreateInfo, nullptr, buffer);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Vertex Buffer!");
	}

	// GET BUFFER MEMORY REQUIREMENTS
	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(device, *buffer, &memoryRequirements);

	// ALLOCATE MEMORY TO BUFFER
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(physicalDevice, memoryRequirements.memoryTypeBits, 
		bufferPropertyFlags);																					// VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT	: CPU can interact w/ memory
																												// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT : Allows placement of data straight into buffer after mapping (otherwise would have to specify manually)
	// Allocate memory to VkDeviceMemory
	result = vkAllocateMemory(device, &memoryAllocateInfo, nullptr, bufferMemory);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Vertex Buffer Memory!");
	}

	// Allocate memory to given Vertex Buffer
	vkBindBufferMemory(device, *buffer, *bufferMemory, 0);
}

static void copyBuffer(VkDevice device, VkQueue transferQueue, VkCommandPool transferCommandPool,
	VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize bufferSize)
{
	// Command buffer to hold transfer commands
	VkCommandBuffer transferCommandBuffer;

	// Command buffer details
	VkCommandBufferAllocateInfo allocateInfo = {};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandPool = transferCommandPool;
	allocateInfo.commandBufferCount = 1;

	// Allocate command buffer from pool
	vkAllocateCommandBuffers(device, &allocateInfo, &transferCommandBuffer);

	// Information to begin the command buffer record
	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;	// We're only using the command buffer once, so set up for one type submit

	// Begin recording transfer commands
	vkBeginCommandBuffer(transferCommandBuffer, &beginInfo);

	// Region of data to copy from and to
	VkBufferCopy bufferCopyRegion = {};
	bufferCopyRegion.srcOffset = 0;
	bufferCopyRegion.dstOffset = 0;
	bufferCopyRegion.size = bufferSize;

	// Command to copy src buffer to dst buffer
	vkCmdCopyBuffer(transferCommandBuffer, srcBuffer, dstBuffer, 1, &bufferCopyRegion);

	// End commands
	vkEndCommandBuffer(transferCommandBuffer);

	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &transferCommandBuffer;

	// Submit transfer command to transfer queue and wait until it finishes
	vkQueueSubmit(transferQueue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(transferQueue);

	// Free temporary command buffer back to pool
	vkFreeCommandBuffers(device, transferCommandPool, 1, &transferCommandBuffer);
}