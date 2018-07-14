#include "main.h"
#include "backend.h"
#include "compositor.h"
#include <set>

namespace Compositor{

CompositorInterface::CompositorInterface(uint physicalDevIndex){
	this->physicalDevIndex = physicalDevIndex;
}

CompositorInterface::~CompositorInterface(){
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
	}

	if(physicalDevIndex >= devCount){
		snprintf(Exception::buffer,sizeof(Exception::buffer),"Invalid gpu-index (%u) exceeds the number of available devices (%u).",physicalDevIndex,devCount);
		throw Exception();
	}

	physicalDev = pdevices[physicalDevIndex];
	
	delete []pdevices;

	uint queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDev,&queueFamilyCount,0);
	VkQueueFamilyProperties *pqueueFamilyProps = new VkQueueFamilyProperties[queueFamilyCount];
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDev,&queueFamilyCount,pqueueFamilyProps);

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
		//DebugPrintf("
		bool compatible = CheckDeviceCompatibility(physicalDev,i);
		vkGetPhysicalDeviceSurfaceSupportKHR(physicalDev,i,surface,&presentSupport);
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

	VkDeviceQueueCreateInfo queueCreateInfo[QUEUE_INDEX_COUNT];
	uint queueCount = 0;
	for(uint queueFamilyIndex1 : queueSet){
		//logical device
		const float queuePriorities[] = {1.0f};
		queueCreateInfo[queueCount] = (VkDeviceQueueCreateInfo){};
		queueCreateInfo[queueCount].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo[queueCount].queueFamilyIndex = queueFamilyIndex1;
		queueCreateInfo[queueCount].queueCount = 1;
		queueCreateInfo[queueCount].pQueuePriorities = queuePriorities;
		++queueCount;
	}

	VkPhysicalDeviceFeatures physicalDevFeatures = {};

	VkDeviceCreateInfo devCreateInfo = {};
	devCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	devCreateInfo.pQueueCreateInfos = queueCreateInfo;
	devCreateInfo.queueCreateInfoCount = queueCount;
	devCreateInfo.pEnabledFeatures = &physicalDevFeatures;
	devCreateInfo.enabledExtensionCount = 0; //TODO: device extensions, surface?
	devCreateInfo.ppEnabledLayerNames = players;
	devCreateInfo.enabledLayerCount = sizeof(players)/sizeof(players[0]);
	if(vkCreateDevice(physicalDev,&devCreateInfo,0,&logicalDev) != VK_SUCCESS)
		throw Exception("Failed to create a logical device.");

	for(uint i = 0; i < QUEUE_INDEX_COUNT; ++i)
		vkGetDeviceQueue(logicalDev,queueFamilyIndex[i],0,&queue[i]);
	
	DebugPrintf(stdout,"Initialization success.\n");
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

bool X11Compositor::CheckDeviceCompatibility(VkPhysicalDevice physicalDev, uint queueFamilyIndex){
	xcb_visualid_t visualid = pbackend->pscr->root_visual;
	return vkGetPhysicalDeviceXcbPresentationSupportKHR(physicalDev,queueFamilyIndex,pbackend->pcon,visualid) == VK_TRUE;
}

void X11Compositor::CreateSurfaceKHR(VkSurfaceKHR *psurface){
	VkXcbSurfaceCreateInfoKHR xcbSurfaceCreateInfo = {};
	xcbSurfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
	xcbSurfaceCreateInfo.pNext = 0;
	xcbSurfaceCreateInfo.connection = pbackend->pcon; //pcon
	xcbSurfaceCreateInfo.window = pbackend->overlay;
	//if(((PFN_vkCreateXcbSurfaceKHR)vkGetInstanceProcAddr(instance,"vkCreateXcbSurfaceKHR"))(instance,&xcbSurfaceCreateInfo,0,psurface) != VK_SUCCESS)
	if(vkCreateXcbSurfaceKHR(instance,&xcbSurfaceCreateInfo,0,psurface) != VK_SUCCESS){
		//DebugPrintf(stderr,"Failed to create KHR surface.\n");
		throw("Failed to create KHR surface.");
	}
	//
	//PFN_vkCreateXcbSurfaceKHR
}

}

