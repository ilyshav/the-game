#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "device_helpers.cpp"
#include "file_helpers.cpp"

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

const int MAX_FRAMES_IN_FLIGHT = 2;

class Vulkan
{
  public:
	vk::Device device;

	// dynamic
	bool framebufferResized = false;

	Vulkan(GLFWwindow *window)
	{
		this->window = window;
		createInstance();
		createSurface();
		physicalDevice = DeviceHelpers::pickPhysicalDevice(instance, surface);
		indices        = DeviceHelpers::findQueueFamilies(physicalDevice, surface);

		auto result = DeviceHelpers::createLogicalDevice(physicalDevice,
		                                                 validationLayers, surface, indices);

		device        = result.device;
		graphicsQueue = result.graphicsQueue;
		presentQueue  = result.presentQueue;

		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSyncObjects();

		std::cout << "Vulkan initialisation done\n";
	}
	~Vulkan()
	{
		cleanupSwapChain();

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		vkDestroyRenderPass(device, renderPass, nullptr);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
			vkDestroyFence(device, inFlightFences[i], nullptr);
		}

		vkDestroyCommandPool(device, commandPool, nullptr);

		vkDestroyDevice(device, nullptr);

		vkDestroySurfaceKHR(instance, surface, nullptr);
		vkDestroyInstance(instance, nullptr);
	}

	void drawFrame()
	{
		vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		vkResetFences(device, 1, &inFlightFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore          waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
		VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
		submitInfo.waitSemaphoreCount         = 1;
		submitInfo.pWaitSemaphores            = waitSemaphores;
		submitInfo.pWaitDstStageMask          = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &commandBuffers[currentFrame];

		VkSemaphore signalSemaphores[]  = {renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores    = signalSemaphores;

		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to submit draw command buffer!");
		}

		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores    = signalSemaphores;

		VkSwapchainKHR swapChains[] = {swapChain};
		presentInfo.swapchainCount  = 1;
		presentInfo.pSwapchains     = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
		{
			framebufferResized = false;
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

  private:
	vk::Instance                 instance;
	VkSurfaceKHR                 surface;        // surface to render into
	vk::PhysicalDevice           physicalDevice;
	VkQueue                      graphicsQueue;        // queue to the selected logical device
	VkQueue                      presentQueue;         // presentation qeueue, connected to the surface
	vk::SwapchainKHR             swapChain;
	std::vector<vk::Image>       swapChainImages;
	vk::Format                   swapChainImageFormat;
	VkExtent2D                   swapChainExtent;
	std::vector<VkImageView>     swapChainImageViews;
	vk::RenderPass               renderPass;
	VkPipelineLayout             pipelineLayout;
	vk::Pipeline                 graphicsPipeline;
	VkCommandPool                commandPool;
	std::vector<VkFramebuffer>   swapChainFramebuffers;
	std::vector<VkCommandBuffer> commandBuffers;

	// rendering related
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkFence>     inFlightFences;

	QueueFamilyIndices indices;

	// GLFW window
	GLFWwindow *window;

	// dynamic variables
	uint32_t currentFrame = 0;

	// The main vulkan settings
	void createInstance()
	{
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("validation layers requested, but not available!");
		}

		vk::ApplicationInfo appInfo("The game");

		auto requiredExtensions = getExtentions();

		vk::InstanceCreateFlags flags = isMac ? vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR : vk::InstanceCreateFlags{};

		auto                   validation = enableValidationLayers ? validationLayers : std::vector<const char *>{};
		vk::InstanceCreateInfo createInfo(
		    flags,
		    &appInfo,
		    static_cast<uint32_t>(validation.size()),
		    validation.data(),
		    static_cast<uint32_t>(requiredExtensions.size()),
		    requiredExtensions.data());

		instance = vk::createInstance(createInfo);
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char *layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto &layerProperties : availableLayers)
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

	std::vector<const char *> getExtentions()
	{
		uint32_t     glfwExtensionCount = 0;
		const char **glfwExtensions;

		// required for glfw
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char *> extensions(
		    glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (isMac)
		{
			extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
			extensions.push_back(
			    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
		}

		return extensions;
	}

	void createSurface()
	{
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) !=
		    VK_SUCCESS)
		{
			throw std::runtime_error("failed to create window surface!");
		}
	}

	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = DeviceHelpers::querySwapChainSupport(physicalDevice, surface);

		vk::SurfaceFormatKHR surfaceFormat = DeviceHelpers::chooseSwapSurfaceFormat(swapChainSupport.formats);
		auto                 presentMode   = DeviceHelpers::chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D           extent        = DeviceHelpers::chooseSwapExtent(swapChainSupport.capabilities, window);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo{};

		uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		if (indices.graphicsFamily != indices.presentFamily)
		{
			createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices   = queueFamilyIndices;
		}
		else
		{
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		createInfo.clipped = VK_TRUE;

		auto swapChainCreateInfo = vk::SwapchainCreateInfoKHR(
		    {},
		    surface,
		    imageCount,
		    surfaceFormat.format,
		    surfaceFormat.colorSpace,
		    extent,
		    1,        // image array layers
		    vk::ImageUsageFlagBits::eColorAttachment,
		    vk::SharingMode::eExclusive,        // todo!
		    queueFamilyIndices,
		    vk::SurfaceTransformFlagBitsKHR::eIdentity,
		    vk::CompositeAlphaFlagBitsKHR::eOpaque,
		    presentMode,
		    true        // clipped
		);

		swapChain       = device.createSwapchainKHR(swapChainCreateInfo);
		swapChainImages = device.getSwapchainImagesKHR(swapChain);

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent      = extent;
	}

	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo{};
			auto                  subresourceRange = vk::ImageSubresourceRange(
                vk::ImageAspectFlagBits::eColor,
                0,        // base mip level
                1,        // level count
                0,        // base array level
                1         // layerCount
            );

			auto imageViewInfo = vk::ImageViewCreateInfo(
			    {},
			    swapChainImages[i],
			    vk::ImageViewType::e2D,
			    swapChainImageFormat,
			    vk::ComponentMapping(),
			    subresourceRange);

			auto image             = device.createImageView(imageViewInfo);
			swapChainImageViews[i] = image;
		}
	}

	// https://vulkan-tutorial.com/images/vulkan_simplified_pipeline.svg
	void createGraphicsPipeline()
	{
		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		vk::ShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		vk::ShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		auto vertShaderStageInfo = vk::PipelineShaderStageCreateInfo(
		    {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main");

		auto fragShaderStageInfo = vk::PipelineShaderStageCreateInfo(
		    {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main");

		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo();
		auto inputAssembly   = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eTriangleList, false);

		std::vector<vk::DynamicState> dynamicStates = {
		    vk::DynamicState::eViewport,
		    vk::DynamicState::eScissor};

		auto dynamicState = vk::PipelineDynamicStateCreateInfo({}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data());

		auto viewportState = vk::PipelineViewportStateCreateInfo({}, 1, {}, 1);

		auto rasterizer        = vk::PipelineRasterizationStateCreateInfo();
		rasterizer.polygonMode = vk::PolygonMode::eFill;
		rasterizer.lineWidth   = 1.0f;
		rasterizer.cullMode    = vk::CullModeFlagBits::eBack;
		rasterizer.frontFace   = vk::FrontFace::eClockwise;

		auto multisampling = vk::PipelineMultisampleStateCreateInfo();

		auto colorBlendAttachment           = vk::PipelineColorBlendAttachmentState();
		colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
		                                      vk::ColorComponentFlagBits::eG |
		                                      vk::ColorComponentFlagBits::eB |
		                                      vk::ColorComponentFlagBits::eA;

		auto colorBlending            = vk::PipelineColorBlendStateCreateInfo();
		colorBlending.logicOp         = vk::LogicOp::eCopy;
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments    = &colorBlendAttachment;

		auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo();
		pipelineLayout          = device.createPipelineLayout(pipelineLayoutInfo);

		auto pipelineInfo                = vk::GraphicsPipelineCreateInfo();
		pipelineInfo.stageCount          = 2;
		pipelineInfo.pStages             = shaderStages;
		pipelineInfo.pVertexInputState   = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState      = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState   = &multisampling;
		pipelineInfo.pColorBlendState    = &colorBlending;
		pipelineInfo.pDynamicState       = &dynamicState;
		pipelineInfo.layout              = pipelineLayout;
		pipelineInfo.renderPass          = renderPass;
		pipelineInfo.subpass             = 0;
		pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

		auto graphicsPipelineResult = device.createGraphicsPipeline(nullptr, pipelineInfo);
		graphicsPipeline            = graphicsPipelineResult.value;

		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	vk::ShaderModule createShaderModule(const std::vector<char> &code)
	{
		auto convertedCode    = reinterpret_cast<const uint32_t *>(code.data());
		auto createModuleInfo = vk::ShaderModuleCreateInfo({}, code.size(), convertedCode);
		return device.createShaderModule(createModuleInfo);
	}

	void createRenderPass()
	{
		auto colorAttachment = vk::AttachmentDescription(
		    {},
		    swapChainImageFormat,
		    vk::SampleCountFlagBits::e1,
		    vk::AttachmentLoadOp::eClear,
		    vk::AttachmentStoreOp::eStore,
		    vk::AttachmentLoadOp::eDontCare,
		    vk::AttachmentStoreOp::eDontCare,
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::ePresentSrcKHR);

		auto colorAttachmentRef = vk::AttachmentReference(0, vk::ImageLayout::eColorAttachmentOptimal);

		auto subpass = vk::SubpassDescription(
		    {},
		    vk::PipelineBindPoint::eGraphics,
		    0,         // input attachments count
		    {},        // input attachments
		    1,         // color attachment count,
		    &colorAttachmentRef);

		auto dependency = vk::SubpassDependency(
		    vk::SubpassExternal,
		    {},        // dst subpass
		    vk::PipelineStageFlagBits::eColorAttachmentOutput,
		    vk::PipelineStageFlagBits::eColorAttachmentOutput,
		    {},        // srcAccessMask
		    vk::AccessFlagBits::eColorAttachmentWrite);

		auto createInfo = vk::RenderPassCreateInfo(
		    {},
		    1,        // attachment count
		    &colorAttachment,
		    1,        // subpass count
		    &subpass,
		    1,        // dependency count
		    &dependency);
		renderPass = device.createRenderPass(createInfo);
	}

	void createFramebuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = {
			    swapChainImageViews[i]};

			VkFramebufferCreateInfo framebufferInfo{};
			framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass      = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments    = attachments;
			framebufferInfo.width           = swapChainExtent.width;
			framebufferInfo.height          = swapChainExtent.height;
			framebufferInfo.layers          = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create framebuffer!");
			}
		}
	}

	void createCommandPool()
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to create command pool!");
		}
	}

	void createCommandBuffers()
	{
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		VkCommandBufferAllocateInfo allocInfo{};
		allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool        = commandPool;
		allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();
		;

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to allocate command buffers!");
		}
	}

	void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to begin recording command buffer!");
		}

		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass        = renderPass;
		renderPassInfo.framebuffer       = swapChainFramebuffers[imageIndex];
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = swapChainExtent;

		VkClearValue clearColor        = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues    = &clearColor;

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		VkViewport viewport{};
		viewport.x        = 0.0f;
		viewport.y        = 0.0f;
		viewport.width    = (float) swapChainExtent.width;
		viewport.height   = (float) swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = swapChainExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
		{
			throw std::runtime_error("failed to record command buffer!");
		}
	}

	void createSyncObjects()
	{
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
			    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
			    vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS)
			{
				throw std::runtime_error("failed to create synchronization objects for a frame!");
			}
		}
	}

	void cleanupSwapChain()
	{
		for (auto framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		for (auto imageView : swapChainImageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		// todo part of window api in renderer should not be here
		glfwGetFramebufferSize(window, &width, &height);
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		vkDeviceWaitIdle(device);

		cleanupSwapChain();

		createSwapChain();
		createImageViews();
		createFramebuffers();
	}
};