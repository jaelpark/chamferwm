#ifndef COMPOSITOR_RESOURCE_H
#define COMPOSITOR_RESOURCE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

struct gbm_bo;

namespace Compositor{

class TextureBase{
public:
	TextureBase(uint, uint, VkFormat, uint, const class CompositorInterface *);
	virtual ~TextureBase();
	//
	uint w, h;
	uint formatIndex;
	VkImageLayout imageLayout;

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;

	enum FLAG{
		FLAG_IGNORE_ALPHA = 0x1
	};

	const class CompositorInterface *pcomp;

	static const std::vector<std::pair<VkFormat, uint>> formatSizeMap;
};

//texture class for shader reads
class TextureStaged : virtual public TextureBase{
public:
	TextureStaged(uint, uint, VkFormat, uint, const class CompositorInterface *);
	virtual ~TextureStaged();
	const void * Map() const;
	void Unmap(const VkCommandBuffer *, const VkRect2D *, uint);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	uint stagingMemorySize;

	std::vector<VkBufferImageCopy> bufferImageCopyBuffer; //to avoid repeated dynamic allocations each time texture is updated in multiple regions
};

class TexturePixmap : virtual public TextureBase{
public:
	TexturePixmap(uint, uint, uint, const class CompositorInterface *);
	virtual ~TexturePixmap();
	void Attach(xcb_pixmap_t);
	void Detach();
	void Update(const VkCommandBuffer *, const VkRect2D *, uint);

	VkImage transferImage;
	VkDeviceMemory transferMemory;
	VkImageLayout transferImageLayout;

	std::vector<VkImageCopy> imageCopyBuffer;
	
	sint dmafd;
	struct gbm_bo *pgbmBufferObject;
	const class X11Compositor *pcomp11;
};

class TextureHostPointer : virtual public TextureBase{
public:
	TextureHostPointer(uint, uint, uint, const class CompositorInterface *);
	virtual ~TextureHostPointer();
	bool Attach(unsigned char *);
	void Detach(uint64);
	void Update(const VkCommandBuffer *, const VkRect2D *, uint);

	VkBuffer transferBuffer;
	VkDeviceMemory transferMemory;
	std::vector<std::tuple<uint64, VkDeviceMemory, VkBuffer>> discards;

	std::vector<VkBufferImageCopy> bufferImageCopyBuffer;
};

//class Texture : public TextureStaged, public TexturePixmap{
class Texture : public TextureStaged, public TextureHostPointer{
public:
	Texture(uint, uint, uint, const class CompositorInterface *);
	~Texture();
};

class Buffer{
public:
	Buffer(uint, VkBufferUsageFlagBits, const class CompositorInterface *);
	~Buffer();
	const void * Map() const;
	void Unmap(const VkCommandBuffer *);

	const class CompositorInterface *pcomp;
	VkBuffer buffer;
	VkDeviceMemory deviceMemory;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	uint stagingMemorySize;
	uint size;
};

class ShaderModule{
public:
	ShaderModule(const char *, const Blob *, const class CompositorInterface *);
	~ShaderModule();

	const class CompositorInterface *pcomp;
	const char *pname;
	VkShaderModule shaderModule;
	VkPushConstantRange *pPushConstantRanges;
	//VKPushConstantRange pushConstantRange; //only one range can exist
	VkDescriptorSetLayout *pdescSetLayouts;
	uint pushConstantBlockCount;
	uint setCount;
	//uint inputCount;
	//uint inputStride;

	struct Binding{
		const char *pname;
		VkDescriptorType type;
		uint setIndex;
		uint binding;
	};
	std::vector<Binding> bindings;

	struct Input{
		uint location; //index of the input
		uint binding;
		//uint offset; //byte offset
		uint semanticMapIndex; //index to semantic map
	};
	std::vector<Input> inputs;

	struct Variable{
		uint offset;
		uint variableMapIndex;
	};
	std::vector<Variable> variables;

	enum INPUT{
		INPUT_POSITION_SFLOAT2,
		INPUT_POSITION_UINT2,
		INPUT_COUNT
	};
	static const std::vector<std::tuple<const char *, VkFormat, uint>> semanticMap;
	enum VARIABLE{
		VARIABLE_XY0,
		VARIABLE_XY1,
		VARIABLE_SCREEN,
		VARIABLE_MARGIN,
		VARIABLE_FLAGS,
		VARIABLE_TIME,
		VARIABLE_COUNT
	};
	static const std::vector<std::tuple<const char *, uint>> variableMap;
};

class Pipeline{
public:
	Pipeline(ShaderModule *, ShaderModule *, ShaderModule *, const std::vector<std::pair<ShaderModule::INPUT, uint>> *, size_t, const VkPipelineInputAssemblyStateCreateInfo *, const VkPipelineRasterizationStateCreateInfo *, const VkPipelineDepthStencilStateCreateInfo *, const VkPipelineColorBlendStateCreateInfo *, const class CompositorInterface *);
	virtual ~Pipeline();

	enum SHADER_MODULE{
		SHADER_MODULE_VERTEX,
		SHADER_MODULE_GEOMETRY,
		SHADER_MODULE_FRAGMENT,
		SHADER_MODULE_COUNT
	}; //note: code in Pipeline() relies on this order
	ShaderModule *pshaderModule[SHADER_MODULE_COUNT];
	size_t vertexBufferLayoutHash;
	const class CompositorInterface *pcomp;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

class ClientPipeline : public Pipeline{ //, public Desc
public:
	ClientPipeline(ShaderModule *, ShaderModule *, ShaderModule *, const std::vector<std::pair<ShaderModule::INPUT, uint>> *, size_t, const class CompositorInterface *);
	~ClientPipeline();
	static VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
	static VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	static VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
	static VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
};

class TextPipeline : public Pipeline{
public:
	TextPipeline(ShaderModule *, ShaderModule *, ShaderModule *, const std::vector<std::pair<ShaderModule::INPUT, uint>> *, size_t, const class CompositorInterface *);
	~TextPipeline();
	static VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
	static VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	static VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
	static VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
};

}

#endif

