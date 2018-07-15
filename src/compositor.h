#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
//#include <vector>

namespace Backend{
class X11Backend;
};

namespace Compositor{

//class FrameObjectDesc

/*class FrameObject{
public:
	//single frame object consists of window and the frame/decorations
	FrameObject(class CompositorInterface *);
	~FrameObject();
	//SetTranslation
	class CompositorInterface *pcomp;
};*/

class CompositorPipeline{
public:
	CompositorPipeline(class CompositorInterface *);
	~CompositorPipeline();
	class CompositorInterface *pcomp;
	VkShaderModule vertexShader;
	VkShaderModule fragmentShader;
	VkPipelineLayout pipelineLayout;
	//renderPass default
	VkPipeline pipeline;

	static CompositorPipeline * CreateDefault(CompositorInterface *pcomp);
};

class CompositorInterface{
//friend class FrameObject;
friend class CompositorPipeline;
public:
	CompositorInterface(uint);
	virtual ~CompositorInterface();
	virtual void Start() = 0;
protected:
	void Initialize();
	VkShaderModule CreateShaderModule(const char *, size_t);
	VkShaderModule CreateShaderModuleFromFile(const char *);
	virtual bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const = 0;
	virtual void CreateSurfaceKHR(VkSurfaceKHR *) const = 0;
	virtual VkExtent2D GetExtent() const = 0;
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
	VkRenderPass renderPass;
	VkSwapchainKHR swapChain;
	VkExtent2D imageExtent;
	VkImage *pswapChainImages;
	VkImageView *pswapChainImageViews;
	VkFramebuffer *pframebuffers;
	uint queueFamilyIndex[QUEUE_INDEX_COUNT]; //
	uint physicalDevIndex;
	uint swapChainImageCount;

	//std::vector<FrameObject *> frameObjects;
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
};

//Default compositor assumes XCB for its surface
class X11Compositor: public CompositorInterface{
public:
	X11Compositor(uint, const Backend::X11Backend *);
	virtual ~X11Compositor();
	void Start();
	bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const;
	void CreateSurfaceKHR(VkSurfaceKHR *) const;
	VkExtent2D GetExtent() const;
private:
	const Backend::X11Backend *pbackend;
};

}

#endif

