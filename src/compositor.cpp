#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"
#include "CompositorFont.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <sys/shm.h>
#include <boost/container_hash/hash.hpp>

//#include <gbm.h>
#include <fcntl.h>
#include <unistd.h>

namespace Compositor{

Drawable::Drawable(Pipeline *pPipeline, CompositorInterface *_pcomp) : pcomp(_pcomp), passignedSet(0){
	if(!AssignPipeline(pPipeline))
		throw Exception("Failed to assign a pipeline");
}

Drawable::~Drawable(){
	for(PipelineDescriptorSet &pipelineDescSet : descSets)
		for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
			if(pipelineDescSet.p->pshaderModule[i] && pipelineDescSet.pdescSets[i])
				pcomp->ReleaseDescSets(pipelineDescSet.p->pshaderModule[i],pipelineDescSet.pdescSets[i]);
}

bool Drawable::AssignPipeline(const Pipeline *prenderPipeline){
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
		if(!prenderPipeline->pshaderModule[i])
			continue;
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

void Drawable::BindShaderResources(const std::vector<std::pair<ShaderModule::VARIABLE, const void *>> *pVarAddrs, const VkCommandBuffer *pcommandBuffer){
	alignas(16) char pushConstantBuffer[128];
	for(uint i = 0, p = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		//bind descriptor sets
		if(!passignedSet->p->pshaderModule[i])
			continue;
		if(passignedSet->p->pshaderModule[i]->setCount > 0){
			vkCmdBindDescriptorSets(*pcommandBuffer,VK_PIPELINE_BIND_POINT_GRAPHICS,passignedSet->p->pipelineLayout,p,passignedSet->p->pshaderModule[i]->setCount,passignedSet->pdescSets[i],0,0);
			p += passignedSet->p->pshaderModule[i]->setCount;
		}
		//copy push constants
		for(uint j = 0; j < passignedSet->p->pshaderModule[i]->variables.size(); ++j){
			auto m = std::find_if(pVarAddrs->begin(),pVarAddrs->end(),[&](auto &r)->bool{
				return (uint)r.first == passignedSet->p->pshaderModule[i]->variables[j].variableMapIndex;
			});
			if(m == pVarAddrs->end())
				continue;
			//printf("var: %s, +%u\n",std::get<0>(ShaderModule::variableMap[passignedSet->p->pshaderModule[i]->variables[j].variableMapIndex]),passignedSet->p->pshaderModule[i]->variables[j].offset);
			memcpy(pushConstantBuffer+passignedSet->p->pshaderModule[i]->variables[j].offset,
				m->second,std::get<1>(ShaderModule::variableMap[passignedSet->p->pshaderModule[i]->variables[j].variableMapIndex]));
		}
	}

	if(passignedSet->p->pushConstantRange.size > 0)
		vkCmdPushConstants(*pcommandBuffer,passignedSet->p->pipelineLayout,passignedSet->p->pushConstantRange.stageFlags,passignedSet->p->pushConstantRange.offset,passignedSet->p->pushConstantRange.size,pushConstantBuffer);
}

ColorFrame::ColorFrame(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : Drawable(_pcomp->LoadPipeline<ClientPipeline>(pshaderName,0),_pcomp), shaderUserFlags(0), shaderFlags(0), oldShaderFlags(0){
	//
	clock_gettime(CLOCK_MONOTONIC,&creationTime);
}

ColorFrame::~ColorFrame(){
	//
}

void ColorFrame::Draw(const VkRect2D &frame, const glm::vec2 &margin, const glm::vec2 &titlePad, const glm::vec2 &titleSpan, uint flags, const VkCommandBuffer *pcommandBuffer){
	time = timespec_diff(pcomp->frameTime,creationTime);

	glm::vec2 imageExtent = glm::vec2(pcomp->imageExtent.width,pcomp->imageExtent.height);

	glm::vec4 frameVec = {frame.offset.x,frame.offset.y,
		frame.offset.x+frame.extent.width,frame.offset.y+frame.extent.height};
	frameVec = 2.0f*(frameVec+0.5f)/glm::vec4(imageExtent,imageExtent)-1.0f;

	std::vector<std::pair<ShaderModule::VARIABLE, const void *>> varAddrs = {
		{ShaderModule::VARIABLE_XY0,&frameVec},
		{ShaderModule::VARIABLE_XY1,&frameVec.z},
		{ShaderModule::VARIABLE_SCREEN,&imageExtent},
		{ShaderModule::VARIABLE_MARGIN,&margin},
		{ShaderModule::VARIABLE_TITLEPAD,&titlePad},
		{ShaderModule::VARIABLE_TITLESPAN,&titleSpan},
		{ShaderModule::VARIABLE_FLAGS,&flags},
		{ShaderModule::VARIABLE_TIME,&time}
	};
	BindShaderResources(&varAddrs,pcommandBuffer);

	vkCmdDraw(*pcommandBuffer,1,1,0,0);

	passignedSet->fenceTag = pcomp->frameTag;
}

ClientFrame::ClientFrame(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : ColorFrame(pshaderName,_pcomp), ptitle(0), fullRegionUpdate(true), animationCompleted(true){
	//
}

ClientFrame::~ClientFrame(){
	if(ptitle)
		delete ptitle;
	pcomp->updateQueue.erase(std::remove(pcomp->updateQueue.begin(),pcomp->updateQueue.end(),this),pcomp->updateQueue.end());
	pcomp->titleUpdateQueue.erase(std::remove(pcomp->titleUpdateQueue.begin(),pcomp->titleUpdateQueue.end(),this),pcomp->titleUpdateQueue.end());
}

void ClientFrame::CreateSurface(uint w, uint h, uint depth){
	pcomp->updateQueue.push_back(this);

	surfaceDepth = depth;
	ptexture = pcomp->CreateTexture(w,h,surfaceDepth);

	UpdateDescSets();
}

void ClientFrame::AdjustSurface(uint w, uint h){
	pcomp->ReleaseTexture(ptexture);

	pcomp->updateQueue.push_back(this);
	fullRegionUpdate = true;

	ptexture = pcomp->CreateTexture(w,h,surfaceDepth);
	//In this case updating the descriptor sets would be enough, but we can't do that because of them being used currently by the pipeline.
	if(!AssignPipeline(passignedSet->p))
		throw Exception("Failed to assign a pipeline.");
	DebugPrintf(stdout,"Texture created: %ux%u\n",w,h);

	UpdateDescSets();
}

void ClientFrame::DestroySurface(){
	//
	pcomp->ReleaseTexture(ptexture);
}

void ClientFrame::SetShaders(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT]){
	Pipeline *pPipeline = pcomp->LoadPipeline<ClientPipeline>(pshaderName,0);
	if(!AssignPipeline(pPipeline))
		throw Exception("Failed to assign a pipeline.");
	
	UpdateDescSets();
}

void ClientFrame::SetTitle(const char *ptext){
	if(!ptitle){
		static const char *pshaderName[Pipeline::SHADER_MODULE_COUNT] = {
			"text_vertex.spv",0,"text_fragment.spv"
		};
		ptitle = new Text(pshaderName,pcomp->ptextEngine);
	}
	title = ptext;
	pcomp->titleUpdateQueue.push_back(this);
}

void ClientFrame::UpdateDescSets(){
	//
	VkDescriptorImageInfo descImageInfo = {};
	descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descImageInfo.imageView = ptexture->imageView;
	descImageInfo.sampler = pcomp->pointSampler;

	std::vector<VkWriteDescriptorSet> writeDescSets;
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		if(!passignedSet->p->pshaderModule[i])
			continue;
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

CompositorInterface::CompositorInterface(const Configuration *pconfig) : physicalDevIndex(pconfig->deviceIndex), currentFrame(0), imageIndex(0), frameTag(0), pcolorBackground(0), pbackground(0), ptextEngine(0), unredirected(false), appFullscreen(false), playingAnimation(false), debugLayers(pconfig->debugLayers), scissoring(pconfig->scissoring), hostMemoryImport(pconfig->hostMemoryImport), unredirOnFullscreen(pconfig->unredirOnFullscreen), enableAnimation(pconfig->enableAnimation), animationDuration(pconfig->animationDuration), pfontName(pconfig->pfontName), fontSize(pconfig->fontSize){
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

	//const char *players[] = {"VK_LAYER_LUNARG_standard_validation"};
	const char *players[] = {"VK_LAYER_KHRONOS_validation"};
	DebugPrintf(stdout,"Enumerating required layers\n");
	uint layersFound = 0;
	for(uint i = 0; i < layerCount; ++i)
		for(uint j = 0; j < sizeof(players)/sizeof(players[0]); ++j)
			if(strcmp(playerProps[i].layerName,players[j]) == 0){
				printf("  %s\n",players[j]);
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
		"VK_KHR_get_physical_device_properties2",
		"VK_KHR_external_memory_capabilities"
	};
	DebugPrintf(stdout,"Enumerating required extensions\n");
	uint extFound = 0;
	for(uint i = 0; i < extCount; ++i)
		for(uint j = 0; j < sizeof(pextensions)/sizeof(pextensions[0]); ++j)
			if(strcmp(pextProps[i].extensionName,pextensions[j]) == 0){
				printf("  %s\n",pextensions[j]);
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

	VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {};
	debugUtilsMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugUtilsMessengerCreateInfo.messageSeverity
		 = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
		|VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugUtilsMessengerCreateInfo.messageType
		= VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugUtilsMessengerCreateInfo.pfnUserCallback = ValidationLayerDebugCallback2;

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
	instanceCreateInfo.pNext = &debugUtilsMessengerCreateInfo;
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

	//VkPhysicalDeviceProperties *pdevProps = new VkPhysicalDeviceProperties[devCount];
	VkPhysicalDeviceExternalMemoryHostPropertiesEXT *pPhysicalDeviceExternalMemoryHostProps = new VkPhysicalDeviceExternalMemoryHostPropertiesEXT[devCount];
	VkPhysicalDeviceProperties2 *pdevProps = new VkPhysicalDeviceProperties2[devCount];
	for(uint i = 0; i < devCount; ++i){
		//device properties
		pPhysicalDeviceExternalMemoryHostProps[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT;
		pPhysicalDeviceExternalMemoryHostProps[i].pNext = 0;

		pdevProps[i].sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
		pdevProps[i].pNext = &pPhysicalDeviceExternalMemoryHostProps[i];
		vkGetPhysicalDeviceProperties2(pdevices[i],&pdevProps[i]);

		//device features
		VkPhysicalDeviceFeatures devFeatures;
		vkGetPhysicalDeviceFeatures(pdevices[i],&devFeatures);

		//extra checks
		VkPhysicalDeviceExternalBufferInfo physicalDeviceExternalBufferInfo = {};
		physicalDeviceExternalBufferInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_BUFFER_INFO;
		physicalDeviceExternalBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		physicalDeviceExternalBufferInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
		VkExternalBufferProperties externalBufferProps = {};
		externalBufferProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_BUFFER_PROPERTIES;
		vkGetPhysicalDeviceExternalBufferProperties(pdevices[i],&physicalDeviceExternalBufferInfo,&externalBufferProps);
		if((externalBufferProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_IMPORTABLE_BIT) && (externalBufferProps.externalMemoryProperties.externalMemoryFeatures & VK_EXTERNAL_MEMORY_FEATURE_DEDICATED_ONLY_BIT) == 0 && (externalBufferProps.externalMemoryProperties.compatibleHandleTypes & VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT))
			printf("Host pointer import supported!\n");

		printf("%c %u: %s\n\t.deviceID: %u\n\t.vendorID: %u\n\t.deviceType: %u\n",
			i == physicalDevIndex?'*':' ',
			i,pdevProps[i].properties.deviceName,pdevProps[i].properties.deviceID,pdevProps[i].properties.vendorID,pdevProps[i].properties.deviceType);
		printf("  max push constant size: %u\n  max bound desc sets: %u\n",
			pdevProps[i].properties.limits.maxPushConstantsSize,pdevProps[i].properties.limits.maxBoundDescriptorSets);
	}

	if(physicalDevIndex >= devCount){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Invalid gpu-index (%u) exceeds the number of available devices (%u).",physicalDevIndex,devCount);
		throw Exception();
	}

	physicalDev = pdevices[physicalDevIndex];
	physicalDevExternalMemoryHostProps = pPhysicalDeviceExternalMemoryHostProps[physicalDevIndex];
	physicalDevProps = pdevProps[physicalDevIndex].properties;

	delete []pdevices;
	delete []pdevProps;
	delete []pPhysicalDeviceExternalMemoryHostProps;

	//VkSurfaceCapabilitiesKHR surfaceCapabilities;
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
	/*for(uint i = 0; i < queueFamilyCount; ++i){
		if(pqueueFamilyProps[i].queueCount > 0 && pqueueFamilyProps[i].queueFlags & VK_QUEUE_TRANSFER_BIT){
			queueFamilyIndex[QUEUE_INDEX_TRANSFER] = i;
			break;
		}
	}*/
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
	const char *pdevExtensions[] = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME,
		//VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
		VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME,
		VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
		VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME
		//VK_KHR_BIND_MEMORY_2_EXTENSION_NAME,
		//VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME,
		//VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
		//VK_KHR_SAMPLER_YCBCR_CONVERSION_EXTENSION_NAME,
		//VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME
		//VK_GOOGLE_HLSL_FUNCTIONALITY1_EXTENSION_NAME,VK_GOOGLE_DECORATE_STRING_EXTENSION_NAME};
	};

	DebugPrintf(stdout,"Enumerating required device extensions\n");
	uint devExtFound = 0;
	for(uint i = 0; i < devExtCount; ++i)
		for(uint j = 0; j < sizeof(pdevExtensions)/sizeof(pdevExtensions[0]); ++j)
			if(strcmp(pdevExtProps[i].extensionName,pdevExtensions[j]) == 0){
				printf("  %s\n",pdevExtensions[j]);
				++devExtFound;
			}
	if(devExtFound < sizeof(pdevExtensions)/sizeof(pdevExtensions[0]))
		throw Exception("Could not find all required device extensions.");

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
	
	InitializeSwapchain();

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
	updateQueue.reserve(1024);
	titleUpdateQueue.reserve(1024);
	scissorRegions.reserve(32);
	presentRectLayers.reserve(32);

	ptextEngine = new TextEngine(pfontName,fontSize,this);
}

void CompositorInterface::InitializeSwapchain(){
	imageExtent = GetExtent();
	
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.surface = surface;
	swapchainCreateInfo.minImageCount = std::max(surfaceCapabilities.minImageCount,3u);
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
}

void CompositorInterface::DestroyRenderEngine(){
	DebugPrintf(stdout,"Compositor cleanup\n");

	delete ptextEngine;

	for(TextureCacheEntry &textureCacheEntry : textureCache)
		delete textureCacheEntry.ptexture;
	
	for(auto m : pipelines)
		delete m.second;
	shaders.clear();

	vkFreeCommandBuffers(logicalDev,commandPool,swapChainImageCount,pcommandBuffers);
	vkFreeCommandBuffers(logicalDev,commandPool,swapChainImageCount,pcopyCommandBuffers);
	delete []pcommandBuffers;
	delete []pcopyCommandBuffers;
	vkDestroyCommandPool(logicalDev,commandPool,0);

	for(VkDescriptorPool &descPool : descPoolArray)
		vkDestroyDescriptorPool(logicalDev,descPool,0);
	descPoolArray.clear();

	vkDestroySampler(logicalDev,pointSampler,0);

	DestroySwapchain();

	vkDestroyRenderPass(logicalDev,renderPass,0);

	vkDestroyDevice(logicalDev,0);

	((PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT"))(instance,debugReportCb,0);

	vkDestroySurfaceKHR(instance,surface,0);
	vkDestroyInstance(instance,0);
}

void CompositorInterface::DestroySwapchain(){
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

	VkRectLayerKHR &rectLayer = presentRectLayers.emplace_back();
	rectLayer.offset = {std::max(prect->offset.x,0),std::max(prect->offset.y,0)};
	rectLayer.extent = {
		prect->extent.width+std::min((int)imageExtent.width-(rectLayer.offset.x+(int)prect->extent.width),0),
		prect->extent.height+std::min((int)imageExtent.height-(rectLayer.offset.y+(int)prect->extent.height),0)
	};
	rectLayer.layer = 0;
}

void CompositorInterface::AddDamageRegion(const WManager::Client *pclient){
	if(!scissoring)
		return;
	glm::ivec2 margin = 4*glm::ivec2(
		pclient->pcontainer->margin.x*(float)imageExtent.width,
		pclient->pcontainer->margin.y*(float)imageExtent.width); //due to aspect, this must be *width
	
	glm::ivec2 titlePad = glm::ivec2(
		pclient->pcontainer->titlePad.x*(float)imageExtent.width,
		pclient->pcontainer->titlePad.y*(float)imageExtent.width);
	glm::ivec2 titlePadOffset = glm::min(titlePad,glm::ivec2(0));
	glm::ivec2 titlePadExtent = glm::max(titlePad,glm::ivec2(0));

	VkRect2D rect1;
	rect1.offset = {pclient->rect.x-margin.x+titlePadOffset.x,pclient->rect.y-margin.y+titlePadOffset.y};
	rect1.extent = {pclient->rect.w+2*margin.x-titlePadOffset.x+titlePadExtent.x,pclient->rect.h+2*margin.y-titlePadOffset.y+titlePadExtent.y};
	AddDamageRegion(&rect1);
	rect1.offset = {pclient->oldRect.x-margin.x+titlePadOffset.x,pclient->oldRect.y-margin.y+titlePadOffset.y};
	rect1.extent = {pclient->oldRect.w+2*margin.x-titlePadOffset.x+titlePadExtent.x,pclient->oldRect.h+2*margin.y-titlePadOffset.y+titlePadExtent.y};
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
		renderObject.pclientFrame->shaderFlags =
			(renderObject.pclient->pcontainer == pfocus?ClientFrame::SHADER_FLAG_FOCUS:0)
			|(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FLOATING?ClientFrame::SHADER_FLAG_FLOATING:0)
			//|(renderObject.pclient->pcontainer->pParent && renderObject.pclient->pParent->flags & WManager::Container::FLAG_STACKED?ClientFrame::SHADER_FLAG_STACKED:0)
			|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);

		CreateRenderQueueAppendix((*m).second,pfocus);

		m = appendixQueue.erase(m);
	}
}

void CompositorInterface::CreateRenderQueue(const WManager::Container *pcontainer, const WManager::Container *pfocus){
	if(!pcontainer->pch)
		return; //workaround a bug with stackQueue, need to fix this properly
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
				(pcont == pfocus || pcontainer == pfocus?ClientFrame::ClientFrame::SHADER_FLAG_FOCUS:0)
				//|(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FLOATING?ClientFrame::SHADER_FLAG_FLOATING:0) //probably not required here
				|(renderObject.pclient->pcontainer->pParent && renderObject.pclient->pcontainer->pParent->flags & WManager::Container::FLAG_STACKED?ClientFrame::SHADER_FLAG_STACKED:0)
				|renderObject.pclientFrame->shaderUserFlags;
			renderQueue.push_back(renderObject);
		}
		CreateRenderQueue(pcont,pfocus);
	}

	if(!pcontainer->pclient)
		return;
	CreateRenderQueueAppendix(pcontainer->pclient,pfocus);
}

bool CompositorInterface::PollFrameFence(){
	for(uint i = 0; i < 3; ++i){
		if(vkAcquireNextImageKHR(logicalDev,swapChain,std::numeric_limits<uint64_t>::max(),psemaphore[currentFrame][SEMAPHORE_INDEX_IMAGE_AVAILABLE],0,&imageIndex) != VK_SUCCESS){
			DebugPrintf(stderr,"Failed to acquire a swap chain image. Attempting to recreate (%u/3)...\n",i+1);
			WaitIdle();
			DestroySwapchain();
			InitializeSwapchain();
			continue;
		}
		break;
	}
	
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

	ptextEngine->ReleaseCycle();

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
		renderObject.pclientFrame->shaderFlags =
			(renderObject.pclient->pcontainer == pfocus?ClientFrame::SHADER_FLAG_FOCUS:0)
			|(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FLOATING?ClientFrame::SHADER_FLAG_FLOATING:0)
			|(renderObject.pclient->pcontainer->pParent && renderObject.pclient->pcontainer->pParent->flags & WManager::Container::FLAG_STACKED?ClientFrame::SHADER_FLAG_STACKED:0)
			|renderObject.pclientFrame->shaderUserFlags;
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
		renderObject.pclientFrame->shaderFlags =
			(renderObject.pclient->pcontainer == pfocus?ClientFrame::SHADER_FLAG_FOCUS:0)
			|(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FLOATING?ClientFrame::SHADER_FLAG_FLOATING:0)
			|(renderObject.pclient->pcontainer->pParent && renderObject.pclient->pcontainer->pParent->flags & WManager::Container::FLAG_STACKED?ClientFrame::SHADER_FLAG_STACKED:0)
			|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);

		CreateRenderQueueAppendix((*m).second,pfocus);

		m = appendixQueue.erase(m);
	}

	for(auto &p : appendixQueue){ //push the remaining (untransient) windows to the end of the queue
		RenderObject renderObject;
		renderObject.pclient = p.second;
		renderObject.pclientFrame = dynamic_cast<ClientFrame *>(p.second);
		renderObject.pclientFrame->oldShaderFlags = renderObject.pclientFrame->shaderFlags;
		renderObject.pclientFrame->shaderFlags =
			(renderObject.pclient->pcontainer == pfocus?ClientFrame::SHADER_FLAG_FOCUS:0)
			|(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FLOATING?ClientFrame::SHADER_FLAG_FLOATING:0)
			|(renderObject.pclient->pcontainer->pParent && renderObject.pclient->pcontainer->pParent->flags & WManager::Container::FLAG_STACKED?ClientFrame::SHADER_FLAG_STACKED:0)
			|renderObject.pclientFrame->shaderUserFlags;
		renderQueue.push_back(renderObject);
	}

	appFullscreen = false;
	playingAnimation = false;
	clock_gettime(CLOCK_MONOTONIC,&frameTime);

	for(uint i = 0; i < renderQueue.size(); ++i){
		RenderObject &renderObject = renderQueue[i];

		float s;
		if(enableAnimation){
			float t = timespec_diff(frameTime,renderObject.pclient->translationTime);
			s = std::clamp(t/animationDuration,0.0f,1.0f);
			s = 1.0f/(1.0f+expf(-10.0f*s+5.0f));
		}else s = 1.0f;

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

		}else{
			renderObject.pclient->position = glm::vec2(renderObject.pclient->rect.x,renderObject.pclient->rect.y);
			
			if(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FULLSCREEN)
				appFullscreen = true;
		}

		if(renderObject.pclientFrame->shaderFlags != renderObject.pclientFrame->oldShaderFlags)
			AddDamageRegion(renderObject.pclient);
	}

	if(unredirOnFullscreen){
		if(appFullscreen && !unredirected){
			printf("************** unredirect\n");
			unredirected = true;
		}else
		if(!appFullscreen && unredirected){
			printf("************** redirect\n");
			unredirected = false;
		}
	}
	
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.flags = 0;
	if(vkBeginCommandBuffer(pcopyCommandBuffers[currentFrame],&commandBufferBeginInfo) != VK_SUCCESS)
		throw Exception("Failed to begin command buffer recording.");

	ClientFrame *pbackground1 = dynamic_cast<ClientFrame *>(pbackground);
	if(pbackground1)
		pbackground1->UpdateContents(&pcopyCommandBuffers[currentFrame]);

	//TODO: update only visible clients
	for(ClientFrame *pclientFrame : updateQueue)
		pclientFrame->UpdateContents(&pcopyCommandBuffers[currentFrame]);
	updateQueue.clear();

	for(ClientFrame *pclientFrame : titleUpdateQueue)
		pclientFrame->ptitle->Set(pclientFrame->title.c_str(),&pcopyCommandBuffers[currentFrame]);
	titleUpdateQueue.clear();

	if(vkEndCommandBuffer(pcopyCommandBuffers[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to end command buffer recording.");

	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
	if(vkBeginCommandBuffer(pcommandBuffers[currentFrame],&commandBufferBeginInfo) != VK_SUCCESS)
		throw Exception("Failed to begin command buffer recording.");

	//TODO: debug option: draw frames around scissor area
	VkRect2D scissor;
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

	//clear manually since some parts of the buffer we want to retain based on damage
	if(pbackground){
		vkCmdBindPipeline(pcommandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,pbackground->passignedSet->p->pipeline);

		VkRect2D screenRect;
		screenRect.offset = {0,0};
		screenRect.extent = imageExtent;
		pbackground->Draw(screenRect,glm::vec2(0.0f),glm::vec2(0.0f),glm::vec2(0.0f),0,&pcommandBuffers[currentFrame]);
	}
	
	//TODO: stencil buffer optimization

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

		renderObject.pclientFrame->Draw(frame,renderObject.pclient->pcontainer->margin,renderObject.pclient->pcontainer->titlePad,renderObject.pclient->pcontainer->titleSpan,renderObject.pclientFrame->shaderFlags,&pcommandBuffers[currentFrame]);

		if(renderObject.pclient->pcontainer->titleBar != WManager::Container::TITLEBAR_NONE && !(renderObject.pclient->pcontainer->flags & WManager::Container::FLAG_FULLSCREEN) && renderObject.pclientFrame->ptitle){

			glm::uvec2 titlePosition = glm::uvec2(frame.offset.x,frame.offset.y);
			if(renderObject.pclient->titlePad1.x > 1e-5)
				titlePosition.x += frame.extent.width;
			if(renderObject.pclient->titlePad1.y > 1e-5)
				titlePosition.y += frame.extent.height;
			glm::vec2 titlePadAbs = glm::abs(renderObject.pclient->titlePad1);
			if(titlePadAbs.x > titlePadAbs.y){
				titlePosition.y += renderObject.pclient->titleStackOffset.y+renderObject.pclientFrame->ptitle->GetTextLength()-(renderObject.pclientFrame->ptitle->GetTextLength()-renderObject.pclient->titleFrameExtent.y);
				titlePosition.x += (ptextEngine->fontFace->glyph->metrics.horiBearingY>>6)/2;
			}else{
				titlePosition.x += renderObject.pclient->titleStackOffset.x;
				titlePosition.y += (ptextEngine->fontFace->glyph->metrics.horiBearingY>>6)/2;
			}
			titlePosition += 0.5f*renderObject.pclient->titlePad1;

			//intersect the title region with scissor rectangle
			glm::ivec2 titleRectOffset = glm::ivec2(renderObject.pclient->titleRect.x,renderObject.pclient->titleRect.y);
			glm::ivec2 scissorOffset = glm::ivec2(scissor.offset.x,scissor.offset.y);
			glm::ivec2 offset = glm::max(titleRectOffset,scissorOffset);
			glm::ivec2 extent = glm::min(titleRectOffset+glm::ivec2(renderObject.pclient->titleRect.w,renderObject.pclient->titleRect.h),scissorOffset+glm::ivec2(scissor.extent.width,scissor.extent.height))-offset;

			if(glm::all(glm::greaterThan(extent,glm::ivec2(0)))){
				VkRect2D textFrame;
				textFrame.offset = {offset.x,offset.y};
				textFrame.extent = {extent.x,extent.y};

				vkCmdSetScissor(pcommandBuffers[currentFrame],0,1,&textFrame);

				vkCmdBindPipeline(pcommandBuffers[currentFrame],VK_PIPELINE_BIND_POINT_GRAPHICS,renderObject.pclientFrame->ptitle->passignedSet->p->pipeline);
				renderObject.pclientFrame->ptitle->Draw(titlePosition,renderObject.pclient->pcontainer->titleTransform,&pcommandBuffers[currentFrame]);

				vkCmdSetScissor(pcommandBuffers[currentFrame],0,1,&scissor);
			}
		}
	}

	vkCmdEndRenderPass(pcommandBuffers[currentFrame]);

	if(vkEndCommandBuffer(pcommandBuffers[currentFrame]) != VK_SUCCESS)
		throw Exception("Failed to end command buffer recording.");
}

void CompositorInterface::Present(){
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
	
	VkPresentRegionKHR presentRegion = {};
	presentRegion.rectangleCount = presentRectLayers.size();
	presentRegion.pRectangles = presentRectLayers.data();

	VkPresentRegionsKHR presentRegions = {};
	presentRegions.sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR;
	presentRegions.swapchainCount = 1;
	presentRegions.pRegions = &presentRegion;
	
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &psemaphore[currentFrame][SEMAPHORE_INDEX_RENDER_FINISHED];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = 0;
	presentInfo.pNext = presentRegion.rectangleCount > 0?&presentRegions:0;
	vkQueuePresentKHR(queue[QUEUE_INDEX_PRESENT],&presentInfo);

	presentRectLayers.clear();

	currentFrame = (currentFrame+1)%swapChainImageCount;

	frameTag++;
}

//-vertex buffer layout is always fixed, since there is no predefined information on what shaders (and thus inputs) will be used on what vertex data
//TODO: take out 'new Pipeline' from below, move it outside - no need to template this
template<class T>
Pipeline * CompositorInterface::LoadPipeline(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], const std::vector<std::pair<ShaderModule::INPUT,uint>> *pvertexBufferLayout){
	size_t hash = typeid(T).hash_code();
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i)
		if(pshaderName[i])
			boost::hash_combine(hash,std::string(pshaderName[i]));
	if(pvertexBufferLayout)
		boost::hash_range(hash,pvertexBufferLayout->begin(),pvertexBufferLayout->end());
	
	auto m = std::find_if(pipelines.begin(),pipelines.end(),[&](auto &r)->bool{
		return r.first == hash;
	});
	if(m != pipelines.end())
		return (*m).second;

	ShaderModule *pshader[Pipeline::SHADER_MODULE_COUNT] = {};
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		if(!pshaderName[i])
			continue;
		auto n = std::find_if(shaders.begin(),shaders.end(),[&](auto &r)->bool{
			return strcmp(r.pname,pshaderName[i]) == 0;
		});
		if(n == shaders.end()){
			snprintf(Exception::buffer,sizeof(Exception::buffer),"Shader not found: %s.",pshaderName[i]);
			throw Exception();
		}
		pshader[i] = &(*n);
	}
	Pipeline *pPipeline = new T(
		pshader[Pipeline::SHADER_MODULE_VERTEX],
		pshader[Pipeline::SHADER_MODULE_GEOMETRY],
		pshader[Pipeline::SHADER_MODULE_FRAGMENT],pvertexBufferLayout,this);
	pipelines.push_back(std::pair<size_t, Pipeline *>(hash,pPipeline));

	return pPipeline;
}

template Pipeline * CompositorInterface::LoadPipeline<ClientPipeline>(const char *[Pipeline::SHADER_MODULE_COUNT], const std::vector<std::pair<ShaderModule::INPUT,uint>> *);
template Pipeline * CompositorInterface::LoadPipeline<TextPipeline>(const char *[Pipeline::SHADER_MODULE_COUNT], const std::vector<std::pair<ShaderModule::INPUT,uint>> *);

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
	screenRect.extent = imageExtent;
	AddDamageRegion(&screenRect);
}

Texture * CompositorInterface::CreateTexture(uint w, uint h, uint surfaceDepth){
	Texture *ptexture;

	//should be larger than 0:
	w = std::max(std::min(w,physicalDevProps.limits.maxImageDimension2D),1u);
	h = std::max(std::min(h,physicalDevProps.limits.maxImageDimension2D),1u);

	const VkComponentMapping *pcomponentMapping = surfaceDepth > 24?&TexturePixmap::pixmapComponentMapping:&TexturePixmap::pixmapComponentMapping24;
	uint componentMappingHash = TextureBase::GetComponentMappingHash(pcomponentMapping);

	auto m = std::find_if(textureCache.begin(),textureCache.end(),[&](auto &r)->bool{
		//return r.ptexture->w == w && r.ptexture->h == h;
		return r.ptexture->w == w && r.ptexture->h == h && r.ptexture->componentMappingHash == componentMappingHash;
	});
	if(m != textureCache.end()){
		ptexture = (*m).ptexture;

		std::iter_swap(m,textureCache.end()-1);
		textureCache.pop_back();
		printf("----------- found cached texture\n");

	}else ptexture = new Texture(w,h,pcomponentMapping,0,this);

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
	DebugPrintf(stdout,"validation layer: %s\n",pmsg);
	return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL CompositorInterface::ValidationLayerDebugCallback2(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pdata, void *puserData){
	DebugPrintf(stdout,"validation layer2: %s\n",pdata->pMessage);
	return VK_FALSE;
}

X11ClientFrame::X11ClientFrame(WManager::Container *pcontainer, const Backend::X11Client::CreateInfo *_pcreateInfo, const char *_pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : X11Client(pcontainer,_pcreateInfo), ClientFrame(_pshaderName,_pcomp){
	xcb_composite_redirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);

	windowPixmap = xcb_generate_id(pbackend->pcon);

	StartComposition1();
}

X11ClientFrame::~X11ClientFrame(){
	StopComposition1();
	xcb_composite_unredirect_window(pbackend->pcon,window,XCB_COMPOSITE_REDIRECT_MANUAL);
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

		fullRegionUpdate = false;
	}

	if(!pcomp->hostMemoryImport){
		unsigned char *pdata = (unsigned char *)ptexture->Map();

		for(VkRect2D &rect1 : damageRegions){
			xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(pbackend->pcon,windowPixmap,rect1.offset.x,rect1.offset.y,rect1.extent.width,rect1.extent.height,~0u,XCB_IMAGE_FORMAT_Z_PIXMAP,segment,0);

			xcb_shm_get_image_reply_t *pimageReply = xcb_shm_get_image_reply(pbackend->pcon,imageCookie,0);
			xcb_flush(pbackend->pcon);
			free(pimageReply);

			for(uint y = 0; y < rect1.extent.height; ++y){
				uint offsetDst = 4*(rect.w*(y+rect1.offset.y)+rect1.offset.x);
				uint offsetSrc = 4*(rect1.extent.width*y);
				memcpy(pdata+offsetDst,pchpixels+offsetSrc,4*rect1.extent.width);
			}

			VkRect2D screenRect;
			screenRect.offset = {(sint)position.x+rect1.offset.x,(sint)position.y+rect1.offset.y};
			screenRect.extent = rect1.extent;
			pcomp->AddDamageRegion(&screenRect);
		}
		ptexture->Unmap(pcommandBuffer,damageRegions.data(),damageRegions.size());

	}else{
		xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(pbackend->pcon,windowPixmap,0,0,rect.w,rect.h,~0u,XCB_IMAGE_FORMAT_Z_PIXMAP,segment,0); //need to get whole image

		xcb_shm_get_image_reply_t *pimageReply = xcb_shm_get_image_reply(pbackend->pcon,imageCookie,0);
		xcb_flush(pbackend->pcon);
		free(pimageReply);

		for(VkRect2D &rect1 : damageRegions){
			VkRect2D screenRect;
			screenRect.offset = {(sint)position.x+rect1.offset.x,(sint)position.y+rect1.offset.y};
			screenRect.extent = rect1.extent;
			pcomp->AddDamageRegion(&screenRect);
		}
		
		ptexture->Update(pcommandBuffer,damageRegions.data(),damageRegions.size());
	}

	damageRegions.clear();
}

void X11ClientFrame::AdjustSurface1(){
	if(oldRect.w != rect.w || oldRect.h != rect.h){
		//detach
		if(pcomp->hostMemoryImport)
			ptexture->Detach(pcomp->frameTag);
		xcb_shm_detach(pbackend->pcon,segment);
		shmdt(pchpixels);

		shmctl(shmid,IPC_RMID,0);

		//attach
		uint textureSize = rect.w*rect.h*4;
		shmid = shmget(IPC_PRIVATE,(textureSize-1)+pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment-(textureSize-1)%pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment,IPC_CREAT|0777);
		if(shmid == -1){
			DebugPrintf(stderr,"Failed to allocate shared memory.\n");
			return;
		}
		xcb_shm_attach(pbackend->pcon,segment,shmid,0);
		pchpixels = (unsigned char*)shmat(shmid,0,0);

		xcb_free_pixmap(pbackend->pcon,windowPixmap);
		xcb_composite_name_window_pixmap(pbackend->pcon,window,windowPixmap);

		AdjustSurface(rect.w,rect.h);

		if(pcomp->hostMemoryImport && !ptexture->Attach(pchpixels)){
			DebugPrintf(stderr,"Failed to import host memory. Disabling feature.\n");
			pcomp->hostMemoryImport = false;
		}

		pcomp->AddDamageRegion(this);
	}else
	if(oldRect.x != rect.x || oldRect.y != rect.y)
		pcomp->AddDamageRegion(this);
}

void X11ClientFrame::StartComposition1(){
	xcb_composite_name_window_pixmap(pbackend->pcon,window,windowPixmap);

	damage = xcb_generate_id(pbackend->pcon);
	xcb_damage_create(pbackend->pcon,damage,window,XCB_DAMAGE_REPORT_LEVEL_NON_EMPTY);

	//attach to shared memory
	uint textureSize = rect.w*rect.h*4;
	shmid = shmget(IPC_PRIVATE,(textureSize-1)+pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment-(textureSize-1)%pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment,IPC_CREAT|0777);
	if(shmid == -1){
		DebugPrintf(stderr,"Failed to allocate shared memory.\n");
		return;
	}
	segment = xcb_generate_id(pbackend->pcon);
	xcb_shm_attach(pbackend->pcon,segment,shmid,0);
	pchpixels = (unsigned char*)shmat(shmid,0,0);

	xcb_flush(pbackend->pcon);
	
	//get image depth
	xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(pbackend->pcon,windowPixmap,0,0,1,1,~0u,XCB_IMAGE_FORMAT_Z_PIXMAP,segment,0);
	xcb_shm_get_image_reply_t *pimageReply = xcb_shm_get_image_reply(pbackend->pcon,imageCookie,0);

	uint depth;
	if(pimageReply){
		depth = pimageReply->depth;
		free(pimageReply);
	}else{
		DebugPrintf(stderr,"Failed to get SHM image. Assuming 24 bit depth.\n");
		depth = 24;
	}

	CreateSurface(rect.w,rect.h,depth);

	if(pcomp->hostMemoryImport && !ptexture->Attach(pchpixels)){
		DebugPrintf(stderr,"Failed to import host memory. Disabling feature.\n");
		pcomp->hostMemoryImport = false;
	}

	pcomp->AddDamageRegion(this);
}

void X11ClientFrame::StopComposition1(){
	if(pcomp->hostMemoryImport)
		ptexture->Detach(pcomp->frameTag);

	xcb_shm_detach(pbackend->pcon,segment);
	shmdt(pchpixels);

	shmctl(shmid,IPC_RMID,0);

	xcb_damage_destroy(pbackend->pcon,damage);

	xcb_free_pixmap(pbackend->pcon,windowPixmap);

	pcomp->AddDamageRegion(this);

	DestroySurface();
}

void X11ClientFrame::SetTitle1(const char *ptitle){
	SetTitle(ptitle);

	VkRect2D rect;
	rect.offset = {titleRect.x,titleRect.y};
	rect.extent = {titleRect.w,titleRect.h};
	pcomp->AddDamageRegion(&rect);
}

X11Background::X11Background(xcb_pixmap_t _pixmap, uint _w, uint _h, const char *_pshaderName[Pipeline::SHADER_MODULE_COUNT], X11Compositor *_pcomp) : w(_w), h(_h), ClientFrame(_pshaderName,_pcomp), pcomp11(_pcomp), pixmap(_pixmap){
	//
	uint textureSize = w*h*4;
	shmid = shmget(IPC_PRIVATE,(textureSize-1)+pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment-(textureSize-1)%pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment,IPC_CREAT|0777);
	if(shmid == -1){
		DebugPrintf(stderr,"Failed to allocate shared memory.\n");
		return;
	}
	segment = xcb_generate_id(pcomp11->pbackend->pcon);
	xcb_shm_attach(pcomp11->pbackend->pcon,segment,shmid,0);

	pchpixels = (unsigned char*)shmat(shmid,0,0);

	CreateSurface(w,h,24);

	if(pcomp->hostMemoryImport && !ptexture->Attach(pchpixels)){
		DebugPrintf(stderr,"Failed to import host memory. Disabling feature.\n");
		pcomp->hostMemoryImport = false;
	}

	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = {w,h};
	pcomp->AddDamageRegion(&screenRect);
}

X11Background::~X11Background(){
	if(pcomp->hostMemoryImport)
		ptexture->Detach(pcomp->frameTag);

	xcb_shm_detach(pcomp11->pbackend->pcon,segment);
	shmdt(pchpixels);

	shmctl(shmid,IPC_RMID,0);

	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = {w,h};
	pcomp->AddDamageRegion(&screenRect);
}

void X11Background::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	if(!fullRegionUpdate)
		return;

	xcb_shm_get_image_cookie_t imageCookie = xcb_shm_get_image(pcomp11->pbackend->pcon,pixmap,0,0,w,h,~0u,XCB_IMAGE_FORMAT_Z_PIXMAP,segment,0);

	xcb_shm_get_image_reply_t *pimageReply = xcb_shm_get_image_reply(pcomp11->pbackend->pcon,imageCookie,0);
	xcb_flush(pcomp11->pbackend->pcon);

	if(!pimageReply){
		DebugPrintf(stderr,"No shared memory.\n");
		return;
	}

	free(pimageReply);

	VkRect2D screenRect;
	screenRect.offset = {0,0};
	screenRect.extent = {w,h};

	if(!pcomp->hostMemoryImport){
		unsigned char *pdata = (unsigned char*)ptexture->Map();
		memcpy(pdata,pchpixels,w*h*4);
		
		ptexture->Unmap(pcommandBuffer,&screenRect,1);
	
	}else{
		ptexture->Update(pcommandBuffer,&screenRect,1);
	}

	fullRegionUpdate = false;

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
		throw Exception("XCompositor unavailable.");
	xcb_composite_query_version_cookie_t compCookie = xcb_composite_query_version(pbackend->pcon,XCB_COMPOSITE_MAJOR_VERSION,XCB_COMPOSITE_MINOR_VERSION);
	xcb_composite_query_version_reply_t *pcompReply = xcb_composite_query_version_reply(pbackend->pcon,compCookie,0);
	if(!pcompReply)
		throw Exception("XCompositor unavailable.");
	DebugPrintf(stdout,"XComposite %u.%u\n",pcompReply->major_version,pcompReply->minor_version);
	free(pcompReply);

	//overlay
	xcb_composite_get_overlay_window_cookie_t overlayCookie = xcb_composite_get_overlay_window(pbackend->pcon,pbackend->pscr->root);
	xcb_composite_get_overlay_window_reply_t *poverlayReply = xcb_composite_get_overlay_window_reply(pbackend->pcon,overlayCookie,0);
	if(!poverlayReply)
		throw Exception("Unable to get overlay window.");
	overlay = poverlayReply->overlay_win;
	free(poverlayReply);
	DebugPrintf(stdout,"overlay xid: %u\n",overlay);

	uint mask = XCB_CW_EVENT_MASK;
	uint values[1] = {XCB_EVENT_MASK_EXPOSURE};
	xcb_change_window_attributes(pbackend->pcon,overlay,mask,values);

	//xfixes
	if(!pbackend->QueryExtension("XFIXES",&xfixesEventOffset,&xfixesErrorOffset))
		throw Exception("XFixes unavailable.");
	xcb_xfixes_query_version_cookie_t fixesCookie = xcb_xfixes_query_version(pbackend->pcon,XCB_XFIXES_MAJOR_VERSION,XCB_XFIXES_MINOR_VERSION);
	xcb_xfixes_query_version_reply_t *pfixesReply = xcb_xfixes_query_version_reply(pbackend->pcon,fixesCookie,0);
	if(!pfixesReply)
		throw Exception("XFixes unavailable.");
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
	if(!pdamageReply)
		throw Exception("Damage extension unavailable.");
	DebugPrintf(stdout,"Damage %u.%u\n",pdamageReply->major_version,pdamageReply->minor_version);
	free(pdamageReply);

	xcb_shm_query_version_cookie_t shmCookie = xcb_shm_query_version(pbackend->pcon);
	xcb_shm_query_version_reply_t *pshmReply = xcb_shm_query_version_reply(pbackend->pcon,shmCookie,0);
	if(!pshmReply || !pshmReply->shared_pixmaps)
		throw Exception("SHM extension unavailable.");
	DebugPrintf(stdout,"SHM %u.%u\n",pshmReply->major_version,pshmReply->minor_version);
	free(pshmReply);

	xcb_dri3_query_version_cookie_t dri3Cookie = xcb_dri3_query_version(pbackend->pcon,XCB_DRI3_MAJOR_VERSION,XCB_DRI3_MINOR_VERSION);
	xcb_dri3_query_version_reply_t *pdri3Reply = xcb_dri3_query_version_reply(pbackend->pcon,dri3Cookie,0);
	if(!pdri3Reply)
		throw Exception("DRI3 extension unavailable.");
	DebugPrintf(stdout,"DRI3 %u.%u\n",pdri3Reply->major_version,pdri3Reply->minor_version);
	free(pdri3Reply);

	xcb_flush(pbackend->pcon);

	InitializeRenderEngine();

	/*char cardStr[256];
	snprintf(cardStr,sizeof(cardStr),"/dev/dri/card%u",physicalDevIndex);

	cardfd = open(cardStr,O_RDWR|FD_CLOEXEC);
	if(cardfd < 0){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Failed to open %s",cardStr);
		throw Exception();
	}
	pgbmdev = gbm_create_device(cardfd);
	if(!pgbmdev)
		throw Exception("Failed to create GBM device.");*/
}

void X11Compositor::Stop(){
	if(dynamic_cast<ClientFrame *>(pbackground))
		delete pbackground;
	if(pcolorBackground)
		delete pcolorBackground;
	DestroyRenderEngine();

	/*gbm_device_destroy(pgbmdev);
	close(cardfd);*/

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

		X11ClientFrame *pclientFrame = dynamic_cast<X11ClientFrame *>(pclient);

		xcb_xfixes_region_t region = xcb_generate_id(pbackend->pcon);
		xcb_xfixes_create_region(pbackend->pcon,region,0,0);
		xcb_damage_subtract(pbackend->pcon,pclientFrame->damage,0,region);

		xcb_xfixes_fetch_region_cookie_t fetchRegionCookie = xcb_xfixes_fetch_region_unchecked(pbackend->pcon,region);
		xcb_xfixes_destroy_region(pbackend->pcon,region);

		xcb_xfixes_fetch_region_reply_t *pfetchRegionReply = xcb_xfixes_fetch_region_reply(pbackend->pcon,fetchRegionCookie,0);

		uint count = xcb_xfixes_fetch_region_rectangles_length(pfetchRegionReply);
		if(count > 0){
			xcb_rectangle_t *prects = xcb_xfixes_fetch_region_rectangles(pfetchRegionReply);
			for(uint i = 0; i < count; ++i){
				if(pclient->rect.w >= prects[i].x+prects[i].width && pclient->rect.h >= prects[i].y+prects[i].height){
					VkRect2D rect;
					rect.offset = {prects[i].x,prects[i].y};
					rect.extent = {prects[i].width,prects[i].height};
					pclientFrame->damageRegions.push_back(rect);
				}
			}
		}else{
			if(pclient->rect.w >= pev->area.x+pev->area.width && pclient->rect.h >= pev->area.y+pev->area.height){
				VkRect2D rect;
				rect.offset = {pfetchRegionReply->extents.x,pfetchRegionReply->extents.y};
				rect.extent = {pfetchRegionReply->extents.width,pfetchRegionReply->extents.height};
				pclientFrame->damageRegions.push_back(rect);
			}
		}

		free(pfetchRegionReply);

		/*if(pclient->rect.w < pev->area.x+pev->area.width || pclient->rect.h < pev->area.y+pev->area.height)
			return true; //filter out outdated events after client shrink in size*/

		if(std::find(updateQueue.begin(),updateQueue.end(),pclientFrame) == updateQueue.end())
			updateQueue.push_back(pclientFrame);

		/*VkRect2D rect;
		rect.offset = {pev->area.x,pev->area.y};
		rect.extent = {pev->area.width,pev->area.height};
		pclientFrame->damageRegions.push_back(rect);
		//DebugPrintf(stdout,"DAMAGE_EVENT, %x, (%hd,%hd), (%hux%hu)\n",pev->drawable,pev->area.x,pev->area.y,pev->area.width,pev->area.height);*/
		
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

glm::vec2 X11Compositor::GetDPI() const{
	return glm::vec2(25.4f*(float)pbackend->pscr->width_in_pixels/(float)pbackend->pscr->width_in_millimeters,25.4f*(float)pbackend->pscr->height_in_pixels/(float)pbackend->pscr->height_in_millimeters);
}

X11DebugClientFrame::X11DebugClientFrame(WManager::Container *pcontainer, const Backend::DebugClient::CreateInfo *_pcreateInfo, const char *_pshaderName[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *_pcomp) : DebugClient(pcontainer,_pcreateInfo), ClientFrame(_pshaderName,_pcomp){
	//
	StartComposition1();
}

X11DebugClientFrame::~X11DebugClientFrame(){
	StopComposition1();
}

void X11DebugClientFrame::UpdateContents(const VkCommandBuffer *pcommandBuffer){
	//
	uint color[3];
	for(uint &t : color)
		t = rand()%190;
	unsigned char *pdata = (unsigned char*)ptexture->Map();
	for(uint i = 0, n = rect.w*rect.h; i < n; ++i){
		//unsigned char t = (float)(i/rect.w)/(float)rect.h*255;
		pdata[4*i+0] = color[0];
		pdata[4*i+1] = color[1];
		pdata[4*i+2] = color[2];
		pdata[4*i+3] = 190;//255;
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

void X11DebugClientFrame::StartComposition1(){
	CreateSurface(rect.w,rect.h,32);
	pcomp->AddDamageRegion(this);
}

void X11DebugClientFrame::StopComposition1(){
	DestroySurface();
	pcomp->AddDamageRegion(this);
}

void X11DebugClientFrame::SetTitle1(const char *ptitle){
	SetTitle(ptitle);

	VkRect2D rect;
	rect.offset = {titleRect.x,titleRect.y};
	rect.extent = {titleRect.w,titleRect.h};
	pcomp->AddDamageRegion(&rect);
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

glm::vec2 NullCompositor::GetDPI() const{
	return glm::vec2(0.0f);
}

}

