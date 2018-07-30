#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include <vector>

#include <xcb/composite.h>
#include <xcb/damage.h>

namespace Backend{
class X11Backend;
};

namespace Compositor{

class Texture{
public:
	Texture(uint, uint, VkFormat, const class CompositorInterface *pcomp);
	~Texture();
	const void * Map() const;
	void Unmap(const VkCommandBuffer *);
	const class CompositorInterface *pcomp;
	VkImage image;
	VkImageLayout imageLayout;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	uint stagingMemorySize;
	uint w, h;

	static const std::vector<std::pair<VkFormat, uint>> formatSizeMap;
};

class CompositorPipeline{
public:
	CompositorPipeline(const class CompositorInterface *);
	~CompositorPipeline();
	const class CompositorInterface *pcomp;
	VkShaderModule vertexShader;
	VkShaderModule geometryShader;
	VkShaderModule fragmentShader;
	VkPipelineLayout pipelineLayout;
	//renderPass default
	VkPipeline pipeline;

	static CompositorPipeline * CreateDefault(const CompositorInterface *pcomp);
};

//Render queue object: can be a frame, text etc.
class RenderObject{
public:
	RenderObject(const CompositorPipeline *, const class CompositorInterface *);
	virtual ~RenderObject();
	virtual void Draw(const VkCommandBuffer *) = 0;
	//If the pipeline changes, the renderer binds a new one
	const CompositorPipeline *pPipeline;
	const class CompositorInterface *pcomp;
};

class FrameObject : public RenderObject{
public:
	FrameObject(const CompositorPipeline *, const class CompositorInterface *, VkRect2D);
	~FrameObject();
	void Draw(const VkCommandBuffer *);
	VkRect2D frame;
};

class ClientFrame{
public:
	ClientFrame(const class CompositorInterface *);
	virtual ~ClientFrame();
	virtual void UpdateContents(const VkCommandBuffer *) = 0;
	Texture *ptexture;
	const class CompositorInterface *pcomp;
};

class CompositorInterface{
friend class Texture;
friend class CompositorPipeline;
friend class RenderObject;
friend class FrameObject;
friend class ClientFrame;
public:
	CompositorInterface(uint);
	virtual ~CompositorInterface();
	virtual void Start() = 0;
	virtual void Stop() = 0;
protected:
	void InitializeRenderEngine();
	void DestroyRenderEngine();
	VkShaderModule CreateShaderModule(const char *, size_t) const;
	VkShaderModule CreateShaderModuleFromFile(const char *) const;
	void CreateRenderQueue(const WManager::Container *);
	bool PollFrameFence();
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
	VkSemaphore (*psemaphore)[SEMAPHORE_INDEX_COUNT];
	VkFence *pfence;
	VkCommandPool commandPool;
	VkCommandBuffer *pcommandBuffers;
	uint queueFamilyIndex[QUEUE_INDEX_COUNT]; //
	uint physicalDevIndex;
	uint swapChainImageCount;
	uint currentFrame;

	//placeholder variables
	CompositorPipeline *pdefaultPipeline; //temp?
	//const char *pshaderPath;

	std::vector<RenderObject *> renderQueue;
	std::vector<FrameObject> frameObjectPool;
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
};

class X11ClientFrame : public ClientFrame, public Backend::X11Client{
public:
	X11ClientFrame(const Backend::X11Client::CreateInfo *, const CompositorInterface *);
	~X11ClientFrame();
	void UpdateContents(const VkCommandBuffer *);
	xcb_pixmap_t windowPixmap;
	xcb_damage_damage_t damage;
};

//Default compositor assumes XCB for its surface
class X11Compositor : public CompositorInterface{
public:
	X11Compositor(uint, const Backend::X11Backend *);
	~X11Compositor();
	virtual void Start();
	virtual void Stop();
	//void SetupClient(const WManager::Client *);
	bool FilterEvent(const Backend::X11Event *);
	bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const;
	void CreateSurfaceKHR(VkSurfaceKHR *) const;
	VkExtent2D GetExtent() const;
protected:
	const Backend::X11Backend *pbackend;
	xcb_window_t overlay;
	sint compEventOffset;
	sint compErrorOffset;
	sint xfixesEventOffset;
	sint xfixesErrorOffset;
	sint damageEventOffset;
	sint damageErrorOffset;
};

class X11DebugClientFrame : public ClientFrame, public Backend::DebugClient{
public:
	X11DebugClientFrame(const Backend::DebugClient::CreateInfo *, const CompositorInterface *);
	~X11DebugClientFrame();
	void UpdateContents(const VkCommandBuffer *);
};

class X11DebugCompositor : public X11Compositor{
public:
	X11DebugCompositor(uint, const Backend::X11Backend *);
	~X11DebugCompositor();
	void Start();
	void Stop();
};

class NullCompositor : public CompositorInterface{
public:
	NullCompositor();
	~NullCompositor();
	void Start();
	void Stop();
	bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const;
	void CreateSurfaceKHR(VkSurfaceKHR *) const;
	VkExtent2D GetExtent() const;
};

}

#endif

