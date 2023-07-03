#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include<stdexcept>
#include <vector>
#include "Utilities.h"

class VulkanRenderer
{
public:
	VulkanRenderer();

	int init(GLFWwindow *newWindow);
	void cleanup();
	~VulkanRenderer();

private:
	GLFWwindow * window;

	const std::vector<const char*> validationLayers =
	{
		"VK_LAYER_KHRONOS_validation"
	};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	// Vulkan components
	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;

	struct
	{
		VkPhysicalDevice physicalDevice;
		VkDevice logicalDevice;
	} mainDevice;
	VkQueue graphicsQueue;

	// Vulkan functions
	// - Create functions
	void createInstance();
	void createLogicalDevice();

	// - Get Functions
	void getPhysicalDevice();

	// - Support functions
	// -- Checker functions
	bool checkInstanceExtensionSupport(std::vector<const char*> *checkExtensions);
	bool checkDeviceSuitable(VkPhysicalDevice device);
	bool checkValidationLayerSupport();

	// -- Getter functions
	QueueFamilyIndices getQueueFamilies(VkPhysicalDevice device);

	// -- Debug functions
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType,
		const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);
	void setupDebugMessenger();
	VkResult createDebugUtilsMessengerExt(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger);
	void destroyDebugUtilsMessengerExt(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator);
};

