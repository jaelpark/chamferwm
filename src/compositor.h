#ifndef COMPOSITOR_H
#define COMPOSITOR_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

#include <xcb/composite.h>
#include <xcb/damage.h>
#include <xcb/shm.h>
#include <xcb/dri3.h>

struct gbm_device;

namespace Backend{
class X11Backend;
};

namespace Compositor{

class Drawable{
friend class CompositorInterface;
public:
	Drawable(Pipeline *, CompositorInterface *);
	virtual ~Drawable();
	//
	bool AssignPipeline(const Pipeline *);
protected:
	void BindShaderResources(const std::vector<std::pair<ShaderModule::VARIABLE, const void *>> *, const VkCommandBuffer *);
	virtual void UpdateDescSets(){};
	class CompositorInterface *pcomp;
	struct PipelineDescriptorSet{
		uint64 fenceTag;
		const Pipeline *p;
		VkDescriptorSet *pdescSets[Pipeline::SHADER_MODULE_COUNT];
	};
	PipelineDescriptorSet *passignedSet;
	std::vector<PipelineDescriptorSet> descSets;
};

class ColorFrame : public Drawable{
friend class CompositorInterface;
public:
	ColorFrame(const char *[Pipeline::SHADER_MODULE_COUNT], class CompositorInterface *);
	virtual ~ColorFrame();
	void Draw(const VkRect2D &, const glm::vec2 &, const glm::vec2 &, uint, const VkCommandBuffer *);
protected:
	struct timespec creationTime;
	float time;
public:
	uint shaderUserFlags;
protected:
	uint shaderFlags; //current frame shader flags
	uint oldShaderFlags; //used to keep track of changes
};

class ClientFrame : public ColorFrame{
friend class CompositorInterface;
public:
	ClientFrame(const char *[Pipeline::SHADER_MODULE_COUNT], class CompositorInterface *);
	virtual ~ClientFrame();
	virtual void UpdateContents(const VkCommandBuffer *) = 0;
	void CreateSurface(uint, uint, uint);
	void AdjustSurface(uint, uint);
	void SetShaders(const char *[Pipeline::SHADER_MODULE_COUNT]);
	void SetTitle(const char *);
	enum SHADER_FLAG{
		SHADER_FLAG_FOCUS = 0x1,
		SHADER_FLAG_FLOATING = 0x2,
		SHADER_FLAG_USER_BIT = 0x4
	};
protected:
	void UpdateDescSets();
	Texture *ptexture;
	class Text *ptitle;
	uint surfaceDepth;
	bool fullRegionUpdate;
	bool animationCompleted;
};

class CompositorInterface{
friend class TextureBase;
friend class TextureStaged;
friend class TexturePixmap;
friend class TextureHostPointer;
friend class Texture;
friend class Buffer;
friend class ShaderModule;
friend class Pipeline;
friend class ClientPipeline;
friend class FontAtlas;
friend class Text;
friend class TextEngine;
friend class Drawable;
friend class ColorFrame;
friend class ClientFrame;
friend class X11ClientFrame;
friend class X11Background;
friend class X11DebugClientFrame;
public:
	struct Configuration{
		uint deviceIndex;
		bool debugLayers;
		bool scissoring;
		bool hostMemoryImport;
		bool enableAnimation;
		float animationDuration;
	};
	CompositorInterface(const Configuration *);
	virtual ~CompositorInterface();
	virtual void Start() = 0;
	virtual void Stop() = 0;
protected:
	void InitializeRenderEngine();
	void DestroyRenderEngine();
	void AddShader(const char *, const Blob *);
	void AddDamageRegion(const VkRect2D *);
	void AddDamageRegion(const WManager::Client *);
	void WaitIdle();
	void CreateRenderQueueAppendix(const WManager::Client *, const WManager::Container *);
	void CreateRenderQueue(const WManager::Container *, const WManager::Container *);
	bool PollFrameFence();
	void GenerateCommandBuffers(const WManager::Container *, const std::vector<std::pair<const WManager::Client *, WManager::Client *>> *, const WManager::Container *);
	void Present();
	virtual bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const = 0;
	virtual void CreateSurfaceKHR(VkSurfaceKHR *) const = 0;
	virtual VkExtent2D GetExtent() const = 0;
	virtual glm::vec2 GetDPI() const = 0;
	VkInstance instance;
	VkSurfaceKHR surface;
	VkDebugReportCallbackEXT debugReportCb;
	VkPhysicalDevice physicalDev;
	VkPhysicalDeviceProperties physicalDevProps;
	VkPhysicalDeviceExternalMemoryHostPropertiesEXT physicalDevExternalMemoryHostProps;
	VkDevice logicalDev;
	enum QUEUE_INDEX{
		QUEUE_INDEX_GRAPHICS,
		QUEUE_INDEX_PRESENT,
		//QUEUE_INDEX_TRANSFER,
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
	//back buffer to enable the use of previous frame contents
	//VkImage renderImage;
	//VkDeviceMemory renderImageDeviceMemory;

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
	uint imageIndex;

	template<class T>
	Pipeline * LoadPipeline(const char *[Pipeline::SHADER_MODULE_COUNT], const std::vector<std::pair<ShaderModule::INPUT,uint>> *);

	std::vector<ShaderModule> shaders;
	std::vector<std::pair<size_t, Pipeline *>> pipelines; //hash, pipeline

	std::vector<ClientFrame *> updateQueue;
	std::vector<ClientFrame *> titleUpdateQueue;
	//Scissoring regions based on client damage. Second (uint) is a bitmask for
	//swap chain images, indicating which image has been updated so far. When
	//all images have been updated, the region is removed from the list.
	std::vector<std::pair<VkRect2D, uint>> scissorRegions; 
	std::vector<VkRectLayerKHR> presentRectLayers;

	void ClearBackground();

	ColorFrame *pcolorBackground;
	ColorFrame *pbackground;
public:
	class TextEngine *ptextEngine;
	//class Text *ptestText;
protected:

	VkSampler pointSampler;

	struct timespec frameTime;
	uint64 frameTag; //frame counter, to prevent releasing resources still in use

	struct RenderObject{
		WManager::Client *pclient;
		ClientFrame *pclientFrame;
		//uint flags;
	};
	std::vector<RenderObject> renderQueue;
	std::deque<std::pair<const WManager::Client *, WManager::Client *>> appendixQueue;

	//Used textures get stored for potential reuse before they get destroyed.
	//Many of the allocated window textures will initially have some common reoccuring size.
	//The purpose of caching is also to avoid attempts to destroy resources that are currently used by the pipeline.
	Texture * CreateTexture(uint, uint, uint);
	//Texture * CreateTextureGeneric(uint, uint, VkFormat, uint);
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

	bool playingAnimation;

	//config
	bool debugLayers;
	bool scissoring;
	bool hostMemoryImport;

public:
	bool enableAnimation;
	float animationDuration;
protected:

	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *);
	static VKAPI_ATTR VkBool32 VKAPI_CALL ValidationLayerDebugCallback2(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *, void *);
};

class X11ClientFrame : public Backend::X11Client, public ClientFrame{
public:
	X11ClientFrame(WManager::Container *, const Backend::X11Client::CreateInfo *, const char *[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *);
	~X11ClientFrame();
	void UpdateContents(const VkCommandBuffer *);
	void AdjustSurface1();
	xcb_pixmap_t windowPixmap;
	xcb_shm_seg_t segment;
	xcb_damage_damage_t damage;
	std::vector<VkRect2D> damageRegions;
	sint shmid;
	unsigned char *pchpixels;
};

class X11Background : public ClientFrame{
public:
	X11Background(xcb_pixmap_t, uint, uint, const char *[Pipeline::SHADER_MODULE_COUNT], X11Compositor *);
	~X11Background();
	void UpdateContents(const VkCommandBuffer *);
	
	X11Compositor *pcomp11;
	xcb_pixmap_t pixmap;
	xcb_shm_seg_t segment;
	uint w, h;
	sint shmid;
	unsigned char *pchpixels;
};

//Default compositor assumes XCB for its surface
class X11Compositor : public CompositorInterface{
friend class TexturePixmap;
public:
	//Derivatives of compositor classes should not point to their default corresponding backend classes (Backend::Default in this case). This is to allow the compositor to be independent of the backend implementation, as long as it's based on X11 here.
	X11Compositor(const Configuration *, const Backend::X11Backend *);
	~X11Compositor();
	virtual void Start();
	virtual void Stop();
	//void SetupClient(const WManager::Client *);
	bool FilterEvent(const Backend::X11Event *);
	bool CheckPresentQueueCompatibility(VkPhysicalDevice, uint) const;
	void CreateSurfaceKHR(VkSurfaceKHR *) const;
	void SetBackgroundPixmap(const Backend::BackendPixmapProperty *);
	VkExtent2D GetExtent() const;
	glm::vec2 GetDPI() const;
	const Backend::X11Backend *pbackend;
	struct gbm_device *pgbmdev;
	sint cardfd;
	//X11Background *pbackground;
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
	X11DebugClientFrame(WManager::Container *, const Backend::DebugClient::CreateInfo *, const char *[Pipeline::SHADER_MODULE_COUNT], CompositorInterface *);
	~X11DebugClientFrame();
	void UpdateContents(const VkCommandBuffer *);
	void AdjustSurface1();
};

class X11DebugCompositor : public X11Compositor{
public:
	X11DebugCompositor(const Configuration *, const Backend::X11Backend *);
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
	glm::vec2 GetDPI() const;
	Configuration config{ //dummy config
		.deviceIndex = 0,
		.debugLayers = false,
		.scissoring = true
	};
};

}

#endif

