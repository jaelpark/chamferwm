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

//Render queue objects: can be frames, text etc.
class RenderObject{
public:
	RenderObject(const Pipeline *, const class CompositorInterface *);
	virtual ~RenderObject();
	virtual void Draw(const VkCommandBuffer *) = 0;
	//If the pipeline changes, the renderer binds a new one
protected:
	const Pipeline *pPipeline;
	const class CompositorInterface *pcomp;
	float time;
};

class RectangleObject : public RenderObject{
public:
	RectangleObject(const Pipeline *, const class CompositorInterface *, VkRect2D);
	virtual ~RectangleObject();
protected:
	void PushConstants(const VkCommandBuffer *);
	VkRect2D frame;
};

class FrameObject : public RectangleObject{
public:
	FrameObject(class ClientFrame *, const class CompositorInterface *, VkRect2D);
	~FrameObject();
	void Draw(const VkCommandBuffer *);
	class ClientFrame *pclientFrame;
};

class ClientFrame{
friend class CompositorInterface;
friend class FrameObject;
public:
	ClientFrame(uint, uint, class CompositorInterface *);
	virtual ~ClientFrame();
	virtual void UpdateContents(const VkCommandBuffer *) = 0;
	bool AssignPipeline(const Pipeline *);
protected:
	Texture *ptexture;
	class CompositorInterface *pcomp;
	struct PipelineDescriptorSet{
		const Pipeline *p;
		VkDescriptorSet *pdescSets[Pipeline::SHADER_MODULE_COUNT];
	};
	std::vector<PipelineDescriptorSet> descSets;
	PipelineDescriptorSet *passignedSet;
	struct timespec creationTime;
};

class CompositorInterface{
friend class Texture;
friend class ShaderModule;
friend class Pipeline;
friend class RenderObject;
friend class RectangleObject;
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
	void WaitIdle();
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
	VkCommandBuffer *pcopyCommandBuffers;

	//VkDescriptorSetLayout descSetLayout;
	VkDescriptorPool descPool;
	//VkDescriptorSet *pdescSets;

	uint queueFamilyIndex[QUEUE_INDEX_COUNT]; //
	uint physicalDevIndex;
	uint swapChainImageCount;
	uint currentFrame;

	//all the resources are preloaded for now
	std::vector<ShaderModule> shaders;
	std::vector<Pipeline> pipelines;

	//placeholder variables
public:
	Pipeline *pdefaultPipeline; //temp?
private:

	VkSampler pointSampler;
	//const char *pshaderPath;

	struct timespec frameTime;

	std::vector<RenderObject *> renderQueue;
	std::vector<FrameObject> frameObjectPool;
	//std::vector<TextureObject> textureObjectPool;

	std::vector<ClientFrame *> updateQueue;

	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
};

class X11ClientFrame : public Backend::X11Client, public ClientFrame{
public:
	X11ClientFrame(const Backend::X11Client::CreateInfo *, CompositorInterface *);
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

class X11DebugClientFrame : public Backend::DebugClient, public ClientFrame{
public:
	X11DebugClientFrame(const Backend::DebugClient::CreateInfo *, CompositorInterface *);
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

