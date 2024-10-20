#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "device_helpers.cpp"
#include "file_helpers.cpp"
#include "vertexData.cpp"

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

		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createVertexBuffer();
		createCommandBuffers();
		createSyncObjects();

		std::cout << "Vulkan initialisation done\n";
	}
	~Vulkan()
	{
		cleanupSwapChain();

		device.destroyBuffer(vertexBuffer);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

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

	void updateVertexBuffer(std::vector<Vertex> v)
	{
		auto  size = sizeof(v[0]) * v.size();
		void *data;
		data = device.mapMemory(vertexBufferMemory, 0, size);
		memcpy(data, v.data(), (size_t) size);
		device.unmapMemory(vertexBufferMemory);
	}

	void drawFrame()
	{
		// todo check result
		auto r = device.waitForFences(1, &inFlightFences[currentFrame], true, UINT64_MAX);

		uint32_t   imageIndex;
		vk::Result result =
		    device.acquireNextImageKHR(swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			recreateSwapChain();
			return;
		}
		else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// todo check result
		r = device.resetFences(1, &inFlightFences[currentFrame]);

		commandBuffers[currentFrame].reset();
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		auto submitInfo = vk::SubmitInfo();

		vk::Semaphore          waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
		vk::PipelineStageFlags waitStages[]     = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
		submitInfo.waitSemaphoreCount           = 1;
		submitInfo.pWaitSemaphores              = waitSemaphores;
		submitInfo.pWaitDstStageMask            = waitStages;

		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers    = &commandBuffers[currentFrame];

		vk::Semaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
		submitInfo.signalSemaphoreCount  = 1;
		submitInfo.pSignalSemaphores     = signalSemaphores;

		graphicsQueue.submit(submitInfo, inFlightFences[currentFrame]);

		auto presentInfo               = vk::PresentInfoKHR();
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores    = signalSemaphores;

		vk::SwapchainKHR swapChains[] = {swapChain};
		presentInfo.swapchainCount    = 1;
		presentInfo.pSwapchains       = swapChains;

		presentInfo.pImageIndices = &imageIndex;

		auto pResult = presentQueue.presentKHR(presentInfo);

		if (pResult == vk::Result::eErrorOutOfDateKHR || pResult == vk::Result::eSuboptimalKHR || framebufferResized)
		{
			framebufferResized = false;
			recreateSwapChain();
		}
		else if (result != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to present swap chain image!");
		}

		currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
	}

  private:
	vk::Instance                   instance;
	VkSurfaceKHR                   surface;        // surface to render into
	vk::PhysicalDevice             physicalDevice;
	vk::Queue                      graphicsQueue;        // queue to the selected logical device
	vk::Queue                      presentQueue;         // presentation qeueue, connected to the surface
	vk::SwapchainKHR               swapChain;
	std::vector<vk::Image>         swapChainImages;
	vk::Format                     swapChainImageFormat;
	vk::Extent2D                   swapChainExtent;
	std::vector<vk::ImageView>     swapChainImageViews;
	vk::RenderPass                 renderPass;
	vk::PipelineLayout             pipelineLayout;
	vk::Pipeline                   graphicsPipeline;
	vk::CommandPool                commandPool;
	std::vector<vk::Framebuffer>   swapChainFramebuffers;
	std::vector<vk::CommandBuffer> commandBuffers;
	vk::Buffer                     vertexBuffer;
	vk::DeviceMemory               vertexBufferMemory;

	// rendering related
	std::vector<VkSemaphore> imageAvailableSemaphores;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<vk::Fence>   inFlightFences;

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
			auto subresourceRange = vk::ImageSubresourceRange(
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

		auto bindingDescription    = Vertex::getBindingDescription();
		auto attributeDescriptions = Vertex::getAttributeDescriptions();

		auto vertexInputInfo                            = vk::PipelineVertexInputStateCreateInfo();
		vertexInputInfo.vertexBindingDescriptionCount   = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions      = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions    = attributeDescriptions.data();

		auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo({}, vk::PrimitiveTopology::eTriangleList, false);

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
			vk::ImageView attachments[] = {
			    swapChainImageViews[i]};

			auto framebufferInfo            = vk::FramebufferCreateInfo();
			framebufferInfo.renderPass      = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments    = attachments;
			framebufferInfo.width           = swapChainExtent.width;
			framebufferInfo.height          = swapChainExtent.height;
			framebufferInfo.layers          = 1;

			swapChainFramebuffers[i] = device.createFramebuffer(framebufferInfo);
		}
	}

	void createCommandPool()
	{
		auto poolInfo             = vk::CommandPoolCreateInfo();
		poolInfo.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
		poolInfo.queueFamilyIndex = indices.graphicsFamily.value();

		commandPool = device.createCommandPool(poolInfo);
	}

	void createCommandBuffers()
	{
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		auto allocInfo               = vk::CommandBufferAllocateInfo();
		allocInfo.commandPool        = commandPool;
		allocInfo.level              = vk::CommandBufferLevel::ePrimary;
		allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

		commandBuffers = device.allocateCommandBuffers(allocInfo);
	}

	void recordCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
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

		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, graphicsPipeline);

		vk::Buffer     vertexBuffers[] = {vertexBuffer};
		vk::DeviceSize offsets[]       = {0};
		commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, offsets);

		vkCmdDraw(commandBuffer, static_cast<uint32_t>(vertices.size()), 1, 0, 0);

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
			imageAvailableSemaphores[i] = device.createSemaphore(semaphoreInfo);
			renderFinishedSemaphores[i] = device.createSemaphore(semaphoreInfo);
			inFlightFences[i]           = device.createFence(fenceInfo);
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

	void createVertexBuffer()
	{
		auto bufferInfo = vk::BufferCreateInfo({}, sizeof(vertices[0]) * vertices.size(), vk::BufferUsageFlagBits::eVertexBuffer);

		vertexBuffer = device.createBuffer(bufferInfo);

		auto memRequirements = device.getBufferMemoryRequirements(vertexBuffer);

		auto memoryIndex = DeviceHelpers::findMemoryType(physicalDevice, memRequirements.memoryTypeBits, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
		auto allocInfo   = vk::MemoryAllocateInfo(memRequirements.size, memoryIndex);

		vertexBufferMemory = device.allocateMemory(allocInfo);
		device.bindBufferMemory(vertexBuffer, vertexBufferMemory, 0);

		void *data;
		data = device.mapMemory(vertexBufferMemory, 0, bufferInfo.size);
		memcpy(data, vertices.data(), (size_t) bufferInfo.size);
		device.unmapMemory(vertexBufferMemory);
	}
};