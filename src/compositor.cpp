#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <set>
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <sys/shm.h>

namespace Compositor{

ColorFrame::ColorFrame(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : pcomp(_pcomp), passignedSet(0), time(0.0f), shaderUserFlags(0), shaderFlags(0), oldShaderFlags(0){
	//
	Pipeline *pPipeline = pcomp->LoadPipeline(pshaderName);
	if(!AssignPipeline(pPipeline))
		throw Exception("Failed to assign a pipeline.");

	clock_gettime(CLOCK_MONOTONIC,&creationTime);
}

ColorFrame::~ColorFrame(){
	for(PipelineDescriptorSet &pipelineDescSet : descSets)
		for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
			if(pipelineDescSet.pdescSets[i])
				pcomp->ReleaseDescSets(pipelineDescSet.p->pshaderModule[i],pipelineDescSet.pdescSets[i]);
}

void ColorFrame::SetShaders(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT]){
	Pipeline *pPipeline = pcomp->LoadPipeline(pshaderName);
	if(!AssignPipeline(pPipeline))
		throw Exception("Failed to assign a pipeline.");
}

void ColorFrame::Draw(const VkRect2D &frame, const glm::vec2 &margin, uint flags, const VkCommandBuffer *pcommandBuffer){
	time = timespec_diff(pcomp->frameTime,creationTime);

	for(uint i = 0, descPointer = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
		if(passignedSet->p->pshaderModule[i]->setCount > 0){
			vkCmdBindDescriptorSets(*pcommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,passignedSet->p->pipelineLayout,descPointer,passignedSet->p->pshaderModule[i]->setCount,passignedSet->pdescSets[i],0,0);
			descPointer += passignedSet->p->pshaderModule[i]->setCount;
		}
	
	struct{
		glm::vec4 frameVec;
		glm::vec2 imageExtent;
		glm::vec2 margin;
		uint flags;
		float time;
	} pushConstants;

	pushConstants.frameVec = {frame.offset.x,frame.offset.y,frame.offset.x+frame.extent.width,frame.offset.y+frame.extent.height};
	pushConstants.frameVec += 0.5f;
	pushConstants.frameVec /= (glm::vec4){pcomp->imageExtent.width,pcomp->imageExtent.height,pcomp->imageExtent.width,pcomp->imageExtent.height};
	pushConstants.frameVec *= 2.0f;
	pushConstants.frameVec -= 1.0f;

	pushConstants.imageExtent = glm::vec2(pcomp->imageExtent.width,pcomp->imageExtent.height);
	pushConstants.margin = margin;
	pushConstants.flags = flags;
	pushConstants.time = time;

	vkCmdPushConstants(*pcommandBuffer,passignedSet->p->pipelineLayout,VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,40,&pushConstants); //size fixed also in CompositorResource VkPushConstantRange

	vkCmdDraw(*pcommandBuffer,1,1,0,0);

	passignedSet->fenceTag = pcomp->frameTag;
}

bool ColorFrame::AssignPipeline(const Pipeline *prenderPipeline){
	auto m = std::find_if(descSets.begin(),descSets.end(),[&](auto &r)->bool{
		return r.p == prenderPipeline && pcomp->frameTag > r.fenceTag+pcomp->swapChainImageCount+1;
	});
	if(m != descSets.end()){
		passignedSet = &(*m);
		return true;
	}

	PipelineDescriptorSet pipelineDescSet;
	pipelineDescSet.fenceTag = pcomp->frameTag;
	pipelineDescSet.p = prenderPipeline;
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		if(!prenderPipeline->pshaderModule[i] || prenderPipeline->pshaderModule[i]->setCount == 0){
			pipelineDescSet.pdescSets[i] = 0;
			continue;
		}
		pipelineDescSet.pdescSets[i] = pcomp->CreateDescSets(prenderPipeline->pshaderModule[i]);
		if(!pipelineDescSet.pdescSets[i])
			return false;
	}
	descSets.push_back(pipelineDescSet);
	passignedSet = &descSets.back();

	return true;
}

ClientFrame::ClientFrame(uint w, uint h, const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : ColorFrame(pshaderName,_pcomp), fullRegionUpdate(true), animationCompleted(true){
	pcomp->updateQueue.push_back(this);

	ptexture = pcomp->CreateTexture(w,h);

	UpdateDescSets();
}

ClientFrame::~ClientFrame(){
	pcomp->updateQueue.erase(std::remove(pcomp->updateQueue.begin(),pcomp->updateQueue.end(),this),pcomp->updateQueue.end());

	pcomp->ReleaseTexture(ptexture);
}

void ClientFrame::AdjustSurface(uint w, uint h){
	pcomp->ReleaseTexture(ptexture);

	pcomp->updateQueue.push_back(this);
	fullRegionUpdate = true;

	ptexture = pcomp->CreateTexture(w,h);
	//In this case updating the descriptor sets would be enough, but we can't do that because of them being used currently by frames in flight.
	if(!AssignPipeline(passignedSet->p))
		throw Exception("Failed to assign a pipeline.");
	DebugPrintf(stdout,"Texture created: %ux%u\n",w,h);

	UpdateDescSets();
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

CompositorInterface::CompositorInterface(const Configuration *pconfig) : physicalDevIndex(pconfig->deviceIndex), currentFrame(0), imageIndex(0), frameTag(0), pcolorBackground(0), pbackground(0), debugLayers(pconfig->debugLayers), scissoring(pconfig->scissoring), playingAnimation(false){
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
	if(debugLayers && layersFound < sizeof(players)/sizeof(players[0]))
		throw Exception("Could not find all required layers.");

	uint extCount;
	vkEnumerateInstanceExtensionProperties(0,&extCount,0);
	VkExtensionProperties *pextProps = new VkExtensionProperties[extCount];
	vkEnumerateInstanceExtensionProperties(0,&extCount,pextProps);

	const char *pextensions[] = {
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		"VK_KHR_surface",
		"VK_KHR_xcb_surface",
		//"VK_KHR_get_physical_device_properties2",
		//VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME
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
	if(debugLayers){
		DebugPrintf(stdout,"Debug layers enabled.\n");
		//also in vkCreateDevice
		instanceCreateInfo.enabledLayerCount = sizeof(players)/sizeof(players[0]);
		instanceCreateInfo.ppEnabledLayerNames = players;
	}else{
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = 0;
	}
	instanceCreateInfo.enabledExtensionCount = sizeof(pextensions)/sizeof(pextensions[0]);
	instanceCreateInfo.ppEnabledExtensionNames = pextensions;
	if(vkCreateInstance(&instanceCreateInfo,0,&instance) != VK_SUCCESS)
		throw Exception("Failed to create Vulkan instance.");
	
	//delete []playerProps;
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

	VkPhysicalDeviceProperties *pdevProps = new VkPhysicalDeviceProperties[devCount];
	for(uint i = 0; i < devCount; ++i){
		vkGetPhysicalDeviceProperties(pdevices[i],&pdevProps[i]);
		VkPhysicalDeviceFeatures devFeatures;
		vkGetPhysicalDeviceFeatures(pdevices[i],&devFeatures);

		printf("%c %u: %s\n\t.deviceID: %u\n\t.vendorID: %u\n\t.deviceType: %u\n",
			i == physicalDevIndex?'*':' ',
			i,pdevProps[i].deviceName,pdevProps[i].deviceID,pdevProps[i].vendorID,pdevProps[i].deviceType);
		printf("  max push constant size: %u\n  max bound desc sets: %u\n  max viewports: %u\n  multi viewport: %u\n",
			pdevProps[i].limits.maxPushConstantsSize,pdevProps[i].limits.maxBoundDescriptorSets,pdevProps[i].limits.maxViewports,devFeatures.multiViewport);
	}

	if(physicalDevIndex >= devCount){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Invalid gpu-index (%u) exceeds the number of available devices (%u).",physicalDevIndex,devCount);
		throw Exception();
	}

	physicalDev = pdevices[physicalDevIndex];
	physicalDevProps = pdevProps[physicalDevIndex];

	delete []pdevices;
	delete []pdevProps;

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
		queueFamilyIndex[i] = ~0u;
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
	//physicalDevFeatures.multiViewport = VK_TRUE;
	
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
	if(debugLayers){
		devCreateInfo.ppEnabledLayerNames = players;
		devCreateInfo.enabledLayerCount = sizeof(players)/sizeof(players[0]);
	}else{
		devCreateInfo.ppEnabledLayerNames = 0;
		devCreateInfo.enabledLayerCount = 0;
	}
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
	attachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;//VK_ATTACHMENT_LOAD_OP_CLEAR;
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
	/*VkDescriptorPoolSize descPoolSizes[2];
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
	descPoolCreateInfo.maxSets = 16;
	descPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	if(vkCreateDescriptorPool(logicalDev,&descPoolCreateInfo,0,&descPool) != VK_SUCCESS)
		throw Exception("Failed to create a descriptor pool.");*/

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
	
	shaders.reserve(1024);

	pipelines.reserve(1024);
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

	for(VkDescriptorPool &descPool : descPoolArray)
		vkDestroyDescriptorPool(logicalDev,descPool,0);
	descPoolArray.clear();

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

void CompositorInterface::AddShader(const char *pname, const Blob *pblob){
	shaders.emplace_back(pname,pblob,this);
}

void CompositorInterface::AddDamageRegion(const VkRect2D *prect){
	if(!scissoring)
		return;
	//find the regions that are completely covered by the param and replace them all with the one larger area
	scissorRegions.erase(std::remove_if(scissorRegions.begin(),scissorRegions.end(),[&](auto &scissorRegion)->bool{
		return prect->offset.x <= scissorRegion.first.offset.x && prect->offset.y <= scissorRegion.first.offset.y
			&& prect->offset.x+prect->extent.width >= scissorRegion.first.offset.x+scissorRegion.first.extent.width
			&& prect->offset.y+prect->extent.height >= scissorRegion.first.offset.y+scissorRegion.first.extent.height;
	}),scissorRegions.end());
	scissorRegions.push_back(std::pair<VkRect2D, uint>(*prect,0));
}

void CompositorInterface::AddDamageRegion(const WManager::Client *pclient){
	if(!scissoring)
		return;
	glm::ivec2 margin = 4*glm::ivec2(
		pclient->pcontainer->margin.x*(float)imageExtent.width,
		pclient->pcontainer->margin.y*(float)imageExtent.width); //due to aspect, this must be *width
	
	VkRect2D rect1;
	//rect1.offset = {(sint)pclient->position.x-margin.x,(sint)pclient->position.y-margin.y};
	rect1.offset = {pclient->rect.x-margin.x,pclient->rect.y-margin.y};
	rect1.extent = {pclient->rect.w+2*margin.x,pclient->rect.h+2*margin.y};
	AddDamageRegion(&rect1);
	rect1.offset = {pclient->oldRect.x-margin.x,pclient->oldRect.y-margin.y};
	rect1.extent = {pclient->oldRect.w+2*margin.x,pclient->oldRect.h+2*margin.y};
	AddDamageRegion(&rect1);
}

void CompositorInterface::WaitIdle(){
	vkDeviceWaitIdle(logicalDev);
}

void CompositorInterface::CreateRenderQueueAppendix(const WManager::Client *pclient, const WManager::Container *pfocus){
	auto s = [&](auto &p)->bool{
		return pclient == p.first;
	};
	for(auto m = std::find_if(appendixQueue.begin(),appendixQueue.end(),s);
		m != appendixQueue.end(); m = std::find_if(m,appendixQueue.end(),s)){
		RenderObject renderObject;
		renderObject.pclient = (*m).second;
		renderObject.pclientFrame = dynamic_cast<ClientFrame *>((*m).second);
		renderObject.pclientFrame->oldShaderFlags = renderObject.pclientFrame->shaderFlags;
		renderObject.pclientFrame->shaderFlags = (renderObject.pclient->pcontainer == pfocus?0x1:0)|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);

		CreateRenderQueueAppendix((*m).second,pfocus);

		m = appendixQueue.erase(m);
	}
}

void CompositorInterface::CreateRenderQueue(const WManager::Container *pcontainer, const WManager::Container *pfocus){
	for(const WManager::Container *pcont : pcontainer->stackQueue){
		if(pcont->pclient){
			ClientFrame *pclientFrame = dynamic_cast<ClientFrame *>(pcont->pclient);
			if(!pclientFrame)
				continue;
			
			RenderObject renderObject;
			renderObject.pclient = pcont->pclient;
			renderObject.pclientFrame = pclientFrame;
			renderObject.pclientFrame->oldShaderFlags = renderObject.pclientFrame->shaderFlags;
			renderObject.pclientFrame->shaderFlags =
				(pcont == pfocus || pcontainer == pfocus?0x1:0)|renderObject.pclientFrame->shaderUserFlags;
			renderQueue.push_back(renderObject);
		}
		CreateRenderQueue(pcont,pfocus);
	}

	if(!pcontainer->pclient)
		return;
	CreateRenderQueueAppendix(pcontainer->pclient,pfocus);
}

bool CompositorInterface::PollFrameFence(){
	if(vkAcquireNextImageKHR(logicalDev,swapChain,std::numeric_limits<uint64_t>::max(),psemaphore[currentFrame][SEMAPHORE_INDEX_IMAGE_AVAILABLE],0,&imageIndex) != VK_SUCCESS)
		throw Exception("Failed to acquire a swap chain image.\n");
	
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
		auto m = std::find_if(descPoolReference.begin(),descPoolReference.end(),[&](auto &p)->bool{
			return descSetCacheEntry.pdescSets == p.first;
		});
		if(m == descPoolReference.end())
			return true;
		vkFreeDescriptorSets(logicalDev,(*m).second,descSetCacheEntry.setCount,descSetCacheEntry.pdescSets);
		descPoolReference.erase(m);
		printf("************ releasing desc set\n");

		delete []descSetCacheEntry.pdescSets;
		return true;
	}),descSetCache.end());
	return true;
}

void CompositorInterface::GenerateCommandBuffers(const WManager::Container *proot, const std::vector<std::pair<const WManager::Client *, WManager::Client *>> *pstackAppendix, const WManager::Container *pfocus){
	if(!proot)
		return;
	
	//Create a render list elements arranged from back to front
	renderQueue.clear();
	appendixQueue.clear();
	for(auto &p : *pstackAppendix){
		if(p.first){
			appendixQueue.push_back(p);
			continue;
		}
		//desktop features are placed first
		RenderObject renderObject;
		renderObject.pclient = p.second;
		renderObject.pclientFrame = dynamic_cast<ClientFrame *>(p.second);
		renderObject.pclientFrame->oldShaderFlags = renderObject.pclientFrame->shaderFlags;
		renderObject.pclientFrame->shaderFlags = (renderObject.pclient->pcontainer == pfocus?0x1:0)|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);
	}

	CreateRenderQueue(proot,pfocus);

	for(auto m = appendixQueue.begin(); m != appendixQueue.end();){
		auto k = std::find_if(appendixQueue.begin(),appendixQueue.end(),[&](auto &p)->bool{
			return (*m).first == p.second;
		});
		if(k != appendixQueue.end()){
			++m;
			continue;
		}
			
		RenderObject renderObject;
		renderObject.pclient = (*m).second;
		renderObject.pclientFrame = dynamic_cast<ClientFrame *>((*m).second);
		renderObject.pclientFrame->oldShaderFlags = renderObject.pclientFrame->shaderFlags;
		renderObject.pclientFrame->shaderFlags = (renderObject.pclient->pcontainer == pfocus?0x1:0)|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);

		CreateRenderQueueAppendix((*m).second,pfocus);

		m = appendixQueue.erase(m);
	}

	for(auto &p : appendixQueue){ //push the remaining (untransient) windows to the end of the queue
		RenderObject renderObject;
		renderObject.pclient = p.second;
		renderObject.pclientFrame = dynamic_cast<ClientFrame *>(p.second);
		renderObject.pclientFrame->oldShaderFlags = renderObject.pclientFrame->shaderFlags;
		renderObject.pclientFrame->shaderFlags = (renderObject.pclient->pcontainer == pfocus?0x1:0)|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);
	}

	playingAnimation = false;
	clock_gettime(CLOCK_MONOTONIC,&frameTime);

	for(uint i = 0; i < renderQueue.size(); ++i){
		RenderObject &renderObject = renderQueue[i];

		float t = timespec_diff(frameTime,renderObject.pclient->translationTime);
		float s = std::clamp(t/0.3f,0.0f,1.0f);
		s = 1.0f/(1.0f+expf(-10.0f*s+5.0f));

		glm::vec2 oldRect1 = glm::vec2(renderObject.pclient->oldRect.x,renderObject.pclient->oldRect.y);
		renderObject.pclient->position = oldRect1+s*(glm::vec2(renderObject.pclient->rect.x,renderObject.pclient->rect.y)-oldRect1);
		if(s < 0.99f
			&& (renderObject.pclient->oldRect.x != renderObject.pclient->rect.x || renderObject.pclient->oldRect.y != renderObject.pclient->rect.y)
			&& !(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FLOATING)){
			AddDamageRegion(renderObject.pclient); //need to keep adding the client for as long as the animation is playing

			playingAnimation = true;
			renderObject.pclientFrame->animationCompleted = false;

		}else
		if(!renderObject.pclientFrame->animationCompleted){
			AddDamageRegion(renderObject.pclient);
			renderObject.pclientFrame->animationCompleted = true;

			renderObject.pclient->position = glm::vec2(renderObject.pclient->rect.x,renderObject.pclient->rect.y);

		}else renderObject.pclient->position = glm::vec2(renderObject.pclient->rect.x,renderObject.pclient->rect.y);

		if(renderObject.pclientFrame->shaderFlags != renderObject.pclientFrame->oldShaderFlags)
			AddDamageRegion(renderObject.pclient);
	}
	
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = 0;
	if(vkBeginCommandBuffer(pcopyCommandBuffers[currentFrame],&commandBufferBeginInfo) != VK_SUCCESS)
		throw Exception("Failed to begin command buffer recording.");

	ClientFrame *pbackground1 = dynamic_cast<ClientFrame *>(pbackground);
	if(pbackground1)
		pbackground1->UpdateContents(&pcopyCommandBuffers[currentFrame]);

	for(ClientFrame *pclientFrame : updateQueue)
		pclientFrame->UpdateContents(&pcopyCommandBuffers[currentFrame]);
	updateQueue.clear();

	if(vkEndCommandBuffer(pcopyCommandBuffers[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to end command buffer recording.");

	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	if(vkBeginCommandBuffer(pcommandBuffers[currentFrame],&commandBufferBeginInfo) != VK_SUCCESS)
		throw Exception("Failed to begin command buffer recording.");

	//TODO: debug option: draw frames around scissor area
	if(scissoring){
		glm::ivec2 a = glm::ivec2(imageExtent.width,imageExtent.height);
		glm::ivec2 b = glm::ivec2(0);
		for(auto m = scissorRegions.begin(); m != scissorRegions.end();){
			if(((*m).second & 1<<imageIndex) == 0){
				glm::ivec2 a1 = glm::ivec2((*m).first.offset.x,(*m).first.offset.y);
				glm::ivec2 b1 = a1+glm::ivec2((*m).first.extent.width,(*m).first.extent.height);
				a = glm::min(a,a1);
				b = glm::max(b,b1);

				(*m).second |= 1<<imageIndex;
			}

			//As soon as the rectangle has been drawn for every image of the swap chain, remove it
			if(~(~0u<<swapChainImageCount) == (*m).second)
				m = scissorRegions.erase(m);
			else ++m;
		}

		VkRect2D scissor;
		scissor.offset = {std::max(a.x,0),std::max(a.y,0)};
		scissor.extent = {std::max(b.x-scissor.offset.x,0),std::max(b.y-scissor.offset.y,0)};
		vkCmdSetScissor(pcommandBuffers[currentFrame],0,1,&scissor);
	}

	//static const VkClearValue clearValue = {1.0f,1.0f,1.0f,1.0f};
	VkRenderPassBeginInfo renderPassBeginInfo = {};
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.framebuffer = pframebuffers[imageIndex];
	renderPassBeginInfo.renderArea.offset = {0,0};
	renderPassBeginInfo.renderArea.extent = imageExtent;
	renderPassBeginInfo.clearValueCount = 0;//1;
	renderPassBeginInfo.pClearValues = 0;//&clearValue;
	vkCmdBeginRenderPass(pcommandBuffers[currentFrame],&renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

	if(pbackground){
		vkCmdBindPipeline(pcommandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,pbackground->passignedSet->p->pipeline);

		VkRect2D screenRect;
		screenRect.offset = {0,0};
		screenRect.extent = imageExtent;
		pbackground->Draw(screenRect,glm::vec2(0.0f),0,&pcommandBuffers[currentFrame]);

	}

	//for(RenderObject &renderObject : renderQueue){
	for(uint i = 0; i < renderQueue.size(); ++i){
		RenderObject &renderObject = renderQueue[i];

		if((sint)renderObject.pclient->position.x+renderObject.pclient->rect.w <= 1 || (sint)renderObject.pclient->position.y+renderObject.pclient->rect.h <= 1
			|| (sint)renderObject.pclient->position.x > (sint)imageExtent.width || (sint)renderObject.pclient->position.y > (sint)imageExtent.height)
			continue;

		VkRect2D frame;
		frame.offset = {(sint)renderObject.pclient->position.x,(sint)renderObject.pclient->position.y};
		frame.extent = {renderObject.pclient->rect.w,renderObject.pclient->rect.h};
		//frame.offset = {renderObject.pclient->rect.x,renderObject.pclient->rect.y};
		//frame.extent = {renderObject.pclient->rect.w,renderObject.pclient->rect.h};

		/*VkRect2D scissor = frame;
		for(uint j = i+1; j < renderQueue.size(); ++j){
			RenderObject &renderObject1 = renderQueue[j];

			if(frame.offset.y >= renderObject1.pclient->rect.y-1 &&
				frame.offset.y+frame.extent.height <= renderObject1.pclient->rect.y+renderObject1.pclient->rect.h+1){
				if(scissor.offset.x+scissor.extent.width > renderObject1.pclient->rect.x &&
					scissor.offset.x < renderObject1.pclient->rect.x)
					scissor.extent.width = renderObject1.pclient->rect.x-scissor.offset.x;

				if(renderObject1.pclient->rect.x+renderObject1.pclient->rect.w > scissor.offset.x &&
					renderObject1.pclient->rect.x < scissor.offset.x){
					sint oldOffset = scissor.offset.x;
					scissor.offset.x = renderObject1.pclient->rect.x+renderObject1.pclient->rect.w;
					scissor.extent.width -= scissor.offset.x-oldOffset;
				}
			}
		}*/

		vkCmdBindPipeline(pcommandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,renderObject.pclientFrame->passignedSet->p->pipeline);

		//renderObject.pclientFrame->Draw(frame,renderObject.pclient->pcontainer->margin,renderObject.flags,&pcommandBuffers[currentFrame]);
		renderObject.pclientFrame->Draw(frame,renderObject.pclient->pcontainer->margin,renderObject.pclientFrame->shaderFlags,&pcommandBuffers[currentFrame]);
	}

	vkCmdEndRenderPass(pcommandBuffers[currentFrame]);

	if(vkEndCommandBuffer(pcommandBuffers[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to end command buffer recording.");
}

void CompositorInterface::Present(){
	/*uint imageIndex;
	if(vkAcquireNextImageKHR(logicalDev,swapChain,std::numeric_limits<uint64_t>::max(),psemaphore[currentFrame][SEMAPHORE_INDEX_IMAGE_AVAILABLE],0,&imageIndex) != VK_SUCCESS)
		throw Exception("Failed to acquire a swap chain image.\n");*/
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

Pipeline * CompositorInterface::LoadPipeline(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT]){
	auto m = std::find_if(pipelines.begin(),pipelines.end(),[&](auto &r)->bool{
		for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
			if(strcmp(r.pshaderModule[i]->pname,pshaderName[i]) != 0)
				return false;
		return true;
	});
	Pipeline *pPipeline;
	if(m != pipelines.end())
		pPipeline = &(*m);
	else{
		ShaderModule *pshader[Pipeline::SHADER_MODULE_COUNT];
		for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
			auto n = std::find_if(shaders.begin(),shaders.end(),[&](auto &r)->bool{
				return strcmp(r.pname,pshaderName[i]) == 0;
			});
			if(n == shaders.end()){
				snprintf(Exception::buffer,sizeof(Exception::buffer),"Shader not found: %s.",pshaderName[i]);
				throw Exception();
			}
			pshader[i] = &(*n);
		}
		pipelines.reserve(pipelines.size());
		pPipeline = &pipelines.emplace_back(
			pshader[Pipeline::SHADER_MODULE_VERTEX],
			pshader[Pipeline::SHADER_MODULE_GEOMETRY],
			pshader[Pipeline::SHADER_MODULE_FRAGMENT],this);
	}
	return pPipeline;
}

void CompositorInterface::ClearBackground(){
	ClientFrame *pbackground1 = dynamic_cast<ClientFrame *>(pbackground);
	if(pbackground1)
		delete pbackground1;
	if(!pcolorBackground){
		static const char *pshaderName[Pipeline::SHADER_MODULE_COUNT] = {
			"default_vertex.spv","default_geometry.spv","solid_fragment.spv"
		};
		pcolorBackground = new ColorFrame(pshaderName,this);
	}

	pbackground = pcolorBackground;
	
	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = imageExtent;//{imageExtent.width,h};
	AddDamageRegion(&screenRect);
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

	for(VkDescriptorPool &descPool : descPoolArray){
		VkDescriptorSetAllocateInfo descSetAllocateInfo = {};
		descSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descSetAllocateInfo.descriptorPool = descPool;
		descSetAllocateInfo.pSetLayouts = pshaderModule->pdescSetLayouts;
		descSetAllocateInfo.descriptorSetCount = pshaderModule->setCount;
		if(vkAllocateDescriptorSets(logicalDev,&descSetAllocateInfo,pdescSets) == VK_SUCCESS){
			descPoolReference.push_back(std::pair<VkDescriptorSet *, VkDescriptorPool>(pdescSets,descPool));
			return pdescSets;
		}
	}

	VkDescriptorPoolSize descPoolSizes[2];
	descPoolSizes[0] = (VkDescriptorPoolSize){};
	descPoolSizes[0].type = VK_DESCRIPTOR_TYPE_SAMPLER;//VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	descPoolSizes[0].descriptorCount = 16;

	descPoolSizes[1] = (VkDescriptorPoolSize){};
	descPoolSizes[1].type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	descPoolSizes[1].descriptorCount = 16;

	VkDescriptorPool descPool;

	//if there are no pools or they are all out of memory, attempt to create a new one
	VkDescriptorPoolCreateInfo descPoolCreateInfo = {};
	descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descPoolCreateInfo.poolSizeCount = sizeof(descPoolSizes)/sizeof(descPoolSizes[0]);
	descPoolCreateInfo.pPoolSizes = descPoolSizes;
	descPoolCreateInfo.maxSets = 16;
	descPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	if(vkCreateDescriptorPool(logicalDev,&descPoolCreateInfo,0,&descPool) != VK_SUCCESS){
		delete []pdescSets;
		return 0;
	}
	descPoolArray.push_front(descPool);
	
	VkDescriptorSetAllocateInfo descSetAllocateInfo = {};
	descSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descSetAllocateInfo.descriptorPool = descPool;
	descSetAllocateInfo.pSetLayouts = pshaderModule->pdescSetLayouts;
	descSetAllocateInfo.descriptorSetCount = pshaderModule->setCount;
	if(vkAllocateDescriptorSets(logicalDev,&descSetAllocateInfo,pdescSets) == VK_SUCCESS){
		descPoolReference.push_back(std::pair<VkDescriptorSet *, VkDescriptorPool>(pdescSets,descPool));
		return pdescSets;
	}

	delete []pdescSets;
	return 0;
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

X11ClientFrame::X11ClientFrame(WManager::Container *pcontainer, const Backend::X11Client::CreateInfo *_pcreateInfo, const char *_pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : X11Client(pcontainer,_pcreateInfo), ClientFrame(rect.w,rect.h,_pshaderName,_pcomp){
	xcb_composite_redirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);

	windowPixmap = xcb_generate_id(pbackend->pcon);
	xcb_composite_name_window_pixmap(pbackend->pcon,window,windowPixmap);

	segment = xcb_generate_id(pbackend->pcon);

	damage = xcb_generate_id(pbackend->pcon);
	xcb_damage_create(pbackend->pcon,damage,window,XCB_DAMAGE_REPORT_LEVEL_RAW_RECTANGLES);

	pcomp->AddDamageRegion(this);
}

X11ClientFrame::~X11ClientFrame(){
	xcb_damage_destroy(pbackend->pcon,damage);
	//
	xcb_composite_unredirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);
	xcb_free_pixmap(pbackend->pcon,windowPixmap);

	pcomp->AddDamageRegion(this);
}

void X11ClientFrame::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	if(!fullRegionUpdate && damageRegions.size() == 0)
		return;

	if(fullRegionUpdate){
		damageRegions.clear();

		VkRect2D rect1;
		rect1.offset = {0,0};
		rect1.extent = {rect.w,rect.h};
		damageRegions.push_back(rect1);
	}

	unsigned char *pdata = (unsigned char *)ptexture->Map();

	/*struct timespec t1;
	clock_gettime(CLOCK_MONOTONIC,&t1);*/

	for(VkRect2D &rect1 : damageRegions){
		sint shmid = shmget(IPC_PRIVATE,rect1.extent.width*rect1.extent.height*4,IPC_CREAT|0777);
		if(shmid == -1){
			DebugPrintf(stderr,"Failed to allocate shared memory.\n");
			return;
		}
		
		xcb_shm_attach(pbackend->pcon,segment,shmid,0);
		xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(pbackend->pcon,windowPixmap,rect1.offset.x,rect1.offset.y,rect1.extent.width,rect1.extent.height,~0u,XCB_IMAGE_FORMAT_Z_PIXMAP,segment,0);
		xcb_shm_detach(pbackend->pcon,segment);

		xcb_shm_get_image_reply_t *pimageReply = xcb_shm_get_image_reply(pbackend->pcon,imageCookie,0);
		xcb_flush(pbackend->pcon);

		if(!pimageReply){
			shmctl(shmid,IPC_RMID,0);
			DebugPrintf(stderr,"No shared memory.\n");
			return;
		}

		uint depth = pimageReply->depth;
		free(pimageReply);

		unsigned char *pchpixels = (unsigned char*)shmat(shmid,0,0);
		shmctl(shmid,IPC_RMID,0);

		for(uint y = 0; y < rect1.extent.height; ++y){
			uint offsetDst = 4*(rect.w*(y+rect1.offset.y)+rect1.offset.x);
			uint offsetSrc = 4*(rect1.extent.width*y);
			//uint offsetSrc = offsetDst;
			memcpy(pdata+offsetDst,pchpixels+offsetSrc,4*rect1.extent.width);
			if(depth != 32)
				for(uint i = 0; i < rect1.extent.width; ++i)
					pdata[offsetDst+4*i+3] = 255;
		}

		shmdt(pchpixels);

		VkRect2D screenRect;
		screenRect.offset = {(sint)position.x+rect1.offset.x,(sint)position.y+rect1.offset.y};
		screenRect.extent = rect1.extent;
		pcomp->AddDamageRegion(&screenRect);
	}

	ptexture->Unmap(pcommandBuffer,damageRegions.data(),damageRegions.size());

	/*struct timespec t3;
	clock_gettime(CLOCK_MONOTONIC,&t3);
	float dt2 = timespec_diff(t3,t2);

	FILE *pf = fopen("/tmp/timelog","a+");
	fprintf(pf,"%x (%ux%u): %f + %f = %f\n",this,rect.w,rect.h,dt1,dt2,dt1+dt2);
	fclose(pf);*/

	damageRegions.clear();
}

void X11ClientFrame::AdjustSurface1(){
	if(oldRect.w != rect.w || oldRect.h != rect.h){
		xcb_free_pixmap(pbackend->pcon,windowPixmap);
		xcb_composite_name_window_pixmap(pbackend->pcon,window,windowPixmap);

		AdjustSurface(rect.w,rect.h);
		pcomp->AddDamageRegion(this);
	}else
	if(oldRect.x != rect.x || oldRect.y != rect.y)
		pcomp->AddDamageRegion(this);
}

X11Background::X11Background(xcb_pixmap_t _pixmap, uint _w, uint _h, const char *_pshaderName[Pipeline::SHADER_MODULE_COUNT], X11Compositor *_pcomp) : w(_w), h(_h), ClientFrame(_w,_h,_pshaderName,_pcomp), pcomp11(_pcomp), pixmap(_pixmap){
	//
	segment = xcb_generate_id(pcomp11->pbackend->pcon);

	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = {w,h};
	pcomp->AddDamageRegion(&screenRect);
}

X11Background::~X11Background(){
	//
	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = {w,h};
	pcomp->AddDamageRegion(&screenRect);
}

void X11Background::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	if(!fullRegionUpdate)
		return;
	//
	/*xcb_get_image_cookie_t imageCookie = xcb_get_image_unchecked(pcomp11->pbackend->pcon,XCB_IMAGE_FORMAT_Z_PIXMAP,pixmap,0,0,w,h,~0u);
	xcb_get_image_reply_t *pimageReply = xcb_get_image_reply(pcomp11->pbackend->pcon,imageCookie,0);
	if(!pimageReply){
		DebugPrintf(stderr,"Failed to receive image reply.\n");
		return;
	}

	unsigned char *pchpixels = xcb_get_image_data(pimageReply);*/

	sint shmid = shmget(IPC_PRIVATE,w*h*4,IPC_CREAT|0777);
	if(shmid == -1){
		DebugPrintf(stderr,"Failed to allocate shared memory.\n");
		return;
	}
	
	xcb_shm_attach(pcomp11->pbackend->pcon,segment,shmid,0);
	xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(pcomp11->pbackend->pcon,pixmap,0,0,w,h,~0u,XCB_IMAGE_FORMAT_Z_PIXMAP,segment,0);
	xcb_shm_detach(pcomp11->pbackend->pcon,segment);

	xcb_shm_get_image_reply_t *pimageReply = xcb_shm_get_image_reply(pcomp11->pbackend->pcon,imageCookie,0);
	xcb_flush(pcomp11->pbackend->pcon);

	if(!pimageReply){
		shmctl(shmid,IPC_RMID,0);
		DebugPrintf(stderr,"No shared memory.\n");
		return;
	}

	uint depth = pimageReply->depth;
	free(pimageReply);

	unsigned char *pchpixels = (unsigned char*)shmat(shmid,0,0);
	shmctl(shmid,IPC_RMID,0);

	unsigned char *pdata = (unsigned char *)ptexture->Map();
	memcpy(pdata,pchpixels,w*h*4);
	if(depth != 32)
		for(uint i = 0; i < w*h; ++i)
			pdata[4*i+3] = 255;
	fullRegionUpdate = false;
	
	VkRect2D rect1 = {0,0,w,h};
	ptexture->Unmap(pcommandBuffer,&rect1,1);

	shmdt(pchpixels);

	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = {w,h};
	pcomp->AddDamageRegion(&screenRect);
}

X11Compositor::X11Compositor(const Configuration *_pconfig, const Backend::X11Backend *_pbackend) : CompositorInterface(_pconfig), pbackend(_pbackend){
	//
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
	DebugPrintf(stdout,"overlay xid: %u\n",overlay);

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
		throw Exception("Damage extension unavailable.\n");

	xcb_damage_query_version_cookie_t damageCookie = xcb_damage_query_version(pbackend->pcon,XCB_DAMAGE_MAJOR_VERSION,XCB_DAMAGE_MINOR_VERSION);
	xcb_damage_query_version_reply_t *pdamageReply = xcb_damage_query_version_reply(pbackend->pcon,damageCookie,0);
	if(!pdamageReply)
		throw Exception("Damage extension unavailable.\n");
	DebugPrintf(stdout,"Damage %u.%u\n",pdamageReply->major_version,pdamageReply->minor_version);
	free(pdamageReply);

	xcb_shm_query_version_cookie_t shmCookie = xcb_shm_query_version(pbackend->pcon);
	xcb_shm_query_version_reply_t *pshmReply = xcb_shm_query_version_reply(pbackend->pcon,shmCookie,0);
	if(!pshmReply || !pshmReply->shared_pixmaps)
		throw Exception("SHM extension unavailable.\n");
	DebugPrintf(stdout,"SHM %u.%u\n",pshmReply->major_version,pshmReply->minor_version);
	free(pshmReply);

	xcb_flush(pbackend->pcon);

	InitializeRenderEngine();
}

void X11Compositor::Stop(){
	if(dynamic_cast<ClientFrame *>(pbackground))
		delete pbackground;
	if(pcolorBackground)
		delete pcolorBackground;
	DestroyRenderEngine();

	xcb_xfixes_set_window_shape_region(pbackend->pcon,overlay,XCB_SHAPE_SK_BOUNDING,0,0,XCB_XFIXES_REGION_NONE);
	xcb_xfixes_set_window_shape_region(pbackend->pcon,overlay,XCB_SHAPE_SK_INPUT,0,0,XCB_XFIXES_REGION_NONE);

	xcb_composite_release_overlay_window(pbackend->pcon,overlay);

	xcb_flush(pbackend->pcon);
}

bool X11Compositor::FilterEvent(const Backend::X11Event *pevent){
	if(pevent->pevent->response_type == XCB_DAMAGE_NOTIFY+damageEventOffset){
		xcb_damage_notify_event_t *pev = (xcb_damage_notify_event_t*)pevent->pevent;

		Backend::X11Client *pclient = pevent->pbackend->FindClient(pev->drawable,Backend::X11Backend::MODE_UNDEFINED);
		if(!pclient){
			DebugPrintf(stderr,"Unknown damage event.\n");
			return true;
		}

		if(pclient->rect.w < pev->area.x+pev->area.width || pclient->rect.h < pev->area.y+pev->area.height)
			return true; //filter out outdated events after client shrink in size

		X11ClientFrame *pclientFrame = dynamic_cast<X11ClientFrame *>(pclient);
		if(std::find(updateQueue.begin(),updateQueue.end(),pclientFrame) == updateQueue.end())
			updateQueue.push_back(pclientFrame);

		VkRect2D rect;
		rect.offset = {pev->area.x,pev->area.y};
		rect.extent = {pev->area.width,pev->area.height};
		pclientFrame->damageRegions.push_back(rect);
		//DebugPrintf(stdout,"DAMAGE_EVENT, %x, (%hd,%hd), (%hux%hu)\n",pev->drawable,pev->area.x,pev->area.y,pev->area.width,pev->area.height);
		
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

void X11Compositor::SetBackgroundPixmap(const Backend::BackendPixmapProperty *pPixmapProperty){
	if(pbackground){
		delete pbackground;
		pbackground = pcolorBackground;
	}
	if(pPixmapProperty->pixmap != 0){
		xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pbackend->pcon,pPixmapProperty->pixmap);
		xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pbackend->pcon,geometryCookie,0);
		if(!pgeometryReply)
			throw("Invalid geometry size - unable to retrieve.");

		static const char *pshaderName[Pipeline::SHADER_MODULE_COUNT] = {
			"default_vertex.spv","default_geometry.spv","default_fragment.spv"
		};
		pbackground = new X11Background(pPixmapProperty->pixmap,pgeometryReply->width,pgeometryReply->height,pshaderName,this);
		printf("background set!\n");
	}
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

X11DebugClientFrame::X11DebugClientFrame(WManager::Container *pcontainer, const Backend::DebugClient::CreateInfo *_pcreateInfo, const char *_pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : DebugClient(pcontainer,_pcreateInfo), ClientFrame(rect.w,rect.h,_pshaderName,_pcomp){
	//
	pcomp->AddDamageRegion(this);
}

X11DebugClientFrame::~X11DebugClientFrame(){
	//delete ptexture;
	pcomp->AddDamageRegion(this);
}

void X11DebugClientFrame::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	//
	uint color[3];
	for(uint &t : color)
		//t = rand()%255;
		t = rand()%190;
	const void *pdata = ptexture->Map();
	for(uint i = 0, n = rect.w*rect.h; i < n; ++i){
		//unsigned char t = (float)(i/rect.w)/(float)rect.h*255;
		((unsigned char*)pdata)[4*i+0] = color[0];
		((unsigned char*)pdata)[4*i+1] = color[1];
		((unsigned char*)pdata)[4*i+2] = color[2];
		((unsigned char*)pdata)[4*i+3] = 190;//255;
	}
	VkRect2D rect1;
	rect1.offset = {0,0};
	rect1.extent = {rect.w,rect.h};
	ptexture->Unmap(pcommandBuffer,&rect1,1);

	pcomp->AddDamageRegion(&rect1);
}

void X11DebugClientFrame::AdjustSurface1(){
	if(oldRect.w != rect.w || oldRect.h != rect.h){
		AdjustSurface(rect.w,rect.h);
		pcomp->AddDamageRegion(this);
	}else
	if(oldRect.x != rect.x || oldRect.y != rect.y)
		pcomp->AddDamageRegion(this);
}

X11DebugCompositor::X11DebugCompositor(const Configuration *_pconfig, const Backend::X11Backend *pbackend) : X11Compositor(_pconfig,pbackend){
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

NullCompositor::NullCompositor() : CompositorInterface(&config){
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

