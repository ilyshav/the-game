#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_beta.h>

#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

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

	bool isComplete()
	{
		return graphicsFamily.has_value() && presentFamily.has_value();
	}
};

class DeviceHelpers
{
  public:
	static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
	                                            VkSurfaceKHR     surface)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
		                                         nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
		                                         queueFamilies.data());

		int i = 0;
		for (const auto &queueFamily : queueFamilies)
		{
			if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
			                                     &presentSupport);

			if (presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	static std::tuple<bool, std::string> isDeviceSuitable(
	    VkPhysicalDevice device, VkSurfaceKHR surface)
	{
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures   deviceFeatures;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		QueueFamilyIndices indices = findQueueFamilies(device, surface);

		// originally there was deviceFeatures.geometryShader check, but it is
		// not available in MacOS
		bool isSuitable = indices.isComplete();

		std::string name   = std::string(deviceProperties.deviceName);
		auto        result = std::tuple(isSuitable, name);

		return result;
	}

	static VkPhysicalDevice pickPhysicalDevice(VkInstance   instance,
	                                           VkSurfaceKHR surface)
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			throw std::runtime_error(
			    "failed to find GPUs with Vulkan support!");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

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

	static VkDevice createLogicalDevice(
	    VkPhysicalDevice          physicalDevice,
	    std::vector<const char *> validationLayers, VkSurfaceKHR surface)
	{
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice, surface);

		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount       = 1;

		float queuePriority              = 1.0f;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		// features, that Im going to use from videocard
		VkPhysicalDeviceFeatures deviceFeatures{};

		VkDeviceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

		createInfo.pQueueCreateInfos    = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;

		createInfo.pEnabledFeatures = &deviceFeatures;

		std::vector<const char *> deviceExtensions;

		if (isMac)
		{
			deviceExtensions.push_back(
			    VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
		}

		createInfo.enabledExtensionCount =
		    static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount =
			    static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		VkDevice device;

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) !=
		    VK_SUCCESS)
		{
			throw std::runtime_error("failed to create logical device!");
		}

		return device;
	}
};