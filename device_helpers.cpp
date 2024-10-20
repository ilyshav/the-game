#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_beta.h>

#include <algorithm>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <set>

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

#ifdef __APPLE__
const bool isMac = true;
#else
const bool isMac = false;
#endif

struct QueueFamilyIndices
{
	std::optional<uint32_t> graphicsFamily;
	std::optional<uint32_t> presentFamily;
};

struct SwapChainSupportDetails
{
	vk::SurfaceCapabilitiesKHR        capabilities;
	std::vector<vk::SurfaceFormatKHR> formats;
	std::vector<vk::PresentModeKHR>   presentModes;
};

struct CreateDeviceResult
{
	vk::Device device;
	vk::Queue  graphicsQueue;
	vk::Queue  presentQueue;
};

class DeviceHelpers
{
  private:
	static bool areAllExtensionsSupported(vk::PhysicalDevice device)
	{
		auto availableExtensions = device.enumerateDeviceExtensionProperties();

		const std::vector<const char *> deviceExtensions = {
		    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (auto &ext : availableExtensions)
		{
			requiredExtensions.erase(ext.extensionName);
		}
		return requiredExtensions.empty();
	}

  public:
	static QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device,
	                                            VkSurfaceKHR       surface)
	{
		QueueFamilyIndices indices;
		auto               queueFamilyProperties = device.getQueueFamilyProperties();

		// get the first index into queueFamiliyProperties which supports graphics
		auto   propertyIterator         = std::find_if(queueFamilyProperties.begin(),
		                                               queueFamilyProperties.end(),
		                                               [](vk::QueueFamilyProperties const &qfp) { return qfp.queueFlags & vk::QueueFlagBits::eGraphics; });
		size_t graphicsQueueFamilyIndex = std::distance(queueFamilyProperties.begin(), propertyIterator);
		indices.graphicsFamily          = graphicsQueueFamilyIndex;

		vk::Bool32 surfaceSupport = device.getSurfaceSupportKHR(static_cast<uint32_t>(graphicsQueueFamilyIndex), surface);

		if (surfaceSupport)
		{
			indices.presentFamily = graphicsQueueFamilyIndex;
			return indices;
		}
		else
		{
			throw std::runtime_error("Can not find queue with graphics and presentation support");
		}
	}

	static std::tuple<bool, std::string> isDeviceSuitable(
	    vk::PhysicalDevice device, VkSurfaceKHR surface)
	{
		auto deviceProperties = device.getProperties();
		auto deviceFeatures   = device.getFeatures();

		QueueFamilyIndices      indices          = findQueueFamilies(device, surface);
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);

		bool exntesionsSupported  = areAllExtensionsSupported(device);
		bool swapChainSupportFine = false;
		// extensions check must be before swapchain check
		if (exntesionsSupported)
		{
			swapChainSupportFine = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		// originally there was deviceFeatures.geometryShader check, but it is
		// not available in MacOS
		bool isSuitable = swapChainSupportFine;

		auto name   = std::string(deviceProperties.deviceName.data());
		auto result = std::tuple(isSuitable, name);

		return result;
	}

	static VkPhysicalDevice pickPhysicalDevice(vk::Instance instance,
	                                           VkSurfaceKHR surface)
	{
		auto devices = instance.enumeratePhysicalDevices();

		if (devices.size() == 0)
		{
			throw std::runtime_error(
			    "failed to find GPUs with Vulkan support!");
		}

		for (const auto &device : devices)
		{
			auto result     = isDeviceSuitable(device, surface);
			auto isSuitable = std::get<0>(result);
			auto name       = std::get<1>(result);

			if (isSuitable)
			{
				std::cout << std::string("Going to use ") + name +
				                 std::string(" card\n");
				return device;
			}
		}

		throw std::runtime_error("failed to find a suitable GPU!");
	}

	static CreateDeviceResult createLogicalDevice(
	    vk::PhysicalDevice        physicalDevice,
	    std::vector<const char *> validationLayers,
	    VkSurfaceKHR              surface,
	    QueueFamilyIndices        indices)
	{
		std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t>                     uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies)
		{
			auto queueInfo = vk::DeviceQueueCreateInfo(
			    {},
			    queueFamily,
			    1,
			    &queuePriority);
			queueCreateInfos.push_back(queueInfo);
		}

		std::vector<const char *> deviceExtensions = {
		    VK_KHR_SWAPCHAIN_EXTENSION_NAME};

		if (isMac)
		{
			deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		}

		auto deviceCreateInfo = vk::DeviceCreateInfo(
		    {},
		    static_cast<uint32_t>(queueCreateInfos.size()),
		    queueCreateInfos.data(),
		    static_cast<uint32_t>(validationLayers.size()),
		    validationLayers.data(),
		    static_cast<uint32_t>(deviceExtensions.size()),
		    deviceExtensions.data());

		auto device = physicalDevice.createDevice(deviceCreateInfo);

		vk::Queue graphicsQueue;
		vk::Queue presentQueue;

		graphicsQueue = device.getQueue(indices.graphicsFamily.value(), 0);
		presentQueue  = device.getQueue(indices.presentFamily.value(), 0);

		CreateDeviceResult result;
		result.device        = device;
		result.graphicsQueue = graphicsQueue;
		result.presentQueue  = presentQueue;

		return result;
	}

	static SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device, VkSurfaceKHR surface)
	{
		SwapChainSupportDetails details;

		auto capabilities    = device.getSurfaceCapabilitiesKHR(surface);
		details.capabilities = capabilities;

		auto formats    = device.getSurfaceFormatsKHR(surface);
		details.formats = formats;

		auto presentModes    = device.getSurfacePresentModesKHR(surface);
		details.presentModes = presentModes;

		return details;
	}

	// how colors of the image are described. srgb is the best option
	static VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
	{
		for (const auto &availableFormat : availableFormats)
		{
			if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
			    availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
			{
				return availableFormat;
			}
		}

		return availableFormats[0];
	}

	// how we are going to show images from presentation queue.
	static vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
	{
		for (const auto &availablePresentMode : availablePresentModes)
		{
			// the best one
			if (availablePresentMode == vk::PresentModeKHR::eMailbox)
			{
				return availablePresentMode;
			}
		}

		// always avaialble
		return vk::PresentModeKHR::eFifo;
	}

	// resolution our images in the swap chain
	static VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, GLFWwindow *window)
	{
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);

			VkExtent2D actualExtent = {
			    static_cast<uint32_t>(width),
			    static_cast<uint32_t>(height)};

			actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
			actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

			return actualExtent;
		}
	}

	static uint32_t findMemoryType(vk::PhysicalDevice physicalDevice, uint32_t typeFilter, vk::MemoryPropertyFlags properties)
	{
		auto memProperties = physicalDevice.getMemoryProperties();

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}
};