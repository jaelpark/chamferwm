#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
//#include <glm/glm.hpp>
#include <vector> //render queue

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
	VkShaderModule geometryShader;
	VkShaderModule fragmentShader;
	VkPipelineLayout pipelineLayout;
	//renderPass default
	VkPipeline pipeline;

	static CompositorPipeline * CreateDefault(CompositorInterface *pcomp);
};

//Render queue object: can be a frame, text etc.
class RenderObject{
public:
	RenderObject(const CompositorPipeline *, const class CompositorInterface *);
	virtual ~RenderObject();
	virtual void Draw(const VkCommandBuffer *) = 0;
	//If the pipeline changes, the renderer binds a new one
	const CompositorPipeline *pPipeline;
	const CompositorInterface *pcomp;
};

class FrameObject : public RenderObject{
public:
	FrameObject(const CompositorPipeline *, const class CompositorInterface *, VkRect2D);
	~FrameObject();
	void Draw(const VkCommandBuffer *);
	VkRect2D frame;
};

class CompositorInterface{
//friend class FrameObject;
friend class CompositorPipeline;
friend class RenderObject;
friend class FrameObject;
public:
	CompositorInterface(uint);
	virtual ~CompositorInterface();
	virtual void Start() = 0;
	virtual void Stop() = 0;
	virtual void SetupClient(const WManager::Client *) = 0;
protected:
	void InitializeRenderEngine();
	VkShaderModule CreateShaderModule(const char *, size_t) const;
	VkShaderModule CreateShaderModuleFromFile(const char *) const;
	//void SetShaderLoadPath(const char *);
	void CreateRenderQueue(const WManager::Container *);
	void GenerateCommandBuffers(const WManager::Container *);
	void Present();
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
	enum SEMAPHORE_INDEX{
		SEMAPHORE_INDEX_IMAGE_AVAILABLE,
		SEMAPHORE_INDEX_RENDER_FINISHED,
		SEMAPHORE_INDEX_COUNT
	};
	VkSemaphore semaphore[SEMAPHORE_INDEX_COUNT];
	VkCommandPool commandPool;
	VkCommandBuffer *pcommandBuffers;
	uint queueFamilyIndex[QUEUE_INDEX_COUNT]; //
	uint physicalDevIndex;
	uint swapChainImageCount;

	//placeholder variables
	CompositorPipeline *pdefaultPipeline; //temp?
	//const char *pshaderPath;

	std::vector<RenderObject *> renderQueue;
	std::vector<FrameObject> frameObjectPool;
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
};

//Default compositor assumes XCB for its surface
class X11Compositor: public CompositorInterface{
public:
	X11Compositor(uint, const Backend::X11Backend *);
	~X11Compositor();
	virtual void Start();
	virtual void Stop();
	void SetupClient(const WManager::Client *);
	bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const;
	void CreateSurfaceKHR(VkSurfaceKHR *) const;
	VkExtent2D GetExtent() const;
protected:
	const Backend::X11Backend *pbackend;
	xcb_window_t overlay;
};

class X11DebugCompositor : public X11Compositor{
public:
	X11DebugCompositor(uint, const Backend::X11Backend *);
	~X11DebugCompositor();
	void Start();
	void Stop();
};

}

#endif

