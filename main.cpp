#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#include "vulkan.cpp"

#ifdef _WIN32
#	include <corecrt_math_defines.h>
#endif

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
		auto state = vertices;
		int  i     = 0;
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			vulkan->drawFrame();
			i++;

			if (i % 2 == 0)
			{
				auto withColors = i % 10 == 0 ? getNewColors(state) : state;
				auto rotated    = rotate(withColors);
				state           = rotated;
				vulkan->updateVertexBuffer(rotated);
			}
		}

		vkDeviceWaitIdle(vulkan->device);
	}

	std::vector<Vertex> getNewColors(std::vector<Vertex> v)
	{
		std::random_device                    rd;
		std::mt19937                          mt(rd());
		std::uniform_real_distribution<float> dist(0.0, 1.0);

		std::vector<Vertex> result = {
		    {v[0].pos, {dist(mt), dist(mt), dist(mt)}},
		    {v[1].pos, {dist(mt), dist(mt), dist(mt)}},
		    {v[2].pos, {dist(mt), dist(mt), dist(mt)}}};

		return result;
	}

	float               angleInRadians = 1 * (M_PI / 180.0f);
	std::vector<Vertex> rotate(std::vector<Vertex> v)
	{
		// Calculate sine and cosine of the rotation angle
		float cosTheta = std::cos(angleInRadians);
		float sinTheta = std::sin(angleInRadians);

		for (auto &vertex : v)
		{
			// Apply the 2D rotation matrix to each vertex
			float newX = vertex.pos[0] * cosTheta - vertex.pos[1] * sinTheta;
			float newY = vertex.pos[0] * sinTheta + vertex.pos[1] * cosTheta;

			// Update the vertex with the rotated coordinates
			vertex.pos[0] = newX;
			vertex.pos[1] = newY;
		}

		return v;
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