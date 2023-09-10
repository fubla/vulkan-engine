#define STB_IMAGE_IMPLEMENTATION
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

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
	initWindow("Test Window", 1366, 768);

	// Create Vulkan renderer instance
	if (vulkanRenderer.init(window) == EXIT_FAILURE)
	{
		return EXIT_FAILURE;
	}

	float angle = 0.0f;
	float zCoord = 0.0f;
	float deltaTime = 0.0f;
	float lastTime = 0.0f;

	int skull = vulkanRenderer.createMeshModel("Models/12140_Skull_v3_L2.obj");

	// Loop until closed
	while(!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		float now = glfwGetTime();
		deltaTime = now - lastTime;
		lastTime = now;

		angle += 10.0f * deltaTime;

		if(angle > 360.0f)
		{
			angle -= 360.0f;
		}

		glm::mat4 testMat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -10.0f, 0.0f));
		testMat = glm::rotate(testMat, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		testMat = glm::rotate(testMat, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
		vulkanRenderer.updateModel(skull, testMat);

		vulkanRenderer.draw();
	} 

	vulkanRenderer.cleanup();

	// Destroy GLFW window and stop GLFW
	glfwDestroyWindow(window);
	glfwTerminate();

	return EXIT_SUCCESS;
}	