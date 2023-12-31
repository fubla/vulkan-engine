#include "VulkanRenderer.h"
#include <iostream>

VulkanRenderer::VulkanRenderer()
{}

int VulkanRenderer::init(GLFWwindow * newWindow)
{
	window = newWindow;

	try
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		getPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createColorBufferImage();
		createDepthBufferImage();
		createRenderPass();
		createDescriptorSetLayout();
		createPushConstantRange();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createTextureSampler();
		//allocateDynamicBufferTransferSpace();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createInputDescriptorSets();
		createSynchronization();

		uboViewProjection.projection = glm::perspective(glm::radians(45.0f), static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height), 0.1f, 100.0f);
		uboViewProjection.view = glm::lookAt(glm::vec3(10.0f, 0.0f, 100.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		uboViewProjection.projection[1][1] *= -1;

		// Create default "no texture" texture 
		createTexture("texture1.jpg");
	}
	catch (const std::runtime_error &e)
	{
		printf("ERROR: %s\n", e.what());
		return EXIT_FAILURE;
	}

	return 0;
}

void VulkanRenderer::updateModel(int modelId, glm::mat4 newModel)
{
	if (modelId >= modelList.size()) return;

	modelList[modelId].setModel(newModel);
}

void VulkanRenderer::draw()
{
	// -- Get next image --

	// Wait for given fence to signal (open) from last draw before continuing
	vkWaitForFences(mainDevice.logicalDevice, 1, &drawFences [currentFrame], VK_TRUE, std::numeric_limits<uint64_t>::max());
	// Manually reset (close) fences
	vkResetFences(mainDevice.logicalDevice, 1, &drawFences [currentFrame]);

	// Get index of next image to be drawn to, and signal semaphore when ready to be drawn to
	uint32_t imageIndex;
	vkAcquireNextImageKHR(mainDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(), imageAvailable [currentFrame], VK_NULL_HANDLE, &imageIndex);

	recordCommands(imageIndex);
	updateUniformBuffers(imageIndex);

	// -- Submit command buffer to render
	// Queue submission information
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &imageAvailable [currentFrame];				// List of semaphores to wait on
	VkPipelineStageFlags waitStages[] = {
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
	};
	submitInfo.pWaitDstStageMask = waitStages;								// Stages to check semaphores at
	submitInfo.commandBufferCount = 1;										// Num of command buffers to submit
	submitInfo.pCommandBuffers = &commandBuffers [imageIndex];				// Command buffer to submit
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &renderFinished [currentFrame];			// Semaphores to signal when command buffer finishes

	// Submit command buffer to queue
	VkResult result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, drawFences [currentFrame]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to submit Command Buffer to Queue!");
	}

	// -- Present rendered image to screen -- 
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &renderFinished [currentFrame];				// Semaphores to wait on
	presentInfo.swapchainCount = 1;								// Num of swapchains to present to
	presentInfo.pSwapchains = &swapchain;						// Swapchains to present images to
	presentInfo.pImageIndices = &imageIndex;					// Index of images to in swapchains to present

	// Present Image!
	result = vkQueuePresentKHR(presentationQueue, &presentInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to present Image!");
	}
	// Get next frame
	currentFrame = (currentFrame + 1) % MAX_FRAME_DRAWS;
}

void VulkanRenderer::cleanup()
{
	// Wait until no actions being run on device before destroying
	vkDeviceWaitIdle(mainDevice.logicalDevice);

	//_aligned_free(modelTransferSpace);

	for (size_t i = 0; i < modelList.size(); i++)
	{
		modelList[i].destroyMeshModel();
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, inputDescriptorPool, nullptr);

	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, inputSetLayout, nullptr);

	vkDestroyDescriptorPool(mainDevice.logicalDevice, samplerDescriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, samplerSetLayout, nullptr);

	vkDestroySampler(mainDevice.logicalDevice, textureSampler, nullptr);
	for (size_t i = 0; i < textureImages.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, textureImageViews[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, textureImages[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, texturesImageMemory[i], nullptr);
	}


	for (size_t i = 0; i < depthBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, depthBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice, depthBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, depthBufferImageMemory[i], nullptr);
	}

	for (size_t i = 0; i < colorBufferImage.size(); i++)
	{
		vkDestroyImageView(mainDevice.logicalDevice, colorBufferImageView[i], nullptr);
		vkDestroyImage(mainDevice.logicalDevice,colorBufferImage[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, colorBufferImageMemory[i], nullptr);
	}

	vkDestroyDescriptorPool(mainDevice.logicalDevice, descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(mainDevice.logicalDevice, descriptorSetLayout, nullptr);
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		vkDestroyBuffer(mainDevice.logicalDevice, vpUniformBuffers[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, vpUniformBufferMemory[i], nullptr);
		
		// LEGACY
		/*vkDestroyBuffer(mainDevice.logicalDevice, modelUniformBuffersDynamic[i], nullptr);
		vkFreeMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[i], nullptr);*/
	}
	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		vkDestroySemaphore(mainDevice.logicalDevice, renderFinished [i], nullptr);
		vkDestroySemaphore(mainDevice.logicalDevice, imageAvailable [i], nullptr);
		vkDestroyFence(mainDevice.logicalDevice, drawFences [i], nullptr);
	}

	vkDestroyCommandPool(mainDevice.logicalDevice, graphicsCommandPool, nullptr);
	for (auto framebuffer : swapchainFramebuffers)
	{
		vkDestroyFramebuffer(mainDevice.logicalDevice, framebuffer, nullptr);
	}
	vkDestroyPipeline(mainDevice.logicalDevice, secondPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, secondPipelineLayout, nullptr);
	vkDestroyPipeline(mainDevice.logicalDevice, graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(mainDevice.logicalDevice, pipelineLayout, nullptr);
	vkDestroyRenderPass(mainDevice.logicalDevice, renderPass, nullptr);
	for (auto image : swapchainImages)
	{
		vkDestroyImageView(mainDevice.logicalDevice, image.imageView, nullptr);
	}
	vkDestroySwapchainKHR(mainDevice.logicalDevice, swapchain, nullptr);
	vkDestroySurfaceKHR(instance, surface, nullptr);
	vkDestroyDevice(mainDevice.logicalDevice, nullptr);
	if (enableValidationLayers)
	{
		destroyDebugUtilsMessengerExt(instance, debugMessenger, nullptr);
	}
	vkDestroyInstance(instance, nullptr);
}


VulkanRenderer::~VulkanRenderer()
{}

void VulkanRenderer::createInstance()
{
	// Information about the application itself
	// Most data here doesn't affect the program and is for developer convenience
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Vulkan App";				// Custom name of the application
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";						// Custom engine name
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);		// Custom engine version
	appInfo.apiVersion = VK_API_VERSION_1_0;				// The Vulkan version
	// Creation information for a vkInstance (Vulkan Instance)
	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	// Create list to hold instance extensions
	std::vector<const char*> instanceExtensions = std::vector<const char*>();
	// Set up extensions Instance will use
	uint32_t glfwExtensionCount = 0;						// GLFW may require multiple extensions
	const char** glfwExtensions;							// Extensions passed as array of cstrings, so need pointer (the array) to pointer (the cstring)

	// Get GLFW extensions
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	// Add GLFW extensions to list of extensions
	for (size_t i = 0; i < glfwExtensionCount; i++)
	{
		instanceExtensions.push_back(glfwExtensions [i]);
	}

	if (enableValidationLayers)
	{
		instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	// Check Instance Extensions supported...
	if (!checkInstanceExtensionSupport(&instanceExtensions))
	{
		throw std::runtime_error("VkInstance does not support required extensions!");
	}

	createInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

	// Set up Validation Layers 
	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		throw std::runtime_error("Validation layers requested, but not available!");
	}
	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
		createInfo.ppEnabledLayerNames = nullptr;
	}


	// Create instance
	VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Vulkan Instance!");
	}
}

void VulkanRenderer::createLogicalDevice()
{
	// Get the queue family indices for the chosen Physical Device
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// Vector for queue creation information and set for family indices (disallows duplicates)
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
	std::set<int> queueFamilyIndices = {indices.graphicsFamily, indices.presentationFamily};
	// Queues the logical device needs to create and info to do so 
	for (int queueFamilyIndex : queueFamilyIndices)
	{
		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = queueFamilyIndex;									// Index of the family to create a queue from
		queueCreateInfo.queueCount = 1;															// Num of queues to create
		float priority = 1.0f;
		queueCreateInfo.pQueuePriorities = &priority;											// Vulkan needs to know how to handle multiple queues	
		queueCreateInfos.push_back(queueCreateInfo);
	}
	// Information to create logical device (sometimes called "device")
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();								// List of queue create infos so device can create required queues
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());	// Number of enabled logical device extensions
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// Physical Device Features the logical device will be using
	VkPhysicalDeviceFeatures deviceFeatures = {};
	if (mainDevice.deviceFeatures.samplerAnisotropy)
	{
		deviceFeatures.samplerAnisotropy = VK_TRUE;												// Enable Anisotropy
	}

	deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
	// Create the logical device for the given physical device
	VkResult result = vkCreateDevice(mainDevice.physicalDevice, &deviceCreateInfo, nullptr, &mainDevice.logicalDevice);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Logical Device!");
	}

	// Queues are created at the same time as the device, so we want handle to queues
	// From given logical device, of given Queue Family, of given Queue Index (0 since only one queue), place reference in given VkQueue
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.graphicsFamily, 0, &graphicsQueue);
	vkGetDeviceQueue(mainDevice.logicalDevice, indices.presentationFamily, 0, &presentationQueue);
}

void VulkanRenderer::createSurface()
{
	// Create Surface (creates a surface create info struct, runs the create surface function, returns result)
	VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a surface!");
	}
}

void VulkanRenderer::createSwapChain()
{
	// Get Swap Chain details so we can pick best settings
	SwapChainDetails swapChainDetails = getSwapChainDetails(mainDevice.physicalDevice);

	// Find optimal surface values for our swap chain

	// - 1. CHOOSE BEST SURFACE FORMAT
	VkSurfaceFormatKHR surfaceFormat = chooseBestSurfaceFormat(swapChainDetails.formats);
	// - 2. CHOOSE BEST PRESENTATION MODE
	VkPresentModeKHR presentMode = chooseBestPresentationMode(swapChainDetails.presentationModes);
	// - 3. CHOOSE SWAP CHAIN IMAGE RESOLUTION
	VkExtent2D extent = chooseSwapExtent(swapChainDetails.surfaceCapabilities);

	// How many images are in the swap chain? Get 1 more than the minimum to allow triple buffering
	uint32_t imageCount = swapChainDetails.surfaceCapabilities.minImageCount + 1;

	// If imageCount higher than max, clamp down to max
	// If 0, then limitless
	if (swapChainDetails.surfaceCapabilities.maxImageCount > 0
		&& imageCount > swapChainDetails.surfaceCapabilities.maxImageCount)
	{
		imageCount = swapChainDetails.surfaceCapabilities.maxImageCount;
	}

	// Creation information for swap chain
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;														// Swapchain surface
	swapchainCreateInfo.imageFormat = surfaceFormat.format;										// Swapchain format
	swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;								// Swapchain color space
	swapchainCreateInfo.presentMode = presentMode;												// Swapchain presentation mode
	swapchainCreateInfo.imageExtent = extent;													// Swapchain image extents
	swapchainCreateInfo.minImageCount = imageCount;												// Minimum images in swapchain
	swapchainCreateInfo.imageArrayLayers = 1;													// Number of layers for each image in chain
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;						// What attachment images will be used as
	swapchainCreateInfo.preTransform = swapChainDetails.surfaceCapabilities.currentTransform;	// Transform to perform on swap chain images
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;						// How to handle blending images with external graphics (e.g. other windows)
	swapchainCreateInfo.clipped = VK_TRUE;														// Whether to clip parts of image not in view (e.g. behin

	// Get Queue Family Indices
	QueueFamilyIndices indices = getQueueFamilies(mainDevice.physicalDevice);

	// If graphics and presentation families are different, then swapchain must let images be shared between families
	if (indices.graphicsFamily != indices.presentationFamily)
	{
		// Queues to share between
		uint32_t queueFamilyIndices[] = {
			static_cast<uint32_t>(indices.graphicsFamily),
			static_cast<uint32_t>(indices.presentationFamily)
		};
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;						// Image share handling
		swapchainCreateInfo.queueFamilyIndexCount = 2;											// Number of queues to share images between
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;							// Array of queues to share between
	}
	else
	{
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchainCreateInfo.queueFamilyIndexCount = 0;
		swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	}

	// If old swap chain been destroyed and this one replaces it, then link old one to quickly hand over responsibilities
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	// Create Swapchain
	VkResult result = vkCreateSwapchainKHR(mainDevice.logicalDevice, &swapchainCreateInfo, nullptr, &swapchain);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a swapchain!");
	}

	// Store for later reference
	swapchainImageFormat = surfaceFormat.format;
	swapchainExtent = extent;

	// Get swapchain images
	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, nullptr);
	std::vector<VkImage> images(swapchainImageCount);
	vkGetSwapchainImagesKHR(mainDevice.logicalDevice, swapchain, &swapchainImageCount, images.data());

	for (VkImage image : images)
	{
		// Store image handle
		SwapchainImage swapchainImage = {};
		swapchainImage.image = image;
		swapchainImage.imageView = createImageView(image, swapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);

		// Add to swapchain image list
		swapchainImages.push_back(swapchainImage);
	}
}

void VulkanRenderer::createRenderPass()
{
	// Array of Subpasses
	std::array<VkSubpassDescription, 2> subpasses {};
	
	// ATTACHMENTS
	// SUBPASS 1 ATTACHMENTS + REFERENCES (INPUT ATTACHMENTS)

	// Color Attachment (Input)
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = colorImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;			
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth Attachment (Input)
	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = depthImageFormat;
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;			
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;		
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;		
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Color Attachment (Input) Reference
	VkAttachmentReference colorAttachmentReference = {};
	colorAttachmentReference.attachment = 1;
	colorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// Depth Attachment (Input) Reference
	VkAttachmentReference depthAttachmentReference = {};
	depthAttachmentReference.attachment = 2;
	depthAttachmentReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	// Set up Subpass 1
	subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[0].colorAttachmentCount = 1;
	subpasses[0].pColorAttachments = &colorAttachmentReference;
	subpasses[0].pDepthStencilAttachment = &depthAttachmentReference;

	// SUBPASS 2 ATTACHMENTS + REFERENCES

	// Swapchain Color attachment
	VkAttachmentDescription swapchainColorAttachment = {};
	swapchainColorAttachment.format = swapchainImageFormat;
	swapchainColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;					// Number of samples to write for multisampling
	swapchainColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;				// Describes what to do with attachment before rendering
	swapchainColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;			// Describes what to do with attachment after rendering
	swapchainColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;	// Describes what to do with stencil before rendering
	swapchainColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;	// Describes what to do with stencil after rendering

	// Framebuffer data will be stored as an image, but images can be given different data layouts
	// to give optimal use for certain operations
	swapchainColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;	// Image data layout before render pass starts
	swapchainColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;	// Image data layout after render pass (to change to)

	// Attachment reference uses an attachment index that refers to index in the attachment list passed to renderPassCreateInfo
	VkAttachmentReference swapchainColorAttachmentReference = {};
	swapchainColorAttachmentReference.attachment = 0;
	swapchainColorAttachmentReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	// References to attachments that subpass will take input from
	std::array<VkAttachmentReference, 2> inputReferences;
	inputReferences[0].attachment = 1;
	inputReferences[0].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	inputReferences[1].attachment = 2;
	inputReferences[1].layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Set up Subpass 2
	subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpasses[1].colorAttachmentCount = 1;
	subpasses[1].pColorAttachments = &swapchainColorAttachmentReference;
	subpasses[1].inputAttachmentCount = static_cast<uint32_t>(inputReferences.size());
	subpasses[1].pInputAttachments = inputReferences.data();

	// SUBPASS DEPENDENCIES

	// Need to determine when layout transitions occur using subpass dependencies
	std::array<VkSubpassDependency, 3> subpassDependencies;

	// Conversion from VK_IMAGE_LAYOUT_UNDEFINED to VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	// Transition must happen after...
	subpassDependencies [0].srcSubpass = VK_SUBPASS_EXTERNAL;					// Subpass index (VK_SUBPASS_EXTERNAL = special value meaning outside of render pass)
	subpassDependencies [0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; // Pipeline stage
	subpassDependencies [0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;			// Stage access mask (memory access)
	// But must happen before...
	subpassDependencies [0].dstSubpass = 0;
	subpassDependencies [0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies [0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	subpassDependencies [0].dependencyFlags = 0;

	// Subpass 1 layout (color/depth) to Subpass 2 layout (shader read)
	subpassDependencies [1].srcSubpass = 0;				
	subpassDependencies [1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; 
	subpassDependencies [1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// But must happen before...
	subpassDependencies [1].dstSubpass = 1;
	subpassDependencies [1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	subpassDependencies [1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	subpassDependencies [1].dependencyFlags = 0;

	// Conversion from VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	// Transition must happen after...
	subpassDependencies [2].srcSubpass = 1;
	subpassDependencies [2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependencies [2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	// But must happen before...
	subpassDependencies [2].dstSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependencies [2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	subpassDependencies [2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	subpassDependencies [2].dependencyFlags = 0;

	std::array<VkAttachmentDescription, 3> renderPassAttachments = {swapchainColorAttachment, colorAttachment, depthAttachment};

	// Create info for render pass
	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(renderPassAttachments.size());
	renderPassCreateInfo.pAttachments = renderPassAttachments.data();
	renderPassCreateInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
	renderPassCreateInfo.pSubpasses = subpasses.data();
	renderPassCreateInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
	renderPassCreateInfo.pDependencies = subpassDependencies.data();

	VkResult result = vkCreateRenderPass(mainDevice.logicalDevice, &renderPassCreateInfo, nullptr, &renderPass);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Render Pass!");
	}
}

void VulkanRenderer::createDescriptorSetLayout()
{
	// UNIFORM VALUES DESCRIPTOR SET LAYOUT
	// UboViewProjection Binding Info
	VkDescriptorSetLayoutBinding vpLayoutBinding = {};
	vpLayoutBinding.binding = 0;													// Binding point in shader (designated by binding number in shader)
	vpLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;				// Type of descriptor (uniform, dynamic uniform, image sampler, etc.)
	vpLayoutBinding.descriptorCount = 1;											// Number of descriptors for binding
	vpLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;						// Shader stage to bind to
	vpLayoutBinding.pImmutableSamplers = nullptr;									// For Texture: Can make sampler data immutable (ImageView it samples from can still be changed!) by specifying in layout

	// LEGACY
	// Model Binding Info
	/*VkDescriptorSetLayoutBinding modelLayoutBinding = {};
	modelLayoutBinding.binding = 1;
	modelLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;	
	modelLayoutBinding.descriptorCount = 1;										
	modelLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;					
	modelLayoutBinding.pImmutableSamplers = nullptr;	*/							

	std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {vpLayoutBinding};
	// LEGACY
	//std::vector<VkDescriptorSetLayoutBinding> layoutBindings = {vpLayoutBinding, modelLayoutBinding};

	// Create Descriptor Set Layout with given bindings
	VkDescriptorSetLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutCreateInfo.bindingCount = static_cast<uint32_t>(layoutBindings.size());	// Num of binding infos
	layoutCreateInfo.pBindings = layoutBindings.data();								// Array of binding infos

	// Create Descriptor Set Layout
	VkResult result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &layoutCreateInfo, nullptr, &descriptorSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Descriptor Set Layout!");
	}

	// CREATE TEXTURE SAMPLER DESCRIPTOR SET LAYOUT
	// Texture Binding Info
	VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
	samplerLayoutBinding.binding = 0;
	samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerLayoutBinding.descriptorCount = 1;
	samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	samplerLayoutBinding.pImmutableSamplers = nullptr;

	// Create a Descriptor Set Layout with given bindings for texture
	VkDescriptorSetLayoutCreateInfo textureLayoutCreateInfo = {};
	textureLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	textureLayoutCreateInfo.bindingCount = 1;
	textureLayoutCreateInfo.pBindings = &samplerLayoutBinding;

	// Create Descriptor Set Layout
	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &textureLayoutCreateInfo, nullptr, &samplerSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Descriptor Set Layout!");
	}

	// CREATE INPUT ATTACHMENT IMAGE DESCRIPTOR SET LAYOUT
	// Color input binding
	VkDescriptorSetLayoutBinding colorInputLayoutBinding = {};
	colorInputLayoutBinding.binding = 0;
	colorInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputLayoutBinding.descriptorCount = 1;
	colorInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	colorInputLayoutBinding.pImmutableSamplers = nullptr;

	// Depth input binding
	VkDescriptorSetLayoutBinding depthInputLayoutBinding = {};
	depthInputLayoutBinding.binding = 1;
	depthInputLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputLayoutBinding.descriptorCount = 1;
	depthInputLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	depthInputLayoutBinding.pImmutableSamplers = nullptr;

	// Array of input attachment bindings
	std::vector<VkDescriptorSetLayoutBinding> inputBindings = {colorInputLayoutBinding, depthInputLayoutBinding};

	// Create a descriptor set layout for input attachments
	VkDescriptorSetLayoutCreateInfo inputLayoutCreateInfo = {};
	inputLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	inputLayoutCreateInfo.bindingCount = static_cast<uint32_t>(inputBindings.size());
	inputLayoutCreateInfo.pBindings = inputBindings.data();

	// Create Descriptor Set Layout
	result = vkCreateDescriptorSetLayout(mainDevice.logicalDevice, &inputLayoutCreateInfo, nullptr, &inputSetLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Input Descriptor Set Layout!");
	}
}

void VulkanRenderer::createPushConstantRange()
{
	// Define push constant values (no 'create' needed)
	pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pushConstantRange.offset = 0;												// Offset into given data to pass to push constant	
	pushConstantRange.size = sizeof(Model);										// Size of data being passed
}

void VulkanRenderer::createGraphicsPipeline()
{
	auto vertexShaderCode = readFile("Shaders/vert.spv");
	auto fragmentShaderCode = readFile("Shaders/frag.spv");

	VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
	VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

	// -- Shader stage creation information
	// Vertex Stage creation information
	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;				// Shader stage name
	vertexShaderStageCreateInfo.module = vertexShaderModule;					// Shader module to use at stage
	vertexShaderStageCreateInfo.pName = "main";									// Entry point to shader

	// Fragment Stage creation information
	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;				// Shader stage name
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;					// Shader module to use at stage
	fragmentShaderStageCreateInfo.pName = "main";									// Entry point to shader

	// Graphics pipeline creation info requires array of shader stage creates
	VkPipelineShaderStageCreateInfo shaderStages[] = {vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo};

	// How the data for a single vertex (including info such as position, colour, texture coords, normals, etc) is as a whole
	VkVertexInputBindingDescription bindingDescription = {};
	bindingDescription.binding = 0;											// Can bind multiple streams of data, this defines which one
	bindingDescription.stride = sizeof(Vertex);								// Size of a single vertex object
	bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;				// How to move between data after each vertex
																			// VK_VERTEX_INPUT_RATE_VERTEX		: Move on to the next vertex
																			// VK_VERTEX_INPUT_RATE_INSTANCE	: Move to a vertex for the next instance
	// How the data for an attribute is defined within a vertex
	std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

	// Position Attribute
	attributeDescriptions[0].binding = 0;									// Which binding the data is at (should be same as above)
	attributeDescriptions[0].location = 0;									// Location in shader where data will be read from
	attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;			// Format the data will take (also helps define size of data) NOTE: if you choose the wrong one it fucks up your vertices :D (accidentally used with VK_FORMAT_R32G32B32A32_SFLOAT...)
	attributeDescriptions[0].offset = offsetof(Vertex, pos);				// Where this attribute is defined in the data for a single vertex

	// Color Attribute
	attributeDescriptions[1].binding = 0;									
	attributeDescriptions[1].location = 1;									
	attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;			
	attributeDescriptions[1].offset = offsetof(Vertex, col);

	// Texture Attribute
	attributeDescriptions[2].binding = 0;									
	attributeDescriptions[2].location = 2;									
	attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
	attributeDescriptions[2].offset = offsetof(Vertex, tex);

	// -- Vertex input --
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &bindingDescription;			// List of vertex binding descriptions (data spacing/stride information etc.)
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = attributeDescriptions.data();	// List of vertex attribute descriptions (data format and where to bind to/from)

	// -- Input Assembly -- 
	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;			// Primitive type to assemble vertices as
	inputAssembly.primitiveRestartEnable = VK_FALSE;						// Allow overriding of "strip" topology to start new primitive

	// -- Viewport & scissor -- 
	VkViewport viewport = {};
	viewport.x = 0.0f;														// x start coordinate
	viewport.y = 0.0f;
	viewport.width = static_cast<float>(swapchainExtent.width);				// width of viewport
	viewport.height = static_cast<float>(swapchainExtent.height);
	viewport.minDepth = 0.0f;												// min framebuffer depth		
	viewport.maxDepth = 1.0f;												// min framebuffer depth

	VkRect2D scissor = {};
	scissor.offset = {0,0};													// offset to use region from
	scissor.extent = swapchainExtent;										// extent to describe region to use, starting at offset

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	//// -- Dynamic state -- 
	//std::vector<VkDynamicState> dynamicStatEnables;
	//dynamicStatEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);				// Dynamic viewport: Can resize in command buffer with vkCmdSetViewport(commandbuffer, 0, 1, &viewport);
	//dynamicStatEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);					// Dynamic scissor: can resize in command buffer with vkCmdSetScissor(commandbuffer, 0, 1, &scissor);

	//// Dynamic state creation info 
	//VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	//dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	//dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStatEnables.size());
	//dynamicStateCreateInfo.pDynamicStates = dynamicStatEnables.data();

	// -- Rasterizer -- 
	VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo = {};
	rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizerCreateInfo.depthClampEnable = VK_FALSE;						// Change whether fragments beyond near/far planes are clipped (default) or clamped to plane. REQUIRES DEVICE FEATURE! (depthClamp)
	rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;				// Whether to discare data and skip rasterizer. Never creates fragments, only suitable for ppipeline without framebuffer output
	rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;				// How to handle filling points between vertices
	rasterizerCreateInfo.lineWidth = 1.0f;									// If anything other than 1 needs another GPU feature (wide lines)
	rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;					// Which face of a face to cull
	rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;		// Winding to determine which side is front - Vertices in counter-clockwise order
	rasterizerCreateInfo.depthBiasEnable = VK_FALSE;						// Whether to add depth bias to fragments (good for preventing "shadow acne")

	// -- Multisampling --
	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;				// Enable multisampling or not
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;// Number of samples to use per fragment

	// -- Blending -- 
	// Blending decides how to blend a new color being written to a fragment, with the old value

	// Blend attachment state (how blending is handled)
	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;	// Colors to apply blending to
	colorBlendAttachmentState.blendEnable = VK_TRUE;						// Enable blending

	// Blending uses equation: (srcColorFactor * new color) colorBlendOp (dstBlendFactor * old color)
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;

	// Summarised: (VK_BLEND_FACTOR_SRC_ALPHA * new color) + (VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA * old color)
	//				(new color alpha * new color) + ((1 - new color alpha) * old color)
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

	// Summarised: (1 * new alpha) + (0 * old alpha) = new alpha

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;						// Alternative to calculations is to use logical operations
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	// -- Pipeline layout
	std::array<VkDescriptorSetLayout, 2> descriptorSetLayouts =  {descriptorSetLayout, samplerSetLayout};

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
	pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	// Create pipeline layout
	VkResult result = vkCreatePipelineLayout(mainDevice.logicalDevice, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Pipeline Layout!");
	}

	// -- Depth stencil testing --
	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreate = {};
	depthStencilStateCreate.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreate.depthTestEnable = VK_TRUE;
	depthStencilStateCreate.depthWriteEnable = VK_TRUE;						// Enables updating of of the depth buffer values
	depthStencilStateCreate.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilStateCreate.depthBoundsTestEnable = VK_FALSE;				// Does the depth value exist between two bounds?
	depthStencilStateCreate.stencilTestEnable = VK_FALSE;

	// -- Graphics Pipeline Creation --
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.stageCount = 2;													// Number of shader stages
	pipelineCreateInfo.pStages = shaderStages;											// List of shader stages
	pipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;					// All the fixed function pipeline stages
	pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
	pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	pipelineCreateInfo.pDynamicState = nullptr;
	pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
	pipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	pipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	pipelineCreateInfo.pDepthStencilState = &depthStencilStateCreate;
	pipelineCreateInfo.layout = pipelineLayout;											// Pipeline Layout the pipeline should use
	pipelineCreateInfo.renderPass = renderPass;											// Render pass description the pipeline is compatible with
	pipelineCreateInfo.subpass = 0;														// Subpass of render pass to use with pipeline

	// Pipeline derivatives : can create multiple pipelines that derive from one another for optimization
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;								// existing pipeline to derive from...
	pipelineCreateInfo.basePipelineIndex = -1;											// OR index of pipeline being created to derive from (in case of creating multiple at once) 

	// Create graphics pipeline
	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Graphics Pipeline!");
	}
	// Destroy shader modules, no longer needed after pipeline created
	vkDestroyShaderModule(mainDevice.logicalDevice, fragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, vertexShaderModule, nullptr);

	// CREATE SECOND PASS PIPELINE
	// Second pass shaders
	auto secondVertexShaderCode = readFile("Shaders/second_vert.spv");
	auto secondFragmentShaderCode = readFile("Shaders/second_frag.spv");

	// Build shaders
	VkShaderModule secondVertexShaderModule = createShaderModule(secondVertexShaderCode);
	VkShaderModule secondFragmentShaderModule = createShaderModule(secondFragmentShaderCode);

	// Set new shaders
	vertexShaderStageCreateInfo.module = secondVertexShaderModule;
	fragmentShaderStageCreateInfo.module = secondFragmentShaderModule;

	VkPipelineShaderStageCreateInfo secondShaderStages[] = {vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo};

	// No vertex data for second pass
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	// Don't want to write to depth buffer
	depthStencilStateCreate.depthWriteEnable = VK_FALSE;


	// Create new pipeline layput
	VkPipelineLayoutCreateInfo secondPipelineLayoutCreateInfo = {};
	secondPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	secondPipelineLayoutCreateInfo.setLayoutCount = 1;
	secondPipelineLayoutCreateInfo.pSetLayouts = &inputSetLayout;
	secondPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	secondPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;

	result = vkCreatePipelineLayout(mainDevice.logicalDevice, &secondPipelineLayoutCreateInfo, nullptr, &secondPipelineLayout);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a second Pipeline Layout!");
	}

	pipelineCreateInfo.pStages = secondShaderStages;				// Update second shader stage list
	pipelineCreateInfo.layout = secondPipelineLayout;				// Create pipeline layout for input attachment descriptor sets
	pipelineCreateInfo.subpass = 1;									// Use second subpass

	result = vkCreateGraphicsPipelines(mainDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &secondPipeline);
	if(result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a second Graphics Pipeline!");
	}

	// Destroy second shader modules
	vkDestroyShaderModule(mainDevice.logicalDevice, secondFragmentShaderModule, nullptr);
	vkDestroyShaderModule(mainDevice.logicalDevice, secondVertexShaderModule, nullptr);
}

void VulkanRenderer::createColorBufferImage()
{
	colorBufferImage.resize(swapchainImages.size());
	colorBufferImageMemory.resize(swapchainImages.size());
	colorBufferImageView.resize(swapchainImages.size());

	// Get supported format for color attachment
	colorImageFormat = chooseSupportedFormat(
		{ VK_FORMAT_R8G8B8A8_UNORM},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT
	);
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		colorBufferImage[i] = createImage(swapchainExtent.width, swapchainExtent.height, colorImageFormat, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &colorBufferImageMemory[i]);
		colorBufferImageView[i] = createImageView(colorBufferImage[i], colorImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void VulkanRenderer::createDepthBufferImage()
{	
	depthBufferImage.resize(swapchainImages.size());
	depthBufferImageMemory.resize(swapchainImages.size());
	depthBufferImageView.resize(swapchainImages.size());

	// Get supported format for depth buffer
	depthImageFormat = chooseSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT},
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		depthBufferImage[i] = createImage(swapchainExtent.width, swapchainExtent.height, depthImageFormat, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &depthBufferImageMemory[i]);
		depthBufferImageView[i] = createImageView(depthBufferImage[i], depthImageFormat, VK_IMAGE_ASPECT_DEPTH_BIT);
	}
	
}

void VulkanRenderer::createFramebuffers()
{
	// One framebuffer for each image in swapchain
	swapchainFramebuffers.resize(swapchainImages.size());

	for (size_t i = 0; i < swapchainFramebuffers.size(); i++)
	{
		std::array<VkImageView, 3> attachments = {
			swapchainImages[i].imageView,
			colorBufferImageView[i],
			depthBufferImageView[i]
		};

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;										// Render Pass layout the Framebuffer will be used with
		framebufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferCreateInfo.pAttachments = attachments.data();							// List of attachments (1:1 with Render pass)
		framebufferCreateInfo.width = swapchainExtent.width;
		framebufferCreateInfo.height = swapchainExtent.height;
		framebufferCreateInfo.layers = 1;

		VkResult result = vkCreateFramebuffer(mainDevice.logicalDevice, &framebufferCreateInfo, nullptr, &swapchainFramebuffers [i]);
		if (result != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a framebuffer!");
		}
	}
}

void VulkanRenderer::createCommandPool()
{
	// Get indices of queue family from device
	QueueFamilyIndices queueFamilyIndices = getQueueFamilies(mainDevice.physicalDevice);

	VkCommandPoolCreateInfo poolInfo = {};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily; // Queue family type that buffers from this command pool will use

	// Create a graphics queue family command pool
	VkResult result = vkCreateCommandPool(mainDevice.logicalDevice, &poolInfo, nullptr, &graphicsCommandPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Command Pool!");
	}
}

void VulkanRenderer::createCommandBuffers()
{
	// One command buffer for each framebuffer
	commandBuffers.resize(swapchainFramebuffers.size());

	VkCommandBufferAllocateInfo cbAllocInfo = {};
	cbAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cbAllocInfo.commandPool = graphicsCommandPool;
	cbAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;			// VK_COMMAND_BUFFER_LEVEL_PRIMARY		: Buffer you submit directly to queue. Can't be called by other buffers.
																	// VK_COMMAND_BUFFER_LEVEL_SECONDARY	: Buffer can't be called directly. Can be called from other buffers via "vkCmdExecuteCommands" when recording commands in primary buffer.
	cbAllocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

	// Allocate command buffers and place handles in array of buffers
	VkResult result = vkAllocateCommandBuffers(mainDevice.logicalDevice, &cbAllocInfo, commandBuffers.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create Command Buffers!");
	}
}

void VulkanRenderer::createSynchronization()
{
	imageAvailable.resize(MAX_FRAME_DRAWS);
	renderFinished.resize(MAX_FRAME_DRAWS);
	drawFences.resize(MAX_FRAME_DRAWS);

	// Semaphore creation information
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	// Fence creation information
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	for (size_t i = 0; i < MAX_FRAME_DRAWS; i++)
	{
		if (vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &imageAvailable [i]) != VK_SUCCESS ||
			vkCreateSemaphore(mainDevice.logicalDevice, &semaphoreCreateInfo, nullptr, &renderFinished [i]) != VK_SUCCESS ||
			vkCreateFence(mainDevice.logicalDevice, &fenceCreateInfo, nullptr, &drawFences [i]) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create a Semaphore and/or Fence!");
		}
	}

}

void VulkanRenderer::createTextureSampler()
{
	// Check device features for anisotropy support
	VkBool32 enableAnisotropy = VK_FALSE;
	if (mainDevice.deviceFeatures.samplerAnisotropy)
	{
		enableAnisotropy = VK_TRUE;
	}

	// Sampler Creation Info
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;							// How to render when image is magnified on screen (VK_FILTER_LINEAR is linear interpolation between texels)
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;							// How to render when image is minified
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;		// How to handle texture wrap in U direction
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;		// How to handle texture wrap in V direction
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;		// How to handle texture wrap in W direction
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;		// Border beyond texture (only works for border clamp)
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;					// False means to use normalized texture coordinates (between 0 and 1)
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;			// Mipmap interpolation mode
	samplerCreateInfo.mipLodBias = 0.0f;									// Level of detail bias for mip level
	samplerCreateInfo.minLod = 0.0f;										// Minimum level of detail to pick mip level
	samplerCreateInfo.maxLod = 0.0f;										// Maximum level of detail to pick mip level
	samplerCreateInfo.anisotropyEnable = enableAnisotropy;					// Enable anisotropic filtering
	samplerCreateInfo.maxAnisotropy = 16;									// Anisotropy sample level

	VkResult result = vkCreateSampler(mainDevice.logicalDevice, &samplerCreateInfo, nullptr, &textureSampler);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Texture Sampler!");
	}
}

void VulkanRenderer::createUniformBuffers()
{
	// ViewProjection buffer size
	VkDeviceSize vpBufferSize = sizeof(UboViewProjection);

	// LEGACY
	// Model buffer size
	//VkDeviceSize modelBufferSize = modelUniformAlignment * MAX_OBJECTS; 

	// One uniform buffer for each image (and by extension, command buffer)
	vpUniformBuffers.resize(swapchainImages.size());
	vpUniformBufferMemory.resize(swapchainImages.size());

	// LEGACY
	/*modelUniformBuffersDynamic.resize(swapchainImages.size());
	modelUniformBufferMemoryDynamic.resize(swapchainImages.size());*/

	// Create uniform buffers
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, vpBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &vpUniformBuffers[i], &vpUniformBufferMemory[i]);
		
		// LEGACY
		/*createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, modelBufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, 
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &modelUniformBuffersDynamic[i], &modelUniformBufferMemoryDynamic[i]);*/
	}
}

void VulkanRenderer::createDescriptorPool()
{
	// CREATE UNIFORM DESCRIPTOR POOL
	// Type of descriptors + how many DESCRIPTORS, not Descriptor Sets (combined makes the pool size)
	// ViewProjection Pool
	VkDescriptorPoolSize vpPoolSize = {};
	vpPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	vpPoolSize.descriptorCount = static_cast<uint32_t>(vpUniformBuffers.size());

	// LEGACY - for reference
	// Model Pool (DYNAMIC)
	/*VkDescriptorPoolSize modelPoolSize = {};
	modelPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	modelPoolSize.descriptorCount = static_cast<uint32_t>(modelUniformBuffersDynamic.size());*/

	// List of Pool Sizes
	std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {vpPoolSize};
	// LEGACY - for reference
	//std::vector<VkDescriptorPoolSize> descriptorPoolSizes = {vpPoolSize, modelPoolSize};

	// Data to create Descriptor Pool
	VkDescriptorPoolCreateInfo poolCreateInfo = {};
	poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCreateInfo.maxSets = static_cast<uint32_t>(swapchainImages.size());		// Maximum number of Descriptor Sets that can be created from pool
	poolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());											// Amount of Pool Sizes being passed
	poolCreateInfo.pPoolSizes = descriptorPoolSizes.data();										// Pool Sizes to create Pool with

	// Create Descriptor Pool
	VkResult result = vkCreateDescriptorPool(mainDevice.logicalDevice, &poolCreateInfo, nullptr, &descriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Descriptor Pool!");
	}

	// CREATE SAMPLER DESCRIPTOR POOL
	// Texture sampler pool
	VkDescriptorPoolSize samplerPoolSize = {};
	samplerPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	samplerPoolSize.descriptorCount = MAX_OBJECTS;

	VkDescriptorPoolCreateInfo samplerPoolCreateInfo = {};
	samplerPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	samplerPoolCreateInfo.maxSets = MAX_OBJECTS;
	samplerPoolCreateInfo.poolSizeCount = 1;
	samplerPoolCreateInfo.pPoolSizes = &samplerPoolSize;

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &samplerPoolCreateInfo, nullptr, &samplerDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create a Descriptor Pool!");
	}

	// CREATE INPUT ATTACHMENT DESCRIPTOR POOL
	// Color attachment pool size
	VkDescriptorPoolSize colorInputPoolSize = {};
	colorInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	colorInputPoolSize.descriptorCount = static_cast<uint32_t>(colorBufferImageView.size());

	// Depth attachment pool size
	VkDescriptorPoolSize depthInputPoolSize = {};
	depthInputPoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
	depthInputPoolSize.descriptorCount = static_cast<uint32_t>(depthBufferImageView.size());

	std::vector<VkDescriptorPoolSize> inputPoolSizes = {colorInputPoolSize, depthInputPoolSize};

	// Create input attachment pool
	VkDescriptorPoolCreateInfo inputPoolCreateInfo = {};
	inputPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	inputPoolCreateInfo.maxSets = swapchainImages.size();
	inputPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(inputPoolSizes.size());
	inputPoolCreateInfo.pPoolSizes = inputPoolSizes.data();

	result = vkCreateDescriptorPool(mainDevice.logicalDevice, &inputPoolCreateInfo, nullptr, &inputDescriptorPool);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Input Descriptor Pool!");
	}
}

void VulkanRenderer::createDescriptorSets()
{
	// Resize Descriptor Set list so one for every buffer
	descriptorSets.resize(swapchainImages.size());

	std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), descriptorSetLayout);

	// Description Set Allocation Info
	VkDescriptorSetAllocateInfo setAllocateInfo = {};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = descriptorPool;									// Pool to allocate Descriptor Set from
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());	// Number of sets to allocate
	setAllocateInfo.pSetLayouts = setLayouts.data();									// Layouts to use to allocate set (1:1 relationship)

	// Allocate Descriptor Sets (multiple)
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocateInfo, descriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Descriptor Sets!");
	}

	// Update all of descriptor set bindings
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		// VIEW PROJECTION DESCRIPTOR
		// Buffer info and data offset info
		VkDescriptorBufferInfo vpBufferInfo = {};
		vpBufferInfo.buffer = vpUniformBuffers[i];										// Buffer to get data from
		vpBufferInfo.offset = 0;														// Position of start of data
		vpBufferInfo.range = sizeof(UboViewProjection);									// Size of data

		// Data about connection between binding and buffer
		VkWriteDescriptorSet vpSetWrite = {};
		vpSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		vpSetWrite.dstSet = descriptorSets[i];											// Descriptor set to update
		vpSetWrite.dstBinding = 0;														// Binding to update (matches with binding on layout/shader)
		vpSetWrite.dstArrayElement = 0;													// Index in array to update
		vpSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;					// Type of descriptor
		vpSetWrite.descriptorCount = 1;													// Amount to update
		vpSetWrite.pBufferInfo = &vpBufferInfo;											// Information about buffer data to bind

		// LEGACY - for reference
		// MODEL DESCRIPTOR
		// Model Buffer Binding Info
		/*VkDescriptorBufferInfo modelBufferinfo = {};
		modelBufferinfo.buffer = modelUniformBuffersDynamic[i];
		modelBufferinfo.offset = 0;
		modelBufferinfo.range = modelUniformAlignment;

		VkWriteDescriptorSet modelSetWrite = {};
		modelSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		modelSetWrite.dstSet = descriptorSets[i];
		modelSetWrite.dstBinding = 1;
		modelSetWrite.dstArrayElement = 0;
		modelSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		modelSetWrite.descriptorCount = 1;
		modelSetWrite.pBufferInfo = &modelBufferinfo;
*/
		// List of Descriptor Set Writes
		std::vector<VkWriteDescriptorSet> setWrites = {vpSetWrite};
		// FOR REFERENCE
		//std::vector<VkWriteDescriptorSet> setWrites = {vpSetWrite, modelSetWrite};

		// Update the descriptor sets with new buffer/binding info
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 
			0, nullptr);
	}
}

void VulkanRenderer::createInputDescriptorSets()
{
	inputDescriptorSets.resize(swapchainImages.size());

	// Fill array of layouts ready for set creation
	std::vector<VkDescriptorSetLayout> setLayouts(swapchainImages.size(), inputSetLayout);

	VkDescriptorSetAllocateInfo setAllocateInfo = {};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = inputDescriptorPool;
	setAllocateInfo.descriptorSetCount = static_cast<uint32_t>(swapchainImages.size());
	setAllocateInfo.pSetLayouts = setLayouts.data();

	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocateInfo, inputDescriptorSets.data());
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate Input Attachment Descriptor Sets!");
	}

	// Update each descriptor set with input attachment
	for (size_t i = 0; i < swapchainImages.size(); i++)
	{
		// Color attachment descriptor
		VkDescriptorImageInfo colorAttachmentDescriptor = {};
		colorAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		colorAttachmentDescriptor.imageView = colorBufferImageView[i];
		colorAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		// Color Attachment Descriptor Write
		VkWriteDescriptorSet colorWrite = {};
		colorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorWrite.dstSet = inputDescriptorSets[i];
		colorWrite.dstBinding = 0;
		colorWrite.dstArrayElement = 0;
		colorWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		colorWrite.descriptorCount = 1;
		colorWrite.pImageInfo = &colorAttachmentDescriptor;

		// Depth attachment descriptor
		VkDescriptorImageInfo depthAttachmentDescriptor = {};
		depthAttachmentDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		depthAttachmentDescriptor.imageView = depthBufferImageView[i];
		depthAttachmentDescriptor.sampler = VK_NULL_HANDLE;

		// Depth Attachment Descriptor Write
		VkWriteDescriptorSet depthWrite = {};
		depthWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		depthWrite.dstSet = inputDescriptorSets[i];
		depthWrite.dstBinding = 1;
		depthWrite.dstArrayElement = 0;
		depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		depthWrite.descriptorCount = 1;
		depthWrite.pImageInfo = &depthAttachmentDescriptor;

		std::vector<VkWriteDescriptorSet> setWrites = {colorWrite, depthWrite};
		vkUpdateDescriptorSets(mainDevice.logicalDevice, static_cast<uint32_t>(setWrites.size()), setWrites.data(), 0, nullptr);
	}
}

void VulkanRenderer::updateUniformBuffers(uint32_t imageIndex)
{	// Copy VP data
	void *data;
	vkMapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex], 0, sizeof(UboViewProjection), 0, &data);
	memcpy(data, &uboViewProjection, sizeof(UboViewProjection));
	vkUnmapMemory(mainDevice.logicalDevice, vpUniformBufferMemory[imageIndex]);

	// LEGACY - for reference. Replaced by push constants
	//// Copy Model data
	//for (size_t i = 0; i < meshList.size(); i++)
	//{
	//	Model *thisModel = (Model *)((uint64_t)modelTransferSpace + (i * modelUniformAlignment));
	//	*thisModel = meshList[i].getModel();
	//}

	//// Map the list of model data
	//vkMapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex], 0, modelUniformAlignment * meshList.size(), 0, &data);
	//memcpy(data, modelTransferSpace, modelUniformAlignment * meshList.size());
	//vkUnmapMemory(mainDevice.logicalDevice, modelUniformBufferMemoryDynamic[imageIndex]);
}

void VulkanRenderer::recordCommands(uint32_t currentImage)
{
	// Information about how to begin each command buffer
	VkCommandBufferBeginInfo bufferBeginInfo = {};
	bufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;	// Buffer can be resubmitted when it has already been submitted and is awaiting execution

	// Information about how to begin a render pass (only needed for graphical applications)
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;							// Render pass to begin
	renderPassBeginInfo.renderArea.offset = {0, 0};							// Start point of render pass in pixels
	renderPassBeginInfo.renderArea.extent = swapchainExtent;				// Size of region to run render pass on (starting at offset)

	std::array<VkClearValue, 3> clearValues = {};
	clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
	clearValues[1].color = {0.6f, 0.65f, 0.4f, 1.0f};
	clearValues[2].depthStencil.depth = 1.0f;

	renderPassBeginInfo.pClearValues = clearValues.data();					// List of clear values
	renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());

	renderPassBeginInfo.framebuffer = swapchainFramebuffers[currentImage];

	// Start recording commands to command buffer
	VkResult result = vkBeginCommandBuffer(commandBuffers[currentImage], &bufferBeginInfo);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to start recording a Command Buffer!");
	}

		// Begin Render Pass
		vkCmdBeginRenderPass(commandBuffers[currentImage], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			// Bind Pipeline to be used in render pass
			vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			for (size_t j = 0; j < modelList.size(); j++)
			{
				MeshModel thisModel = modelList[j];
				// "Push" constants to given shader directly (no buffer)
				vkCmdPushConstants(
					commandBuffers[currentImage], 
					pipelineLayout,
					VK_SHADER_STAGE_VERTEX_BIT,												// Stage of pipeline to push constants to
					0,																		// Offset of push constants to update
					sizeof(Model),															// Size of data being pushed
					&thisModel.getModel()													// Actual data being pushed (can be array)
				);	

				for (size_t k = 0; k < thisModel.getMeshCount(); k++)
				{
					VkBuffer vertexBuffers[] = {thisModel.getMesh(k)->getVertexBuffer()};	// Buffers to bind
					VkDeviceSize offsets[] = {0};											// Offsets into buffers being bound
					vkCmdBindVertexBuffers(commandBuffers[currentImage], 0, 1, vertexBuffers, offsets);	// Command to bind vertex buffer before drawing with them

					// Bind mesh index buffer, with 0 offset and using the uint32 index type
					vkCmdBindIndexBuffer(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);

					// LEGACY
					// Dynamic Offset Amount
					//uint32_t dynamicOffset = static_cast<uint32_t>(modelUniformAlignment) * j;

					std::array<VkDescriptorSet, 2> descriptorSetGroup = { descriptorSets[currentImage], samplerDescriptorSets[thisModel.getMesh(k)->getTexId()]};

					// Bind Descriptor Sets
					vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 
						0, static_cast<uint32_t>(descriptorSetGroup.size()), descriptorSetGroup.data(), 0, nullptr);

					// Execute pipeline
					vkCmdDrawIndexed(commandBuffers[currentImage], thisModel.getMesh(k)->getIndexCount(), 1, 0, 0, 0);
				}
			}
			// Start second subpas
			vkCmdNextSubpass(commandBuffers[currentImage], VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipeline);
			vkCmdBindDescriptorSets(commandBuffers[currentImage], VK_PIPELINE_BIND_POINT_GRAPHICS, secondPipelineLayout, 
				0, 1, &inputDescriptorSets[currentImage], 0, nullptr);
			vkCmdDraw(commandBuffers[currentImage], 3, 1, 0, 0);

		// End Render Pass
		vkCmdEndRenderPass(commandBuffers[currentImage]);

	// Stop recording to command buffer
	result = vkEndCommandBuffer(commandBuffers[currentImage]);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to stop recording a Command Buffer!");
	}
}

void VulkanRenderer::getPhysicalDevice()
{
	// Enumerate Physical Devices the vkInstance can access
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	// If no devices available, then none suport Vulkan!
	if (deviceCount == 0)
	{
		throw std::runtime_error("Can't find GPU's that support Vulkan Instance!");
	}


	// Get list of Physical Devices
	std::vector<VkPhysicalDevice> deviceList(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, deviceList.data());

	bool foundSuitableDevice = false;
	for (const auto &device : deviceList)
	{
		if (checkDeviceSuitable(device))
		{
			mainDevice.physicalDevice = device;
			foundSuitableDevice = true;
			break;
		}
	}
	if (!foundSuitableDevice)
	{
		throw std::runtime_error("Can't find GPU's that support required queues and extensions!");
	}

	// Get properties of our new device
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(mainDevice.physicalDevice, &deviceProperties);

	// LEGACY
	//minUniformBufferOffset = deviceProperties.limits.minUniformBufferOffsetAlignment;
}

void VulkanRenderer::allocateDynamicBufferTransferSpace()
{
	// LEGACY - replaced by push constants
	//// Bitwise magic to get proper amount of blocks needed for our model data
	//modelUniformAlignment = (sizeof(Model) + minUniformBufferOffset - 1) 
	//						& ~(minUniformBufferOffset - 1);

	//// Create space in memory to hold dynamic buffer that is aligned to our required alignment and holds MAX_OBJECTS
	//modelTransferSpace = static_cast<Model *>(_aligned_malloc(modelUniformAlignment * MAX_OBJECTS, modelUniformAlignment));

}

bool VulkanRenderer::checkInstanceExtensionSupport(std::vector<const char*> *checkExtensions)
{
	// Need to get number of extensions to create array of correct size to hold extensions
	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

	// Create list of VkExtensionProperties using count
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

	// Check if given extensions are in list of available extensions
	for (const auto &checkExtension : *checkExtensions)
	{
		bool hasExtension = false;
		for (const auto &extension : extensions)
		{
			if (strcmp(checkExtension, extension.extensionName))
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}
	}
	return true;
}

bool VulkanRenderer::checkDeviceExtensionSupport(VkPhysicalDevice device)
{
	uint32_t extensionCount = 0;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
	if (extensionCount == 0)
	{
		return false;
	}
	std::vector<VkExtensionProperties> extensions(extensionCount);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data());

	// Check for extension
	for (const auto &deviceExtension : deviceExtensions)
	{
		bool hasExtension = false;
		for (const auto &extension : extensions)
		{
			if (strcmp(deviceExtension, extension.extensionName) == 0)
			{
				hasExtension = true;
				break;
			}
		}
		if (!hasExtension)
		{
			return false;
		}

	}
	return true;
}

bool VulkanRenderer::checkDeviceSuitable(VkPhysicalDevice device)
{
	/*
	// Information about the device itself (ID, name, type, etc.)
	VkPhysicalDeviceProperties deviceProperties;
	vkGetPhysicalDeviceProperties(device, &deviceProperties);
	*/

	// Information about what the device can do (geo shader, tess shader, wide lines, etc)
	
	vkGetPhysicalDeviceFeatures(device, &mainDevice.deviceFeatures);

	QueueFamilyIndices indices = getQueueFamilies(device);

	bool extensionsSupported = checkDeviceExtensionSupport(device);

	bool swapChainValid = false;
	if (extensionsSupported)
	{
		SwapChainDetails swapChainDetails = getSwapChainDetails(device);
		swapChainValid = !swapChainDetails.presentationModes.empty() && !swapChainDetails.formats.empty();
	}


	return indices.isValid() && extensionsSupported && swapChainValid;
}

bool VulkanRenderer::checkValidationLayerSupport()
{
	// Check if all requested Validation Layers are available
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

	for (const char* layerName : validationLayers)
	{
		bool layerFound = false;

		for (const auto& layerProperties : availableLayers)
		{
			if (strcmp(layerName, layerProperties.layerName) == 0)
			{
				layerFound = true;
				break;
			}
		}

		if (!layerFound)
		{
			return false;
		}
	}

	return true;
}


QueueFamilyIndices VulkanRenderer::getQueueFamilies(VkPhysicalDevice device)
{
	QueueFamilyIndices indices;
	// Get all Queue Family Property info for the given device
	uint32_t queueFamilyCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

	std::vector<VkQueueFamilyProperties> queueFamilyList(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyList.data());

	// Go through each queue family and check if it has at least 1 of the required types of queue
	int i = 0;
	for (const auto &queueFamily : queueFamilyList)
	{
		// First check if queue family has at least 1 queue in that family (could have no queues)
		// Queue can be multiple types defined through bitfield. Need to bitwise AND with VK_QUEUE_*_BIT to checkj if has required type
		if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			indices.graphicsFamily = i;		// If queue family is valid, get index
		}

		// Check if queue family supports presentation
		VkBool32 presentationSupport = false;
		vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentationSupport);

		// Check if queue is presentation type (can be both graphics and presentation)
		if (queueFamily.queueCount > 0 && presentationSupport)
		{
			indices.presentationFamily = i;
		}

		// Check if quueue family indices are in a valid state, stop searching if so
		if (indices.isValid())
		{
			break;
		}
		i++;
	}

	return indices;
}

SwapChainDetails VulkanRenderer::getSwapChainDetails(VkPhysicalDevice device)
{
	SwapChainDetails swapChainDetails;

	// -- CAPABILITIES --
	// Get the surface capabilities for the given surface on the given physical device
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &swapChainDetails.surfaceCapabilities);

	// -- FORMATS --
	uint32_t formatCount = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

	// If formats returned, get list of formats
	if (formatCount != 0)
	{
		swapChainDetails.formats.resize(formatCount);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, swapChainDetails.formats.data());
	}
	// -- PRESENTATION MODES --
	uint32_t presentationCount = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, nullptr);

	if (presentationCount != 0)
	{
		swapChainDetails.presentationModes.resize(presentationCount);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentationCount, swapChainDetails.presentationModes.data());
	}

	return swapChainDetails;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void * pUserData)
{
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}

void VulkanRenderer::setupDebugMessenger()
{
	if (!enableValidationLayers)
	{
		return;
	}
	VkDebugUtilsMessengerCreateInfoEXT createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
	createInfo.pUserData = nullptr; // Optional

	if (createDebugUtilsMessengerExt(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
	{
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

VkResult VulkanRenderer::createDebugUtilsMessengerExt(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo, const VkAllocationCallbacks * pAllocator, VkDebugUtilsMessengerEXT * pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanRenderer::destroyDebugUtilsMessengerExt(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks * pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr)
	{
		func(instance, debugMessenger, pAllocator);
	}
}

// Best format is subjective, but ours will be:
// format		:	VK_FORMAT_R8G8B8A8_UNORM (VK_FORMAT_B8G8R8A8_UNORM as backup)
// colorSpace	:	VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
VkSurfaceFormatKHR VulkanRenderer::chooseBestSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
{
	// If only 1 format available and is undefined, then this means ALL formats are available (no restrictions)
	if (formats.size() == 1 && formats [0].format == VK_FORMAT_UNDEFINED)
	{
		return {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
	}
	// If restricted, search for optimal format
	for (const auto &format : formats)
	{
		if ((format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM) && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}
	// If can't find optimal, then just return first format
	return formats [0];
}

VkPresentModeKHR VulkanRenderer::chooseBestPresentationMode(const std::vector<VkPresentModeKHR>& presentationModes)
{
	// Look for Mailbox presentation mode
	for (const auto &presentationMode : presentationModes)
	{
		if (presentationMode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return presentationMode;
		}
	}
	// If can't find, use FIFO as Vulkan spec says it must be present
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR & surfaceCapabilities)
{
	// If current extent is at numeric limits, extent can vary. Otherwise it is the size of the window.
	if (surfaceCapabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
	{
		return surfaceCapabilities.currentExtent;
	}
	else
	{
		// If value can vary, need to set manually
		// Get window size
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		// Create new extent using window size
		VkExtent2D newExtent = {};
		newExtent.width = static_cast<uint32_t>(width);
		newExtent.height = static_cast<uint32_t>(height);

		// Surface also defines max and min, so make sure within boundaries by clamping value
		newExtent.width = std::max(surfaceCapabilities.minImageExtent.width, std::min(surfaceCapabilities.maxImageExtent.width, newExtent.width));
		newExtent.height = std::max(surfaceCapabilities.minImageExtent.height, std::min(surfaceCapabilities.maxImageExtent.height, newExtent.height));

		return newExtent;
	}
}

VkFormat VulkanRenderer::chooseSupportedFormat(const std::vector<VkFormat>& formats, VkImageTiling tiling, VkFormatFeatureFlags featureFlags)
{
	// Loop through options and find compatible one
	for (VkFormat format : formats)
	{
		// Get properties for given format on this device
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(mainDevice.physicalDevice, format, &properties);

		// Depending on tiling choice need to check for different bit flag  
		if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		} 
		else if(tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & featureFlags) == featureFlags)
		{
			return format;
		}
	}
	throw std::runtime_error("Failed to find a matching format!");
}

VkImage VulkanRenderer::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags useFlags, VkMemoryPropertyFlags propertyFlags, VkDeviceMemory * imageMemory)
{
	// CREATE IMAGE
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;									// 1 for 2D image
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;									// Number of levels in image array
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = tiling;									// How image data should be tiled (arranged for optimal reading)
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = useFlags;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;					// Num of samples for multisampling
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;			// Whether image can be shared between queues

	VkImage image;
	VkResult result = vkCreateImage(mainDevice.logicalDevice, &imageCreateInfo, nullptr, &image);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image!");
	}

	// CREATE MEMORY FOR IMAGE
	// Get memory requirements for a type of image
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(mainDevice.logicalDevice, image, &memoryRequirements);

	// Allocate memory using image requirements and user defined properties
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.memoryTypeIndex = findMemoryTypeIndex(mainDevice.physicalDevice, memoryRequirements.memoryTypeBits, propertyFlags);

	result = vkAllocateMemory(mainDevice.logicalDevice, &memoryAllocateInfo, nullptr, imageMemory);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to allocate memory for Image!");
	}

	// Connect memory to image
	vkBindImageMemory(mainDevice.logicalDevice, image, *imageMemory, 0);

	return image;
}

VkImageView VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = image;												// Image to create image for
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;							// Type of image (1D, 2D, 3D, Cube, etc.)
	viewCreateInfo.format = format;												// Format of image data
	viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;				// Allows remapping of RGBA components to other RGBA values
	viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	// Subresources allow the view to view only a part of an image
	viewCreateInfo.subresourceRange.aspectMask = aspectFlags;					// Which aspect of image to view (e.g. COLOR_BIT for viewing color)
	viewCreateInfo.subresourceRange.baseMipLevel = 0;							// Start mipmap level to view from
	viewCreateInfo.subresourceRange.levelCount = 1;								// Number of mipmap levels to view
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;							// Start array level to view from
	viewCreateInfo.subresourceRange.layerCount = 1;								// Number of array layers to view

	// Create image view and return it
	VkImageView imageView;
	VkResult result = vkCreateImageView(mainDevice.logicalDevice, &viewCreateInfo, nullptr, &imageView);
	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create an Image View!");
	}
	return imageView;
}

VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = code.size();
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

	VkShaderModule shaderModule;
	VkResult result = vkCreateShaderModule(mainDevice.logicalDevice, &shaderModuleCreateInfo, nullptr, &shaderModule);

	if (result != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module!");
	}

	return shaderModule;
}

int VulkanRenderer::createTextureImage(std::string fileName)
{
	// Load image file
	int width, height;
	VkDeviceSize imageSize;
	stbi_uc *imageData = loadTextureFile(fileName, &width, &height, &imageSize);

	// Create staging buffer to hold loaded data, ready to copy to device
	VkBuffer imageStagingBuffer;
	VkDeviceMemory imageStagingBufferMemory;
	createBuffer(mainDevice.physicalDevice, mainDevice.logicalDevice, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
		&imageStagingBuffer, &imageStagingBufferMemory);
	// Copy image data to staging buffer
	void *data;
	vkMapMemory(mainDevice.logicalDevice, imageStagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, imageData, static_cast<size_t>(imageSize));
	vkUnmapMemory(mainDevice.logicalDevice, imageStagingBufferMemory);

	// Free original image data
	stbi_image_free(imageData);

	// Create image to hold final texture
	VkImage texImage;
	VkDeviceMemory texImageMemory;
	texImage = createImage(width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 
		&texImageMemory);

	// COPY DATA TO IMAGE
	// Transition image to be DST for copy operation
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, 
		texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// Copy image data
	copyImageBuffer(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, imageStagingBuffer, texImage, width, height);

	// Transition image to be shader readable for shader usage
	transitionImageLayout(mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, 
		texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	// Add texture data to vector for reference
	textureImages.push_back(texImage);
	texturesImageMemory.push_back(texImageMemory);

	// Destroy staging buffers
	vkDestroyBuffer(mainDevice.logicalDevice, imageStagingBuffer, nullptr);
	vkFreeMemory(mainDevice.logicalDevice, imageStagingBufferMemory, nullptr);

	// Return index of new texture image
	return textureImages.size() - 1;
}

int VulkanRenderer::createTexture(std::string fileName)
{
	// Create texture image and get its location in array
	int textureImageLoc = createTextureImage(fileName);

	VkImageView imageView = createImageView(textureImages[textureImageLoc], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	textureImageViews.push_back(imageView);

	// Create Texture Descriptor
	int descriptorLoc = createTextureDescriptor(imageView);

	// Return location of set with texture
	return descriptorLoc;
}

int VulkanRenderer::createTextureDescriptor(VkImageView textureImageView)
{
	VkDescriptorSet descriptorSet;

	// Descriptor Set Allocation Info
	VkDescriptorSetAllocateInfo setAllocateInfo = {};
	setAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	setAllocateInfo.descriptorPool = samplerDescriptorPool;
	setAllocateInfo.descriptorSetCount = 1;
	setAllocateInfo.pSetLayouts = &samplerSetLayout;

	// Allocate Descriptor Sets
	VkResult result = vkAllocateDescriptorSets(mainDevice.logicalDevice, &setAllocateInfo, &descriptorSet);
	if (result != VK_FALSE)
	{
		throw std::runtime_error("Failed to allocate Texture Descriptor Sets!");
	}

	// Texture Image Info
	VkDescriptorImageInfo imageInfo = {};
	imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;				// Image layout when in use
	imageInfo.imageView = textureImageView;											// Image to bind to set
	imageInfo.sampler = textureSampler;												// Sampler to use for set

	// Descriptor Write Info
	VkWriteDescriptorSet descriptorWrite = {};
	descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	descriptorWrite.dstSet = descriptorSet;
	descriptorWrite.dstBinding = 0;
	descriptorWrite.dstArrayElement = 0;
	descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descriptorWrite.descriptorCount = 1;
	descriptorWrite.pImageInfo = &imageInfo;

	// Update new descriptor set
	vkUpdateDescriptorSets(mainDevice.logicalDevice, 1, &descriptorWrite, 0, nullptr);

	// Add descriptor set to list
	samplerDescriptorSets.push_back(descriptorSet);

	// Return descriptor set location
	return samplerDescriptorSets.size() - 1;
}

int VulkanRenderer::createMeshModel(std::string modelFile)
{
	// Import model "scene"
	Assimp::Importer importer;
	const aiScene *scene = importer.ReadFile(modelFile, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

	if(!scene)
	{
		throw std::runtime_error("Failed to load model! (" + modelFile + ")");
	}

	// Get vector of all materials with 1:1 ID placement
	std::vector<std::string> textureNames = MeshModel::LoadMaterials(scene);

	// Conversion from the materials list IDs to our Descriptor Array IDs
	std::vector<int> matToTex(textureNames.size());

	// Loop over textureNames and create textures form them
	for (size_t i = 0; i < textureNames.size(); i++)
	{
		// If material had no texture, set '0' to indicate no texture, texture 0 will be reserved for a default texture
		if(textureNames[i].empty())
		{
			matToTex[i] = 0;
		}
		else
		{
			// Otherwise, create texture and set value to index of new texture
			matToTex[i] = createTexture(textureNames[i]);
		}
	}

	// Load in all our meshes
	std::vector<Mesh> modelMeshes = MeshModel::LoadNode(mainDevice.physicalDevice, mainDevice.logicalDevice, graphicsQueue, graphicsCommandPool, 
		scene->mRootNode, scene, matToTex);

	// Create MeshModel and add to list
	MeshModel meshModel = MeshModel(modelMeshes);
	modelList.push_back(meshModel);

	return modelList.size() - 1;
}

stbi_uc * VulkanRenderer::loadTextureFile(std::string fileName, int * width, int * height, VkDeviceSize * imageSize)
{
	// Num of channels image uses
	int channels;

	// Load pixel data for image
	std::string fileLoc = "Textures/" + fileName;
	stbi_uc *image = stbi_load(fileLoc.c_str(), width, height, &channels, STBI_rgb_alpha);

	if (!image)
	{
		throw std::runtime_error("Failed to load a Texture file! (" + fileName + ")");
	}

	// Calculate image size given and known data
	*imageSize = *width * (*height) * 4;

	return image;
}

