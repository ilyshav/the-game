#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "vulkan.cpp"

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();

		vulkan = new Vulkan(window);
		mainLoop();
		cleanup();
	}

  private:
	Vulkan     *vulkan;
	GLFWwindow *window;

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			vulkan->drawFrame();
		}

		vkDeviceWaitIdle(vulkan->device);
	}

	void cleanup()
	{
		delete (vulkan);

		glfwDestroyWindow(window);
		glfwTerminate();
	}

	void initWindow()
	{
		const uint32_t WIDTH  = 800;
		const uint32_t HEIGHT = 600;

		glfwInit();

		// disable opengl
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		// disable window resizing
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "The Game", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
	{
		auto app                        = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
		app->vulkan->framebufferResized = true;
	}
};

int main()
{
	HelloTriangleApplication app;

	try
	{
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}