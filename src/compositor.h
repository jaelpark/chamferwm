#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <vulkan/vulkan.h>
//#include <xcb/xproto.h>
#include <vulkan/vulkan_xcb.h>

namespace Backend{
class Default;
};

namespace Compositor{

class CompositorInterface{
public:
	CompositorInterface(uint);
	virtual ~CompositorInterface();
	virtual void Start() = 0;
protected:
	void Initialize();
	virtual bool CheckDeviceCompatibility(VkPhysicalDevice, uint) = 0;
	virtual void CreateSurfaceKHR(VkSurfaceKHR *) = 0;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkDebugReportCallbackEXT debugReportCb;
	VkPhysicalDevice physicalDev;
	VkDevice logicalDev;
	enum QUEUE_INDEX{
		QUEUE_INDEX_GRAPHICS,
		QUEUE_INDEX_PRESENT,
		QUEUE_INDEX_COUNT
	};
	VkQueue queue[QUEUE_INDEX_COUNT];
	uint queueFamilyIndex[QUEUE_INDEX_COUNT]; //
	uint physicalDevIndex;
	
	//uint queue

	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
};

//Default compositor assumes XCB for its surface
class X11Compositor: public CompositorInterface{
public:
	X11Compositor(uint, const Backend::X11Backend *);
	virtual ~X11Compositor();
	void Start();
	bool CheckDeviceCompatibility(VkPhysicalDevice, uint);
	void CreateSurfaceKHR(VkSurfaceKHR *);
private:
	const Backend::X11Backend *pbackend;
};

}

#endif

