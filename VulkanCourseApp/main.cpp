#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include<stdexcept>
#include <vector>
#include <iostream>

#include "VulkanRenderer.h"

GLFWwindow *window;
VulkanRenderer vulkanRenderer;



void initWindow(std::string wName = "Test Window", const int width = 800, const int height= 600)
{
	// Init GLFW
	glfwInit();

	// Set GLFW to NOT work with OpenGL, disable window resize as resizing can make things complicated
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	window = glfwCreateWindow(width, height, wName.c_str(), nullptr, nullptr);
}



int main()
{
	// Create window
	initWindow("Test Window", 800, 600);

	// Create Vulkan renderer instance
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}
	// Loop until closed
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	} 

	vulkanRenderer.cleanup();

	// Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}	