#include "main.h"
#include "container.h"
#include "backend.h"
#include "compositor.h"

#define GLMF_SWIZZLE
#include <glm/glm.hpp>
//#include <glm/gtx/vec_swizzle.hpp>
#include <set>
#include <cstdlib>
#include <limits>

namespace Compositor{

CompositorPipeline::CompositorPipeline(CompositorInterface *_pcomp) : pcomp(_pcomp){
	//
}

CompositorPipeline::~CompositorPipeline(){
	vkDestroyShaderModule(pcomp->logicalDev,vertexShader,0);
	vkDestroyShaderModule(pcomp->logicalDev,geometryShader,0);
	vkDestroyShaderModule(pcomp->logicalDev,fragmentShader,0);
	vkDestroyPipeline(pcomp->logicalDev,pipeline,0);
	vkDestroyPipelineLayout(pcomp->logicalDev,pipelineLayout,0);
}

CompositorPipeline * CompositorPipeline::CreateDefault(CompositorInterface *pcomp){
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	//
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = 0;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	//inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[3];

	VkShaderModule vertexShader = pcomp->CreateShaderModuleFromFile("frame_vertex.spv");
	shaderStageCreateInfo[0] = (VkPipelineShaderStageCreateInfo){};
	shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfo[0].module = vertexShader;
	shaderStageCreateInfo[0].pName = "main";

	VkShaderModule geometryShader = pcomp->CreateShaderModuleFromFile("frame_geometry.spv");
	shaderStageCreateInfo[1] = (VkPipelineShaderStageCreateInfo){};
	shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	shaderStageCreateInfo[1].module = geometryShader;
	shaderStageCreateInfo[1].pName = "main";

	VkShaderModule fragmentShader = pcomp->CreateShaderModuleFromFile("frame_fragment.spv");
	shaderStageCreateInfo[2] = (VkPipelineShaderStageCreateInfo){};
	shaderStageCreateInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfo[2].module = fragmentShader;
	shaderStageCreateInfo[2].pName = "main";

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)pcomp->imageExtent.width;
	viewport.height = (float)pcomp->imageExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0,0};
	scissor.extent = pcomp->imageExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	//rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	//rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; 
	//depth stencil

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	//...

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = 32;

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 0;
	layoutCreateInfo.pSetLayouts = 0;
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(pcomp->logicalDev,&layoutCreateInfo,0,&pipelineLayout) != VK_SUCCESS)
		return 0;

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = sizeof(shaderStageCreateInfo)/sizeof(shaderStageCreateInfo[0]);
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfo;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo; //!!
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = 0;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = 0;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = pcomp->renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = 0;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	if(vkCreateGraphicsPipelines(pcomp->logicalDev,0,1,&graphicsPipelineCreateInfo,0,&pipeline) != VK_SUCCESS)
		return 0;

	CompositorPipeline *pcompPipeline = new CompositorPipeline(pcomp);
	pcompPipeline->vertexShader = vertexShader;
	pcompPipeline->geometryShader = geometryShader;
	pcompPipeline->fragmentShader = fragmentShader;
	pcompPipeline->pipelineLayout = pipelineLayout;
	pcompPipeline->pipeline = pipeline;

	return pcompPipeline;
}

/*FrameObject::FrameObject(CompositorInterface *pcomp){
	pcomp->frameObjects.push_back(this);
	this->pcomp = pcomp;
}

FrameObject::~FrameObject(){
	std::vector<FrameObject *>::iterator m = std::find(
		pcomp->frameObjects.begin(),pcomp->frameObjects.end(),this);
	std::iter_swap(m,pcomp->frameObjects.end()-1);
	pcomp->frameObjects.pop_back();
}*/

RenderObject::RenderObject(const CompositorPipeline *_pPipeline, const CompositorInterface *_pcomp) : pPipeline(_pPipeline), pcomp(_pcomp){
	//
}

RenderObject::~RenderObject(){
	//
}

FrameObject::FrameObject(const CompositorPipeline *_pPipeline, const CompositorInterface *_pcomp, VkRect2D _frame) : RenderObject(_pPipeline,_pcomp), frame(_frame){
	//
}

FrameObject::~FrameObject(){
	//
}

void FrameObject::Draw(const VkCommandBuffer *pcommandBuffer){
	/*glm::mat4 tr = glm::mat4(
		2.0f,0.0f,0.0f,0.0f,
		0.0f,1.0f,0.0f,0.0f,
		0.0f,0.0f,2.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f);*/
	/*glm::mat4 tr = glm::mat4(
		1.0f,0.0f,0.0f,0.0f,
		0.0f,0.5f,0.0f,0.0f,
		0.0f,0.0f,1.0f,0.0f,
		0.0f,0.0f,0.0f,1.0f);*/
		//0.5f,0.0f,0.0f,1.0f);*/
	/*glm::mat4 tr = glm::mat4(
		0.7f,0.0f,0.0f,0.0f,
		0.0f,0.7f*0.5f,0.0f,0.0f,
		0.0f,0.0f,0.7f,0.0f,
		0.0f,0.0f,0.0f,1.0f);*/
	//TODO: need accurate coordinates, also add +0.5 for pixel centers
	glm::vec4 frameVec = {frame.offset.x,frame.offset.y,frame.offset.x+frame.extent.width,frame.offset.y+frame.extent.height};
	frameVec += 0.5f;
	frameVec /= (glm::vec4){pcomp->imageExtent.width,pcomp->imageExtent.height,pcomp->imageExtent.width,pcomp->imageExtent.height};
	frameVec *= 2.0f;
	frameVec -= 1.0f;

	float pushConstants[] = {
		frameVec.x,frameVec.y,
		frameVec.z,frameVec.w,
		0,1,0,1
		//(float)pcomp->imageExtent.height/(float)pcomp->imageExtent.width,0.0f,0.0f,0.0f //TODO: should be in separate constant
	};
	//aspectRatio, time, 0, 0
	//vkCmdPushConstants(*pcommandBuffer,pPipeline->pipelineLayout,VK_SHADER_STAGE_GEOMETRY_BIT,0,64,&tr);
	vkCmdPushConstants(*pcommandBuffer,pPipeline->pipelineLayout,VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,0,32,pushConstants);
	vkCmdDraw(*pcommandBuffer,1,1,0,0);
}

CompositorInterface::CompositorInterface(uint _physicalDevIndex) : physicalDevIndex(_physicalDevIndex){
	//
}

CompositorInterface::~CompositorInterface(){
	DebugPrintf(stdout,"Compositor cleanup");
	vkDeviceWaitIdle(logicalDev);

	delete []pcommandBuffers;

	vkDestroyCommandPool(logicalDev,commandPool,0);

	delete pdefaultPipeline;

	for(uint i = 0; i < SEMAPHORE_INDEX_COUNT; ++i)
		vkDestroySemaphore(logicalDev,semaphore[i],0);

	for(uint i = 0; i < swapChainImageCount; ++i){
		vkDestroyFramebuffer(logicalDev,pframebuffers[i],0);
		vkDestroyImageView(logicalDev,pswapChainImageViews[i],0);
	}
	delete []pswapChainImageViews;
	delete []pswapChainImages;
	vkDestroySwapchainKHR(logicalDev,swapChain,0);

	vkDestroyRenderPass(logicalDev,renderPass,0);

	vkDestroyDevice(logicalDev,0);

	((PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT"))(instance,debugReportCb,0);

	vkDestroySurfaceKHR(instance,surface,0);
	vkDestroyInstance(instance,0);
}

void CompositorInterface::Initialize(){
	//https://gist.github.com/graphitemaster/e162a24e57379af840d4
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
				//vkEnumerateInstanceExtensionProperties(players[j],&extCount,0);
				//VkExtensionProperties extProps;
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
	appInfo.pApplicationName = "xwm";
	appInfo.applicationVersion = VK_MAKE_VERSION(0,0,1);
	appInfo.pEngineName = "xwm-engine";
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
		printf("max push constant size: %u\n",devProps.limits.maxPushConstantsSize);
	}

	if(physicalDevIndex >= devCount){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Invalid gpu-index (%u) exceeds the number of available devices (%u).",physicalDevIndex,devCount);
		throw Exception();
	}

	physicalDev = pdevices[physicalDevIndex];

	delete []pdevices;

	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDev,surface,&surfaceCapabilities);
	//TODO

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
	for(uint i = 0; i < SEMAPHORE_INDEX_COUNT; ++i)
		if(vkCreateSemaphore(logicalDev,&semaphoreCreateInfo,0,&semaphore[i]) != VK_SUCCESS)
			throw Exception("Failed to create a semaphore.");

	if(!(pdefaultPipeline = CompositorPipeline::CreateDefault(this)))
		throw Exception("Failed to create the default compositor pipeline.");
	
	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndex[QUEUE_INDEX_GRAPHICS];
	commandPoolCreateInfo.flags = 0; //TODO: flags
	if(vkCreateCommandPool(logicalDev,&commandPoolCreateInfo,0,&commandPool) != VK_SUCCESS)
		throw Exception("Failed to create a command pool.");
	
	pcommandBuffers = new VkCommandBuffer[swapChainImageCount];

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = swapChainImageCount;
	if(vkAllocateCommandBuffers(logicalDev,&commandBufferAllocateInfo,pcommandBuffers) != VK_SUCCESS)
		throw Exception("Failed to allocate command buffers.");

}

VkShaderModule CompositorInterface::CreateShaderModule(const char *pbin, size_t binlen) const{
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.codeSize = binlen;
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(pbin);

	VkShaderModule shaderModule;
	if(vkCreateShaderModule(logicalDev,&shaderModuleCreateInfo,0,&shaderModule) != VK_SUCCESS)
		return 0;
	
	return shaderModule;
}

VkShaderModule CompositorInterface::CreateShaderModuleFromFile(const char *psrc) const{
	FILE *pf = fopen(psrc,"rb");
	fseek(pf,0,SEEK_END);
	size_t len = ftell(pf);
	fseek(pf,0,SEEK_SET);
	
	char *pbuf = new char[len];
	fread(pbuf,1,len,pf);
	fclose(pf);

	VkShaderModule shaderModule = CreateShaderModule(pbuf,len);
	delete []pbuf;

	return shaderModule;
}

void CompositorInterface::CreateRenderQueue(const WManager::Container *pcontainer){
	for(const WManager::Container *pcont = pcontainer; pcont; pcont = pcont->pnext){
		if(!pcont->pclient)
			continue;
		WManager::Rectangle r = pcont->pclient->GetRect();
		VkRect2D frame;
		frame.offset = {r.x,r.y};
		frame.extent = {r.w,r.h};

		FrameObject &frameObject = frameObjectPool.emplace_back(pdefaultPipeline,this,frame);
		renderQueue.push_back(&frameObject);
		if(pcont->pch)
			CreateRenderQueue(pcont->pch);
		//worry about stacks later
		//stacks: render in same order, except skip focus? Focus is rendered last.
	}
}

void CompositorInterface::GenerateCommandBuffers(const WManager::Container *proot){
	//TODO: improved mechanism to generate only the command buffer for the next frame.
	//This function checks wether the command buffer has to be rerecorded
	//constants:
	//-aspect ratio
	//-transformation 3x3 matrix
	//-time since creation of the object
	if(!proot)
		return;

	//Create a render list elements arranged from back to front
	renderQueue.clear();
	frameObjectPool.clear();
	CreateRenderQueue(proot->pch);
	//
	for(uint i = 0; i < swapChainImageCount; ++i){
		VkCommandBufferBeginInfo commandBufferBeginInfo = {};
		commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		commandBufferBeginInfo.pInheritanceInfo = 0;
		if(vkBeginCommandBuffer(pcommandBuffers[i],&commandBufferBeginInfo) != VK_SUCCESS)
			throw Exception("Failed to begin command buffer recording.");

		static const VkClearValue clearValue = {0.0f,0.0f,0.0f,1.0};
		VkRenderPassBeginInfo renderPassBeginInfo = {};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.framebuffer = pframebuffers[i];
		renderPassBeginInfo.renderArea.offset = {0,0};
		renderPassBeginInfo.renderArea.extent = imageExtent;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = &clearValue;
		vkCmdBeginRenderPass(pcommandBuffers[i],&renderPassBeginInfo,VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(pcommandBuffers[i],VK_PIPELINE_BIND_POINT_GRAPHICS,pdefaultPipeline->pipeline);
		
		//vkCmdDraw(pcommandBuffers[i],3,1,0,0);
		//vkCmdDraw(pcommandBuffers[i],1,1,0,0);
		/*for(RenderObject *prenderObject : renderQueue){
			//
			prenderObject->Draw(&pcommandBuffers[i]);
		}*/
		//renderQueue[1]->Draw(&pcommandBuffers[i]);
		//frameObjectPool[0].Draw(&pcommandBuffers[i]);
		//frameObjectPool[1].Draw(&pcommandBuffers[i]);
		for(uint j = 0, n = frameObjectPool.size(); j < n; ++j)
			frameObjectPool[j].Draw(&pcommandBuffers[i]);

		vkCmdEndRenderPass(pcommandBuffers[i]);

		if(vkEndCommandBuffer(pcommandBuffers[i]) != VK_SUCCESS)
			throw Exception("Failed to end command buffer recording.");
	}
}

void CompositorInterface::Present(){
	uint imageIndex;
	if(vkAcquireNextImageKHR(logicalDev,swapChain,std::numeric_limits<uint64_t>::max(),semaphore[SEMAPHORE_INDEX_IMAGE_AVAILABLE],0,&imageIndex) != VK_SUCCESS)
		throw Exception("Failed to acquire a swap chain image.\n");
	//
	VkPipelineStageFlags pipelineStageFlags[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	//VkPipelineStageFlags pipelineStageFlags[] = {VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT};
	
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphore[SEMAPHORE_INDEX_IMAGE_AVAILABLE];
	submitInfo.pWaitDstStageMask = pipelineStageFlags;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphore[SEMAPHORE_INDEX_RENDER_FINISHED];
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &pcommandBuffers[imageIndex];
	if(vkQueueSubmit(queue[QUEUE_INDEX_GRAPHICS],1,&submitInfo,0) != VK_SUCCESS)
		throw Exception("Failed to submit a queue.");
	
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &semaphore[SEMAPHORE_INDEX_RENDER_FINISHED];
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapChain;
	presentInfo.pImageIndices = &imageIndex;
	presentInfo.pResults = 0;
	vkQueuePresentKHR(queue[QUEUE_INDEX_PRESENT],&presentInfo);

	//vkDeviceWaitIdle(logicalDev);
}

VKAPI_ATTR VkBool32 VKAPI_CALL CompositorInterface::ValidationLayerDebugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char *playerPrefix, const char *pmsg, void *puserData){
	DebugPrintf(stdout,"validation layer: %s\n",pmsg);
	return VK_FALSE;
}

X11Compositor::X11Compositor(uint physicalDevIndex, const Backend::X11Backend *pbackend) : CompositorInterface(physicalDevIndex){
	this->pbackend = pbackend;
}

X11Compositor::~X11Compositor(){
	//
}

void X11Compositor::Start(){
	Initialize();
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
	xcbSurfaceCreateInfo.window = pbackend->overlay;
	//if(((PFN_vkCreateXcbSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateXcbSurfaceKHR"))(instance,&xcbSurfaceCreateInfo,0,psurface) != VK_SUCCESS)
	if(vkCreateXcbSurfaceKHR(instance,&xcbSurfaceCreateInfo,0,psurface) != VK_SUCCESS)
		throw("Failed to create KHR surface.");
}

VkExtent2D X11Compositor::GetExtent() const{
	xcb_get_geometry_cookie_t geometryCookie = xcb_get_geometry(pbackend->pcon,pbackend->overlay);
	xcb_get_geometry_reply_t *pgeometryReply = xcb_get_geometry_reply(pbackend->pcon,geometryCookie,0);
	if(!pgeometryReply)
		throw("Invalid geometry size - unable to retrieve.");
	VkExtent2D e = (VkExtent2D){pgeometryReply->width,pgeometryReply->height};
	free(pgeometryReply);
	return e;
}

}

