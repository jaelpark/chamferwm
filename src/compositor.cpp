#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <set>
#include <algorithm>
#include <cstdlib>
#include <limits>

namespace Compositor{

ClientFrame::ClientFrame(uint w, uint h, CompositorInterface *_pcomp) : pcomp(_pcomp), passignedSet(0), time(0.0f){
	pcomp->updateQueue.push_back(this);

	ptexture = pcomp->CreateTexture(w,h);
	if(!AssignPipeline(pcomp->pdefaultPipeline))
		throw Exception("Failed to assign a pipeline.");
	DebugPrintf(stdout,"Texture created: %ux%u\n",w,h);

	UpdateDescSets();

	clock_gettime(CLOCK_MONOTONIC,&creationTime);
}

ClientFrame::~ClientFrame(){
	pcomp->ReleaseTexture(ptexture);

	for(PipelineDescriptorSet &pipelineDescSet : descSets)
		for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
			if(pipelineDescSet.pdescSets[i]){
				/*vkFreeDescriptorSets(pcomp->logicalDev,pcomp->descPool,pipelineDescSet.p->pshaderModule[i]->setCount,pipelineDescSet.pdescSets[i]);
				delete []pipelineDescSet.pdescSets[i];*/
				pcomp->ReleaseDescSets(pipelineDescSet.p->pshaderModule[i],pipelineDescSet.pdescSets[i]);
			}
}

void ClientFrame::Draw(const VkRect2D &frame, const glm::vec2 &borderWidth, const VkCommandBuffer *pcommandBuffer){
	time = timespec_diff(pcomp->frameTime,creationTime);

	for(uint i = 0, descPointer = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
		if(passignedSet->p->pshaderModule[i]->setCount > 0){
			vkCmdBindDescriptorSets(*pcommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,passignedSet->p->pipelineLayout,descPointer,passignedSet->p->pshaderModule[i]->setCount,passignedSet->pdescSets[i],0,0);
			descPointer += passignedSet->p->pshaderModule[i]->setCount;
		}
	
	glm::vec4 frameVec = {frame.offset.x,frame.offset.y,frame.offset.x+frame.extent.width,frame.offset.y+frame.extent.height};
	frameVec += 0.5f;
	frameVec /= (glm::vec4){pcomp->imageExtent.width,pcomp->imageExtent.height,pcomp->imageExtent.width,pcomp->imageExtent.height};
	frameVec *= 2.0f;
	frameVec -= 1.0f;

	const float pushConstants[] = {
		frameVec.x,frameVec.y,
		frameVec.z,frameVec.w,
		pcomp->imageExtent.width,pcomp->imageExtent.height,
		borderWidth.x,borderWidth.y,
		time,0.0f
	};
	vkCmdPushConstants(*pcommandBuffer,passignedSet->p->pipelineLayout,VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,40,pushConstants); //size in CompositorResource VkPushConstantRange

	vkCmdDraw(*pcommandBuffer,1,1,0,0);

	passignedSet->fenceTag = pcomp->frameTag;
}

void ClientFrame::AdjustSurface(uint w, uint h){
	pcomp->ReleaseTexture(ptexture);

	pcomp->updateQueue.push_back(this);

	ptexture = pcomp->CreateTexture(w,h);
	//In this case updating the descriptor sets would be enough, but we can't do that because of them being used currently by frames in flight.
	if(!AssignPipeline(passignedSet->p))
		throw Exception("Failed to assign a pipeline.");
	DebugPrintf(stdout,"Texture created: %ux%u\n",w,h);

	UpdateDescSets();
}

bool ClientFrame::AssignPipeline(const Pipeline *prenderPipeline){
	auto m = std::find_if(descSets.begin(),descSets.end(),[&](auto &r)->bool{
		return r.p == prenderPipeline && pcomp->frameTag > r.fenceTag+pcomp->swapChainImageCount+1;
	});
	if(m != descSets.end()){
		passignedSet = &(*m);
		return true;
	}

	uint totalSetCount = 0;

	PipelineDescriptorSet pipelineDescSet;
	pipelineDescSet.fenceTag = pcomp->frameTag;
	pipelineDescSet.p = prenderPipeline;
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		if(!prenderPipeline->pshaderModule[i] || prenderPipeline->pshaderModule[i]->setCount == 0){
			pipelineDescSet.pdescSets[i] = 0;
			continue;
		}
		/*pipelineDescSet.pdescSets[i] = new VkDescriptorSet[prenderPipeline->pshaderModule[i]->setCount];
		VkDescriptorSetAllocateInfo descSetAllocateInfo = {};
		descSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descSetAllocateInfo.descriptorPool = pcomp->descPool;
		descSetAllocateInfo.pSetLayouts = prenderPipeline->pshaderModule[i]->pdescSetLayouts;
		descSetAllocateInfo.descriptorSetCount = prenderPipeline->pshaderModule[i]->setCount;
		if(vkAllocateDescriptorSets(pcomp->logicalDev,&descSetAllocateInfo,pipelineDescSet.pdescSets[i]) != VK_SUCCESS)
			return false;*/
		pipelineDescSet.pdescSets[i] = pcomp->CreateDescSets(prenderPipeline->pshaderModule[i]);
		if(!pipelineDescSet.pdescSets[i])
			return false;

		totalSetCount += prenderPipeline->pshaderModule[i]->setCount;
	}
	descSets.push_back(pipelineDescSet);
	passignedSet = &descSets.back();

	return true;
}

void ClientFrame::UpdateDescSets(){
	//
	VkDescriptorImageInfo descImageInfo = {};
	descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descImageInfo.imageView = ptexture->imageView;
	descImageInfo.sampler = pcomp->pointSampler;

	std::vector<VkWriteDescriptorSet> writeDescSets;
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		auto m1 = std::find_if(passignedSet->p->pshaderModule[i]->bindings.begin(),passignedSet->p->pshaderModule[i]->bindings.end(),[&](auto &r)->bool{
			return r.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && strcmp(r.pname,"content") == 0;
		});
		if(m1 != passignedSet->p->pshaderModule[i]->bindings.end()){
			VkWriteDescriptorSet &writeDescSet = writeDescSets.emplace_back();
			writeDescSet = (VkWriteDescriptorSet){};
			writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescSet.dstSet = passignedSet->pdescSets[i][(*m1).setIndex];
			writeDescSet.dstBinding = (*m1).binding;
			writeDescSet.dstArrayElement = 0;
			writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			writeDescSet.descriptorCount = 1;
			writeDescSet.pImageInfo = &descImageInfo;
		}

		auto m2 = std::find_if(passignedSet->p->pshaderModule[i]->bindings.begin(),passignedSet->p->pshaderModule[i]->bindings.end(),[&](auto &r)->bool{
			return r.type == VK_DESCRIPTOR_TYPE_SAMPLER;
		});
		if(m2 != passignedSet->p->pshaderModule[i]->bindings.end()){
			VkWriteDescriptorSet &writeDescSet = writeDescSets.emplace_back();
			writeDescSet = (VkWriteDescriptorSet){};
			writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescSet.dstSet = passignedSet->pdescSets[i][(*m2).setIndex];
			writeDescSet.dstBinding = (*m2).binding;
			writeDescSet.dstArrayElement = 0;
			writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
			writeDescSet.descriptorCount = 1;
			writeDescSet.pImageInfo = &descImageInfo;
		}
	}
	vkUpdateDescriptorSets(pcomp->logicalDev,writeDescSets.size(),writeDescSets.data(),0,0);
}

CompositorInterface::CompositorInterface(uint _physicalDevIndex) : physicalDevIndex(_physicalDevIndex), currentFrame(0), frameTag(0){
	//
}

CompositorInterface::~CompositorInterface(){
	//
}

void CompositorInterface::InitializeRenderEngine(){
	uint layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount,0);
	VkLayerProperties *playerProps = new VkLayerProperties[layerCount];
	vkEnumerateInstanceLayerProperties(&layerCount,playerProps);

	const char *players[] = {"VK_LAYER_LUNARG_standard_validation"};
	DebugPrintf(stdout,"Enumerating required layers\n");
	uint layersFound = 0;
	for(uint i = 0; i < layerCount; ++i)
		for(uint j = 0; j < sizeof(players)/sizeof(players[0]); ++j)
			if(strcmp(playerProps[i].layerName,players[j]) == 0){
				printf("%s\n",players[j]);
				++layersFound;
			}
	if(layersFound < sizeof(players)/sizeof(players[0]))
		throw Exception("Could not find all required layers.");

	uint extCount;
	vkEnumerateInstanceExtensionProperties(0,&extCount,0);
	VkExtensionProperties *pextProps = new VkExtensionProperties[extCount];
	vkEnumerateInstanceExtensionProperties(0,&extCount,pextProps);

	const char *pextensions[] = {
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		"VK_KHR_surface",
		"VK_KHR_xcb_surface"
	};
	DebugPrintf(stdout,"Enumerating required extensions\n");
	uint extFound = 0;
	for(uint i = 0; i < extCount; ++i)
		for(uint j = 0; j < sizeof(pextensions)/sizeof(pextensions[0]); ++j)
			if(strcmp(pextProps[i].extensionName,pextensions[j]) == 0){
				printf("%s\n",pextensions[j]);
				++extFound;
			}
	if(extFound < sizeof(pextensions)/sizeof(pextensions[0]))
		throw Exception("Could not find all required extensions.");
	
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "chamferwm";
	appInfo.applicationVersion = VK_MAKE_VERSION(0,0,1);
	appInfo.pEngineName = "chamferwm-engine";
	appInfo.engineVersion = VK_MAKE_VERSION(0,0,1);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	instanceCreateInfo.enabledLayerCount = sizeof(players)/sizeof(players[0]);
	instanceCreateInfo.ppEnabledLayerNames = players;
	instanceCreateInfo.enabledExtensionCount = sizeof(pextensions)/sizeof(pextensions[0]);
	instanceCreateInfo.ppEnabledExtensionNames = pextensions;
	if(vkCreateInstance(&instanceCreateInfo,0,&instance) != VK_SUCCESS)
		throw Exception("Failed to create Vulkan instance.");
	
	delete []playerProps;
	delete []pextProps;

	CreateSurfaceKHR(&surface);
	
	VkDebugReportCallbackCreateInfoEXT debugcbCreateInfo = {};
	debugcbCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	debugcbCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT|VK_DEBUG_REPORT_WARNING_BIT_EXT;
	debugcbCreateInfo.pfnCallback = ValidationLayerDebugCallback;
	((PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkCreateDebugReportCallbackEXT"))(instance,&debugcbCreateInfo,0,&debugReportCb);

	DebugPrintf(stdout,"Enumerating physical devices\n");

	//physical device
	uint devCount;
	vkEnumeratePhysicalDevices(instance,&devCount,0);
	VkPhysicalDevice *pdevices = new VkPhysicalDevice[devCount];
	vkEnumeratePhysicalDevices(instance,&devCount,pdevices);

	for(uint i = 0; i < devCount; ++i){
		VkPhysicalDeviceProperties devProps;
		vkGetPhysicalDeviceProperties(pdevices[i],&devProps);
		VkPhysicalDeviceFeatures devFeatures;
		vkGetPhysicalDeviceFeatures(pdevices[i],&devFeatures);

		printf("%c %u: %s\n\t.deviceID: %u\n\t.vendorID: %u\n\t.deviceType: %u\n",
			i == physicalDevIndex?'*':' ',
			i,devProps.deviceName,devProps.deviceID,devProps.vendorID,devProps.deviceType);
		printf("  max push constant size: %u\n  max bound desc sets: %u\n",devProps.limits.maxPushConstantsSize,devProps.limits.maxBoundDescriptorSets);
	}

	if(physicalDevIndex >= devCount){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Invalid gpu-index (%u) exceeds the number of available devices (%u).",physicalDevIndex,devCount);
		throw Exception();
	}

	physicalDev = pdevices[physicalDevIndex];

	delete []pdevices;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDev,surface,&surfaceCapabilities);

	uint formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDev,surface,&formatCount,0);
	VkSurfaceFormatKHR *pformats = new VkSurfaceFormatKHR[formatCount];
	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDev,surface,&formatCount,pformats);

	DebugPrintf(stdout,"Available surface formats: %u\n",formatCount);
	for(uint i = 0; i < formatCount; ++i)
		if(pformats[i].format == VK_FORMAT_B8G8R8A8_UNORM && pformats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			printf("Surface format ok.\n");

	uint presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDev,surface,&presentModeCount,0);
	VkPresentModeKHR *ppresentModes = new VkPresentModeKHR[presentModeCount];
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDev,surface,&presentModeCount,ppresentModes);

	uint queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDev,&queueFamilyCount,0);
	VkQueueFamilyProperties *pqueueFamilyProps = new VkQueueFamilyProperties[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDev,&queueFamilyCount,pqueueFamilyProps);

	//find required queue families
	for(uint i = 0; i < QUEUE_INDEX_COUNT; ++i)
		queueFamilyIndex[i] = ~0;
	for(uint i = 0; i < queueFamilyCount; ++i){
		if(pqueueFamilyProps[i].queueCount > 0 && pqueueFamilyProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT){
			queueFamilyIndex[QUEUE_INDEX_GRAPHICS] = i;
			break;
		}
	}
	for(uint i = 0; i < queueFamilyCount; ++i){
		VkBool32 presentSupport;
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDev,i,surface,&presentSupport);

		bool compatible = CheckPresentQueueCompatibility(physicalDev,i);
		//printf("Device compatibility:\t%u\nPresent support:\t%u\n",compatible,presentSupport);
		if(pqueueFamilyProps[i].queueCount > 0 && compatible && presentSupport){
			queueFamilyIndex[QUEUE_INDEX_PRESENT] = i;
			break;
		}
	}
	std::set<uint> queueSet;
	for(uint i = 0; i < QUEUE_INDEX_COUNT; ++i){
		if(queueFamilyIndex[i] == ~0u)
			throw Exception("No suitable queue family available.");
		queueSet.insert(queueFamilyIndex[i]);
	}

	delete []pqueueFamilyProps;

	//queue creation
	VkDeviceQueueCreateInfo queueCreateInfo[QUEUE_INDEX_COUNT];
	uint queueCount = 0;
	for(uint queueFamilyIndex1 : queueSet){
		//logical device
		static const float queuePriorities[] = {1.0f};
		queueCreateInfo[queueCount] = (VkDeviceQueueCreateInfo){};
		queueCreateInfo[queueCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[queueCount].queueFamilyIndex = queueFamilyIndex1;
		queueCreateInfo[queueCount].queueCount = 1;
		queueCreateInfo[queueCount].pQueuePriorities = queuePriorities;
		++queueCount;
	}

	VkPhysicalDeviceFeatures physicalDevFeatures = {};
	physicalDevFeatures.geometryShader = VK_TRUE;
	
	uint devExtCount;
	vkEnumerateDeviceExtensionProperties(physicalDev,0,&devExtCount,0);
	VkExtensionProperties *pdevExtProps = new VkExtensionProperties[devExtCount];
	vkEnumerateDeviceExtensionProperties(physicalDev,0,&devExtCount,pdevExtProps);

	//device extensions
	const char *pdevExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
	DebugPrintf(stdout,"Enumerating required device extensions\n");
	uint devExtFound = 0;
	for(uint i = 0; i < devExtCount; ++i)
		for(uint j = 0; j < sizeof(pdevExtensions)/sizeof(pdevExtensions[0]); ++j)
			if(strcmp(pdevExtProps[i].extensionName,pdevExtensions[j]) == 0){
				printf("%s\n",pdevExtensions[j]);
				++devExtFound;
			}
	if(devExtFound < sizeof(pdevExtensions)/sizeof(pdevExtensions[0]))
		throw Exception("Could not find all required device extensions.");
	//

	VkDeviceCreateInfo devCreateInfo = {};
	devCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devCreateInfo.pQueueCreateInfos = queueCreateInfo;
	devCreateInfo.queueCreateInfoCount = queueCount;
	devCreateInfo.pEnabledFeatures = &physicalDevFeatures;
	devCreateInfo.ppEnabledExtensionNames = pdevExtensions;
	devCreateInfo.enabledExtensionCount = sizeof(pdevExtensions)/sizeof(pdevExtensions[0]);
	devCreateInfo.ppEnabledLayerNames = players;
	devCreateInfo.enabledLayerCount = sizeof(players)/sizeof(players[0]);
	if(vkCreateDevice(physicalDev,&devCreateInfo,0,&logicalDev) != VK_SUCCESS)
		throw Exception("Failed to create a logical device.");
	
	delete []pdevExtProps;

	for(uint i = 0; i < QUEUE_INDEX_COUNT; ++i)
		vkGetDeviceQueue(logicalDev,queueFamilyIndex[i],0,&queue[i]);

	//render pass (later an array of these for different purposes)
	VkAttachmentReference attachmentRef = {};
	attachmentRef.attachment = 0;
	attachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDesc = {};
	subpassDesc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &attachmentRef;

	VkAttachmentDescription attachmentDesc = {};
	attachmentDesc.format = VK_FORMAT_B8G8R8A8_UNORM;
	attachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
	attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkSubpassDependency subpassDependency = {};
	subpassDependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpassDependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.srcAccessMask = 0;
	subpassDependency.dstSubpass = 0;
	subpassDependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	//subpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_MEMORY_READ_BIT;

	VkRenderPassCreateInfo renderPassCreateInfo = {};
	renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassCreateInfo.attachmentCount = 1;
	renderPassCreateInfo.pAttachments = &attachmentDesc;
	renderPassCreateInfo.subpassCount = 1;
	renderPassCreateInfo.pSubpasses = &subpassDesc;
	renderPassCreateInfo.dependencyCount = 1;
	renderPassCreateInfo.pDependencies = &subpassDependency;
	if(vkCreateRenderPass(logicalDev,&renderPassCreateInfo,0,&renderPass) != VK_SUCCESS)
		throw Exception("Failed to create a render pass.");
	
	imageExtent = GetExtent();

	//swap chain
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = 3;
	swapchainCreateInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
	swapchainCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
	swapchainCreateInfo.imageExtent = imageExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if(queueFamilyIndex[QUEUE_INDEX_GRAPHICS] != queueFamilyIndex[QUEUE_INDEX_PRESENT]){
		DebugPrintf(stdout,"concurrent swap chain\n");
		static const uint queueFamilyIndex1[] = {queueFamilyIndex[QUEUE_INDEX_GRAPHICS],queueFamilyIndex[QUEUE_INDEX_PRESENT]};
		swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		swapchainCreateInfo.queueFamilyIndexCount = 2;
		swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndex1;
	}else swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	//swapchainCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
	swapchainCreateInfo.presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = 0;
	if(vkCreateSwapchainKHR(logicalDev,&swapchainCreateInfo,0,&swapChain) != VK_SUCCESS)
		throw Exception("Failed to create swap chain.");

	DebugPrintf(stdout,"Swap chain image extent %ux%u\n",swapchainCreateInfo.imageExtent.width,swapchainCreateInfo.imageExtent.height); 
	vkGetSwapchainImagesKHR(logicalDev,swapChain,&swapChainImageCount,0);
	pswapChainImages = new VkImage[swapChainImageCount];
	vkGetSwapchainImagesKHR(logicalDev,swapChain,&swapChainImageCount,pswapChainImages);

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = VK_FORMAT_B8G8R8A8_UNORM;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	pswapChainImageViews = new VkImageView[swapChainImageCount];
	pframebuffers = new VkFramebuffer[swapChainImageCount];
	for(uint i = 0; i < swapChainImageCount; ++i){
		imageViewCreateInfo.image = pswapChainImages[i];
		if(vkCreateImageView(logicalDev,&imageViewCreateInfo,0,&pswapChainImageViews[i]) != VK_SUCCESS)
			throw Exception("Failed to create a swap chain image view.");

		VkFramebufferCreateInfo framebufferCreateInfo = {};
		framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		framebufferCreateInfo.renderPass = renderPass;
		framebufferCreateInfo.attachmentCount = 1;
		framebufferCreateInfo.pAttachments = &pswapChainImageViews[i];
		framebufferCreateInfo.width = imageExtent.width;
		framebufferCreateInfo.height = imageExtent.height;
		framebufferCreateInfo.layers = 1;
		if(vkCreateFramebuffer(logicalDev,&framebufferCreateInfo,0,&pframebuffers[i]) != VK_SUCCESS)
			throw Exception("Failed to create a framebuffer.");
	}

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	psemaphore = new VkSemaphore[swapChainImageCount][SEMAPHORE_INDEX_COUNT];
	pfence = new VkFence[swapChainImageCount];

	for(uint i = 0; i < swapChainImageCount; ++i){
		if(vkCreateFence(logicalDev,&fenceCreateInfo,0,&pfence[i]) != VK_SUCCESS)
			throw Exception("Failed to create a fence.");
		for(uint j = 0; j < SEMAPHORE_INDEX_COUNT; ++j)
			if(vkCreateSemaphore(logicalDev,&semaphoreCreateInfo,0,&psemaphore[i][j]) != VK_SUCCESS)
				throw Exception("Failed to create a semaphore.");
	}

	//sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 1;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	if(vkCreateSampler(logicalDev,&samplerCreateInfo,0,&pointSampler) != VK_SUCCESS)
		throw Exception("Failed to create a sampler.");

	//descriptor pool
	//descriptors of this pool
	VkDescriptorPoolSize descPoolSizes[2];
	descPoolSizes[0] = (VkDescriptorPoolSize){};
	descPoolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;//VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descPoolSizes[0].descriptorCount = 16;

	descPoolSizes[1] = (VkDescriptorPoolSize){};
	descPoolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	descPoolSizes[1].descriptorCount = 16;

	//TODO: pool memory management, probably a vector a pools
	//- find_if, find the pool which still has sets available
	VkDescriptorPoolCreateInfo descPoolCreateInfo = {};
	descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCreateInfo.poolSizeCount = sizeof(descPoolSizes)/sizeof(descPoolSizes[0]);
	descPoolCreateInfo.pPoolSizes = descPoolSizes;
	descPoolCreateInfo.maxSets = 16;//swapChainImageCount;
	descPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	if(vkCreateDescriptorPool(logicalDev,&descPoolCreateInfo,0,&descPool) != VK_SUCCESS)
		throw Exception("Failed to create a descriptor pool.");

	//command pool and buffers
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex[QUEUE_INDEX_GRAPHICS];
	commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	if(vkCreateCommandPool(logicalDev,&commandPoolCreateInfo,0,&commandPool) != VK_SUCCESS)
		throw Exception("Failed to create a command pool.");
	
	pcommandBuffers = new VkCommandBuffer[swapChainImageCount];
	pcopyCommandBuffers = new VkCommandBuffer[swapChainImageCount];

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = swapChainImageCount;
	if(vkAllocateCommandBuffers(logicalDev,&commandBufferAllocateInfo,pcommandBuffers) != VK_SUCCESS)
		throw Exception("Failed to allocate command buffers.");
	
	if(vkAllocateCommandBuffers(logicalDev,&commandBufferAllocateInfo,pcopyCommandBuffers) != VK_SUCCESS)
		throw Exception("Failed to allocate copy command buffer.");

	shaders.reserve(3);

	//load compositor resources
	Blob blob("/mnt/data/Asiakirjat/projects/chamferwm/build/frame_vertex.spv");
	ShaderModule &pvertexShader = shaders.emplace_back(&blob,this);

	blob.~Blob();
	new(&blob) Blob("/mnt/data/Asiakirjat/projects/chamferwm/build/frame_geometry.spv");
	ShaderModule &pgeometryShader = shaders.emplace_back(&blob,this);

	blob.~Blob();
	new(&blob) Blob("/mnt/data/Asiakirjat/projects/chamferwm/build/frame_fragment.spv");
	ShaderModule &pfragmentShader = shaders.emplace_back(&blob,this);

	pipelines.reserve(1);
	
	pdefaultPipeline = &pipelines.emplace_back(&pvertexShader,&pgeometryShader,&pfragmentShader,this);

}

void CompositorInterface::DestroyRenderEngine(){
	DebugPrintf(stdout,"Compositor cleanup\n");

	for(TextureCacheEntry &textureCacheEntry : textureCache)
		delete textureCacheEntry.ptexture;

	pipelines.clear();
	shaders.clear();

	delete []pcommandBuffers;
	delete []pcopyCommandBuffers;
	vkDestroyCommandPool(logicalDev,commandPool,0);

	vkDestroyDescriptorPool(logicalDev,descPool,0);

	vkDestroySampler(logicalDev,pointSampler,0);

	for(uint i = 0; i < swapChainImageCount; ++i){
		vkDestroyFence(logicalDev,pfence[i],0);
		for(uint j = 0; j < SEMAPHORE_INDEX_COUNT; ++j)
			vkDestroySemaphore(logicalDev,psemaphore[i][j],0);

		vkDestroyFramebuffer(logicalDev,pframebuffers[i],0);
		vkDestroyImageView(logicalDev,pswapChainImageViews[i],0);
	}
	delete []psemaphore;
	delete []pfence;
	delete []pswapChainImageViews;
	delete []pswapChainImages;
	vkDestroySwapchainKHR(logicalDev,swapChain,0);

	vkDestroyRenderPass(logicalDev,renderPass,0);

	vkDestroyDevice(logicalDev,0);

	((PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT"))(instance,debugReportCb,0);

	vkDestroySurfaceKHR(instance,surface,0);
	vkDestroyInstance(instance,0);
}

void CompositorInterface::WaitIdle(){
	vkDeviceWaitIdle(logicalDev);
}

void CompositorInterface::CreateRenderQueue(const WManager::Container *pcontainer){
	/*for(const WManager::Container *pcont = pcontainer; pcont; pcont = pcont->pnext){
		if(!pcont->pclient)
			continue;
		ClientFrame *pclientFrame = dynamic_cast<ClientFrame *>(pcont->pclient);
		if(!pclientFrame)
			continue;
		
		RenderObject renderObject;
		renderObject.pclient = pcont->pclient;
		renderObject.pclientFrame = pclientFrame;
		renderQueue.push_back(renderObject);

		if(pcont->pch)
			CreateRenderQueue(pcont->pch);
		//worry about stacks later
		//stacks: render in same order, except skip focus? Focus is rendered last.
	}*/
	for(const WManager::Container *pcont : pcontainer->stackQueue){
		if(!pcont->pclient)
			continue;
		ClientFrame *pclientFrame = dynamic_cast<ClientFrame *>(pcont->pclient);
		if(!pclientFrame)
			continue;
		
		RenderObject renderObject;
		renderObject.pclient = pcont->pclient;
		renderObject.pclientFrame = pclientFrame;
		renderQueue.push_back(renderObject);

		if(pcont->pch)
			CreateRenderQueue(pcont->pch);
	}
}

bool CompositorInterface::PollFrameFence(){
	if(vkWaitForFences(logicalDev,1,&pfence[currentFrame],VK_TRUE,0) == VK_TIMEOUT)
		return false;
	vkResetFences(logicalDev,1,&pfence[currentFrame]);

	//release the textures no longer in use
	textureCache.erase(std::remove_if(textureCache.begin(),textureCache.end(),[&](auto &textureCacheEntry)->bool{
		if(frameTag < textureCacheEntry.releaseTag+swapChainImageCount+1 || timespec_diff(frameTime,textureCacheEntry.releaseTime) < 5.0f)
			return false;
		delete textureCacheEntry.ptexture;
		return true;
	}),textureCache.end());

	descSetCache.erase(std::remove_if(descSetCache.begin(),descSetCache.end(),[&](auto &descSetCacheEntry)->bool{
		if(frameTag < descSetCacheEntry.releaseTag+swapChainImageCount+1)
			return false;
		vkFreeDescriptorSets(logicalDev,descPool,descSetCacheEntry.setCount,descSetCacheEntry.pdescSets);
		delete []descSetCacheEntry.pdescSets;
		return true;
	}),descSetCache.end());
	return true;
}

void CompositorInterface::GenerateCommandBuffers(const WManager::Container *proot){
	if(!proot)
		return;
	
	//Create a render list elements arranged from back to front
	renderQueue.clear();
	//frameObjectPool.clear();
	//CreateRenderQueue(proot->pch);
	CreateRenderQueue(proot);

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = 0;
	if(vkBeginCommandBuffer(pcopyCommandBuffers[currentFrame],&commandBufferBeginInfo) != VK_SUCCESS)
		throw Exception("Failed to begin command buffer recording.");

	for(ClientFrame *pclientFrame : updateQueue)
		pclientFrame->UpdateContents(&pcopyCommandBuffers[currentFrame]);
	updateQueue.clear();

	if(vkEndCommandBuffer(pcopyCommandBuffers[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to end command buffer recording.");

	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	if(vkBeginCommandBuffer(pcommandBuffers[currentFrame],&commandBufferBeginInfo) != VK_SUCCESS)
		throw Exception("Failed to begin command buffer recording.");

	static const VkClearValue clearValue = {1.0f,1.0f,1.0f,1.0};
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = pframebuffers[currentFrame];
	renderPassBeginInfo.renderArea.offset = {0,0};
	renderPassBeginInfo.renderArea.extent = imageExtent;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValue;
	vkCmdBeginRenderPass(pcommandBuffers[currentFrame],&renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(pcommandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,pdefaultPipeline->pipeline);

	clock_gettime(CLOCK_MONOTONIC,&frameTime);

	for(RenderObject &renderObject : renderQueue){
		VkRect2D frame;
		frame.offset = {renderObject.pclient->rect.x,renderObject.pclient->rect.y};
		frame.extent = {renderObject.pclient->rect.w,renderObject.pclient->rect.h};

		renderObject.pclientFrame->Draw(frame,renderObject.pclient->pcontainer->borderWidth,&pcommandBuffers[currentFrame]);
	}

	vkCmdEndRenderPass(pcommandBuffers[currentFrame]);

	if(vkEndCommandBuffer(pcommandBuffers[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to end command buffer recording.");
}

void CompositorInterface::Present(){
	uint imageIndex;
	if(vkAcquireNextImageKHR(logicalDev,swapChain,std::numeric_limits<uint64_t>::max(),psemaphore[currentFrame][SEMAPHORE_INDEX_IMAGE_AVAILABLE],0,&imageIndex) != VK_SUCCESS)
		throw Exception("Failed to acquire a swap chain image.\n");
	//
	VkPipelineStageFlags pipelineStageFlags[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	//VkPipelineStageFlags pipelineStageFlags[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
	
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pcopyCommandBuffers[currentFrame];
	if(vkQueueSubmit(queue[QUEUE_INDEX_GRAPHICS],1,&submitInfo,0) != VK_SUCCESS)
		throw Exception("Failed to submit a queue.");

	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &psemaphore[currentFrame][SEMAPHORE_INDEX_IMAGE_AVAILABLE];
	submitInfo.pWaitDstStageMask = pipelineStageFlags;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &psemaphore[currentFrame][SEMAPHORE_INDEX_RENDER_FINISHED];
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pcommandBuffers[currentFrame];
	if(vkQueueSubmit(queue[QUEUE_INDEX_GRAPHICS],1,&submitInfo,pfence[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to submit a queue.");
	
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &psemaphore[currentFrame][SEMAPHORE_INDEX_RENDER_FINISHED];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = 0;
	vkQueuePresentKHR(queue[QUEUE_INDEX_PRESENT],&presentInfo);

	currentFrame = (currentFrame+1)%swapChainImageCount;

	frameTag++;
}

Texture * CompositorInterface::CreateTexture(uint w, uint h){
	Texture *ptexture;

	auto m = std::find_if(textureCache.begin(),textureCache.end(),[&](auto &r)->bool{
		return r.ptexture->w == w && r.ptexture->h == h;
	});
	if(m != textureCache.end()){
		ptexture = (*m).ptexture;

		std::iter_swap(m,textureCache.end()-1);
		textureCache.pop_back();
		printf("----------- found cached texture\n");

	}else ptexture = new Texture(w,h,VK_FORMAT_R8G8B8A8_UNORM,this);

	return ptexture;
}

void CompositorInterface::ReleaseTexture(Texture *ptexture){
	TextureCacheEntry textureCacheEntry;
	textureCacheEntry.ptexture = ptexture;
	textureCacheEntry.releaseTag = frameTag;
	clock_gettime(CLOCK_MONOTONIC,&textureCacheEntry.releaseTime);
	
	textureCache.push_back(textureCacheEntry); //->emplace_back
}

VkDescriptorSet * CompositorInterface::CreateDescSets(const ShaderModule *pshaderModule){
	VkDescriptorSet *pdescSets = new VkDescriptorSet[pshaderModule->setCount];

	VkDescriptorSetAllocateInfo descSetAllocateInfo = {};
	descSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocateInfo.descriptorPool = descPool;
	descSetAllocateInfo.pSetLayouts = pshaderModule->pdescSetLayouts;
	descSetAllocateInfo.descriptorSetCount = pshaderModule->setCount;
	if(vkAllocateDescriptorSets(logicalDev,&descSetAllocateInfo,pdescSets) != VK_SUCCESS){
		delete []pdescSets;
		return 0;
	}
	
	return pdescSets;
}

void CompositorInterface::ReleaseDescSets(const ShaderModule *pshaderModule, VkDescriptorSet *pdescSets){
	DescSetCacheEntry descSetCacheEntry;
	descSetCacheEntry.pdescSets = pdescSets;
	descSetCacheEntry.setCount = pshaderModule->setCount;
	descSetCacheEntry.releaseTag = frameTag;

	descSetCache.push_back(descSetCacheEntry);
}

VKAPI_ATTR VkBool32 VKAPI_CALL CompositorInterface::ValidationLayerDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char *playerPrefix, const char *pmsg, void *puserData){
	DebugPrintf(stderr,"validation layer: %s\n",pmsg);
	return VK_FALSE;
}

X11ClientFrame::X11ClientFrame(WManager::Container *pcontainer, const Backend::X11Client::CreateInfo *_pcreateInfo, CompositorInterface *_pcomp) : X11Client(pcontainer,_pcreateInfo), ClientFrame(rect.w,rect.h,_pcomp){// : ClientFrame(_pcomp), X11Client(_pcreateInfo){
	//
	//xcb_composite_redirect_subwindows(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);
	//xcb_composite_redirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);
	xcb_composite_redirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);

	windowPixmap = xcb_generate_id(pbackend->pcon);
	xcb_composite_name_window_pixmap(pbackend->pcon,window,windowPixmap);
	//DebugPrintf(stdout,"Created pixmap (%x)\n",windowPixmap);

	damage = xcb_generate_id(pbackend->pcon);
	xcb_damage_create(pbackend->pcon,damage,window,XCB_DAMAGE_REPORT_LEVEL_RAW_RECTANGLES);
	//xcb_damage_create(pbackend->pcon,damage,window,XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);
}

X11ClientFrame::~X11ClientFrame(){
	xcb_damage_destroy(pbackend->pcon,damage);
	//
	xcb_composite_unredirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);
	xcb_free_pixmap(pbackend->pcon,windowPixmap);
}

void X11ClientFrame::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	/*xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry_unchecked(pbackend->pcon,windowPixmap);
	xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pbackend->pcon,geometryCookie,0);
	if(!pgeometryReply){
		DebugPrintf(stderr,"Failed to get pixmap geometry.\n");
		return;
	}
	DebugPrintf(stdout,"---- rect: %ux%u, pixmap: %ux%u\n",rect.w,rect.h,pgeometryReply->width,pgeometryReply->height);
	free(pgeometryReply);*/

	xcb_get_image_cookie_t imageCookie = xcb_get_image_unchecked(pbackend->pcon,XCB_IMAGE_FORMAT_Z_PIXMAP,windowPixmap,0,0,rect.w,rect.h,~0);
	xcb_get_image_reply_t *pimageReply = xcb_get_image_reply(pbackend->pcon,imageCookie,0);
	if(!pimageReply){
		DebugPrintf(stderr,"Failed to receive image reply.\n");
		return;
	}

	//http://doc.qt.io/qt-5/qimage.html
	//argb can be swizzled (image view)

	unsigned char *pdata = (unsigned char *)ptexture->Map();

	unsigned char *pchpixels = xcb_get_image_data(pimageReply);
	//memcpy(pdata,pchpixels,rect.w*rect.h*4);
	for(VkRect2D &rect1 : damageRegions){
		for(uint y = rect1.offset.y, Y = y+rect1.extent.height; y < Y; ++y){
			uint offset = 4*(rect.w*y+rect1.offset.x);
			memcpy(pdata+offset,pchpixels+offset,4*rect1.extent.width);
		}
	}

	ptexture->Unmap(pcommandBuffer,damageRegions.data(),damageRegions.size());
	damageRegions.clear();

	free(pimageReply);
}

void X11ClientFrame::AdjustSurface1(){
	//TODO: should window be re-redirected?
	xcb_free_pixmap(pbackend->pcon,windowPixmap);
	xcb_composite_name_window_pixmap(pbackend->pcon,window,windowPixmap);

	AdjustSurface(rect.w,rect.h);
}

X11Compositor::X11Compositor(uint physicalDevIndex, const Backend::X11Backend *pbackend) : CompositorInterface(physicalDevIndex){
	this->pbackend = pbackend;
}

X11Compositor::~X11Compositor(){
	//
}

void X11Compositor::Start(){

	//compositor
	if(!pbackend->QueryExtension("Composite",&compEventOffset,&compErrorOffset))
		throw Exception("XCompositor unavailable.\n");
	xcb_composite_query_version_cookie_t compCookie = xcb_composite_query_version(pbackend->pcon,XCB_COMPOSITE_MAJOR_VERSION,XCB_COMPOSITE_MINOR_VERSION);
	xcb_composite_query_version_reply_t *pcompReply = xcb_composite_query_version_reply(pbackend->pcon,compCookie,0);
	if(!pcompReply)
		throw Exception("XCompositor unavailable.\n");
	DebugPrintf(stdout,"XComposite %u.%u\n",pcompReply->major_version,pcompReply->minor_version);
	free(pcompReply);

	//overlay
	xcb_composite_get_overlay_window_cookie_t overlayCookie = xcb_composite_get_overlay_window(pbackend->pcon,pbackend->pscr->root);
	xcb_composite_get_overlay_window_reply_t *poverlayReply = xcb_composite_get_overlay_window_reply(pbackend->pcon,overlayCookie,0);
	if(!poverlayReply)
		throw Exception("Unable to get overlay window.\n");
	overlay = poverlayReply->overlay_win;
	free(poverlayReply);

	uint mask = XCB_CW_EVENT_MASK;
	uint values[1] = {XCB_EVENT_MASK_EXPOSURE};
	xcb_change_window_attributes(pbackend->pcon,overlay,mask,values);

	//xfixes
	if(!pbackend->QueryExtension("XFIXES",&xfixesEventOffset,&xfixesErrorOffset))
		throw Exception("XFixes unavailable.\n");
	xcb_xfixes_query_version_cookie_t fixesCookie = xcb_xfixes_query_version(pbackend->pcon,XCB_XFIXES_MAJOR_VERSION,XCB_XFIXES_MINOR_VERSION);
	xcb_xfixes_query_version_reply_t *pfixesReply = xcb_xfixes_query_version_reply(pbackend->pcon,fixesCookie,0);
	if(!pfixesReply)
		throw Exception("XFixes unavailable.\n");
	DebugPrintf(stdout,"XFixes %u.%u\n",pfixesReply->major_version,pfixesReply->minor_version);
	free(pfixesReply);

	//allow overlay input passthrough
	xcb_xfixes_region_t region = xcb_generate_id(pbackend->pcon);
	xcb_void_cookie_t regionCookie = xcb_xfixes_create_region_checked(pbackend->pcon,region,0,0);
	xcb_generic_error_t *perr = xcb_request_check(pbackend->pcon,regionCookie);
	if(perr != 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Unable to create overlay region (%d).",perr->error_code);
		throw Exception();
	}
	xcb_discard_reply(pbackend->pcon,regionCookie.sequence);
	xcb_xfixes_set_window_shape_region(pbackend->pcon,overlay,XCB_SHAPE_SK_BOUNDING,0,0,XCB_XFIXES_REGION_NONE);
	xcb_xfixes_set_window_shape_region(pbackend->pcon,overlay,XCB_SHAPE_SK_INPUT,0,0,region);
	xcb_xfixes_destroy_region(pbackend->pcon,region);

	//damage
	if(!pbackend->QueryExtension("DAMAGE",&damageEventOffset,&damageErrorOffset))
		throw Exception("Damage extension unavailable.");

	xcb_damage_query_version_cookie_t damageCookie = xcb_damage_query_version(pbackend->pcon,XCB_DAMAGE_MAJOR_VERSION,XCB_DAMAGE_MINOR_VERSION);
	xcb_damage_query_version_reply_t *pdamageReply = xcb_damage_query_version_reply(pbackend->pcon,damageCookie,0);
	DebugPrintf(stdout,"Damage %u.%u\n",pdamageReply->major_version,pdamageReply->minor_version);
	free(pdamageReply);

	xcb_flush(pbackend->pcon);

	InitializeRenderEngine();
}

void X11Compositor::Stop(){
	DestroyRenderEngine();

	xcb_xfixes_set_window_shape_region(pbackend->pcon,overlay,XCB_SHAPE_SK_BOUNDING,0,0,XCB_XFIXES_REGION_NONE);
	xcb_xfixes_set_window_shape_region(pbackend->pcon,overlay,XCB_SHAPE_SK_INPUT,0,0,XCB_XFIXES_REGION_NONE);

	xcb_composite_release_overlay_window(pbackend->pcon,overlay);

	xcb_flush(pbackend->pcon);
}

bool X11Compositor::FilterEvent(const Backend::X11Event *pevent){
	if(pevent->pevent->response_type == XCB_DAMAGE_NOTIFY+damageEventOffset){
		xcb_damage_notify_event_t *pev = (xcb_damage_notify_event_t*)pevent->pevent;

		Backend::X11Client *pclient = pevent->pbackend->FindClient(pev->drawable);
		if(!pclient){
			DebugPrintf(stderr,"Unknown damage event.\n");
			return true;
		}

		if(pclient->rect.w < pev->area.x+pev->area.width || pclient->rect.h < pev->area.y+pev->area.height)
			return true; //filter out outdated events after client shrink in size

		//ClientFrame *pclientFrame = dynamic_cast<ClientFrame *>(pclient);
		X11ClientFrame *pclientFrame = dynamic_cast<X11ClientFrame *>(pclient);
		if(std::find(updateQueue.begin(),updateQueue.end(),pclientFrame) == updateQueue.end())
			updateQueue.push_back(pclientFrame);

		VkRect2D rect;
		rect.offset = {pev->area.x,pev->area.y};
		rect.extent = {pev->area.width,pev->area.height};
		pclientFrame->damageRegions.push_back(rect);
		DebugPrintf(stdout,"DAMAGE_EVENT, (%hd,%hd), (%hux%hu)\n",pev->area.x,pev->area.y,pev->area.width,pev->area.height);
		
		return true;
	}

	return false;
}

bool X11Compositor::CheckPresentQueueCompatibility(VkPhysicalDevice physicalDev, uint queueFamilyIndex) const{
	xcb_visualid_t visualid = pbackend->pscr->root_visual;
	return vkGetPhysicalDeviceXcbPresentationSupportKHR(physicalDev,queueFamilyIndex,pbackend->pcon,visualid) == VK_TRUE;
}

void X11Compositor::CreateSurfaceKHR(VkSurfaceKHR *psurface) const{
	VkXcbSurfaceCreateInfoKHR xcbSurfaceCreateInfo = {};
	xcbSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	xcbSurfaceCreateInfo.pNext = 0;
	xcbSurfaceCreateInfo.connection = pbackend->pcon; //pcon
	xcbSurfaceCreateInfo.window = overlay;
	//if(((PFN_vkCreateXcbSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateXcbSurfaceKHR"))(instance,&xcbSurfaceCreateInfo,0,psurface) != VK_SUCCESS)
	if(vkCreateXcbSurfaceKHR(instance,&xcbSurfaceCreateInfo,0,psurface) != VK_SUCCESS)
		throw("Failed to create KHR surface.");
}

VkExtent2D X11Compositor::GetExtent() const{
	xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pbackend->pcon,overlay);
	xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pbackend->pcon,geometryCookie,0);
	if(!pgeometryReply)
		throw("Invalid geometry size - unable to retrieve.");
	VkExtent2D e = (VkExtent2D){pgeometryReply->width,pgeometryReply->height};
	free(pgeometryReply);
	return e;
}

X11DebugClientFrame::X11DebugClientFrame(WManager::Container *pcontainer, const Backend::DebugClient::CreateInfo *_pcreateInfo, CompositorInterface *_pcomp) : DebugClient(pcontainer,_pcreateInfo), ClientFrame(rect.w,rect.h,_pcomp){
	//
}

X11DebugClientFrame::~X11DebugClientFrame(){
	//delete ptexture;
}

void X11DebugClientFrame::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	//
	uint color[3];
	for(uint &t : color)
		t = rand()%255;
	const void *pdata = ptexture->Map();
	for(uint i = 0, n = rect.w*rect.h; i < n; ++i){
		//unsigned char t = (float)(i/rect.w)/(float)rect.h*255;
		((unsigned char*)pdata)[4*i+0] = color[0];
		((unsigned char*)pdata)[4*i+1] = color[1];
		((unsigned char*)pdata)[4*i+2] = color[2];
		((unsigned char*)pdata)[4*i+3] = 255;
	}
	VkRect2D rect1;
	rect1.offset = {0,0};
	rect1.extent = {rect.w,rect.h};
	ptexture->Unmap(pcommandBuffer,&rect1,1);
}

void X11DebugClientFrame::AdjustSurface1(){
	//
	AdjustSurface(rect.w,rect.h);
}

X11DebugCompositor::X11DebugCompositor(uint physicalDevIndex, const Backend::X11Backend *pbackend) : X11Compositor(physicalDevIndex,pbackend){
	//
}

X11DebugCompositor::~X11DebugCompositor(){
	//
}

void X11DebugCompositor::Start(){
	overlay = pbackend->window;

	InitializeRenderEngine();
}

void X11DebugCompositor::Stop(){
	DestroyRenderEngine();
}

NullCompositor::NullCompositor() : CompositorInterface(0){
	//
}

NullCompositor::~NullCompositor(){
	//
}

void NullCompositor::Start(){
	//
}

void NullCompositor::Stop(){
	//
}

bool NullCompositor::CheckPresentQueueCompatibility(VkPhysicalDevice physicalDev, uint queueFamilyIndex) const{
	return true;
}

void NullCompositor::CreateSurfaceKHR(VkSurfaceKHR *psurface) const{
	//
}

VkExtent2D NullCompositor::GetExtent() const{
	return (VkExtent2D){0,0};
}

}

