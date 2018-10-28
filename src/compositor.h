#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#include <xcb/composite.h>
#include <xcb/damage.h>

namespace Backend{
class X11Backend;
};

namespace Compositor{

class ClientFrame{
friend class CompositorInterface;
//friend class FrameObject;
public:
	ClientFrame(uint, uint, class CompositorInterface *);
	virtual ~ClientFrame();
	virtual void UpdateContents(const VkCommandBuffer *) = 0;
	void Draw(const VkRect2D &, const glm::vec2 &, uint, const VkCommandBuffer *);
	void AdjustSurface(uint, uint);
	bool AssignPipeline(const Pipeline *);
private:
	void UpdateDescSets();
protected:
	Texture *ptexture;
	class CompositorInterface *pcomp;
	struct PipelineDescriptorSet{
		uint64 fenceTag;
		const Pipeline *p;
		VkDescriptorSet *pdescSets[Pipeline::SHADER_MODULE_COUNT];
	};
	PipelineDescriptorSet *passignedSet;
	std::vector<PipelineDescriptorSet> descSets;
	struct timespec creationTime;
	float time;
	bool fullRegionUpdate;
};

class CompositorInterface{
friend class Texture;
friend class ShaderModule;
friend class Pipeline;
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
	void CreateRenderQueue(const WManager::Container *, const std::vector<std::pair<const WManager::Container *, WManager::Client *>> *, const WManager::Container *);
	bool PollFrameFence();
	void GenerateCommandBuffers(const WManager::Container *, const std::vector<std::pair<const WManager::Container *, WManager::Client *>> *, const WManager::Container *);
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

	//VkDescriptorPool descPool;
	std::deque<VkDescriptorPool> descPoolArray;
	std::vector<std::pair<VkDescriptorSet *, VkDescriptorPool>> descPoolReference;

	uint queueFamilyIndex[QUEUE_INDEX_COUNT]; //
	uint physicalDevIndex;
	uint swapChainImageCount;
	uint currentFrame;

	//all the resources are preloaded for now
	std::vector<ShaderModule> shaders;
	std::vector<Pipeline> pipelines;

	std::vector<ClientFrame *> updateQueue;

	//placeholder variables
public:
	Pipeline *pdefaultPipeline; //temp?
private:

	VkSampler pointSampler;
	//const char *pshaderPath;

	struct timespec frameTime;
	uint64 frameTag;

	//std::vector<renderObject *> renderQueue;
	//std::vector<FrameObject> frameObjectPool;
	struct RenderObject{
		WManager::Client *pclient;
		ClientFrame *pclientFrame;
		uint flags;
	};
	std::vector<RenderObject> renderQueue;
	std::deque<std::pair<const WManager::Container *, WManager::Client *>> appendixQueue;

	//Used textures get stored for potential reuse before they get destroyed.
	//Many of the allocated window textures will initially have some common reoccuring size.
	//The purpose of caching is also to avoid attempts to destroy resources that are currently used by the pipeline.
	Texture * CreateTexture(uint, uint);
	void ReleaseTexture(Texture *);

	struct TextureCacheEntry{
		Texture *ptexture;
		uint64 releaseTag;
		struct timespec releaseTime;
	};
	std::vector<TextureCacheEntry> textureCache;

	VkDescriptorSet * CreateDescSets(const ShaderModule *);
	void ReleaseDescSets(const ShaderModule *, VkDescriptorSet *);

	struct DescSetCacheEntry{
		VkDescriptorSet *pdescSets;
		uint setCount;
		uint64 releaseTag;
	};
	std::vector<DescSetCacheEntry> descSetCache;

	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
};

class X11ClientFrame : public Backend::X11Client, public ClientFrame{
public:
	X11ClientFrame(WManager::Container *, const Backend::X11Client::CreateInfo *, CompositorInterface *);
	~X11ClientFrame();
	void UpdateContents(const VkCommandBuffer *);
	void AdjustSurface1();
	xcb_pixmap_t windowPixmap;
	xcb_damage_damage_t damage;
	std::vector<VkRect2D> damageRegions;
};

//Default compositor assumes XCB for its surface
class X11Compositor : public CompositorInterface{
public:
	//Derivatives of compositor classes should not point to their default corresponding backend classes (Backend::Default in this case). This is to allow the compositor to be independent of the backend implementation, as long as it's based on X11 here.
	X11Compositor(uint, const Backend::X11Backend *);
	~X11Compositor();
	virtual void Start();
	virtual void Stop();
	//void SetupClient(const WManager::Client *);
	bool FilterEvent(const Backend::X11Event *);
	bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const;
	void CreateSurfaceKHR(VkSurfaceKHR *) const;
	VkExtent2D GetExtent() const;
	const Backend::X11Backend *pbackend;
	xcb_window_t overlay;
protected:
	sint compEventOffset;
	sint compErrorOffset;
	sint xfixesEventOffset;
	sint xfixesErrorOffset;
	sint damageEventOffset;
	sint damageErrorOffset;
};

class X11DebugClientFrame : public Backend::DebugClient, public ClientFrame{
public:
	X11DebugClientFrame(WManager::Container *, const Backend::DebugClient::CreateInfo *, CompositorInterface *);
	~X11DebugClientFrame();
	void UpdateContents(const VkCommandBuffer *);
	void AdjustSurface1();
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

