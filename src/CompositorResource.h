#ifndef COMPOSITOR_RESOURCE_H
#define COMPOSITOR_RESOURCE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

namespace Compositor{

#define TEXTURE_BASE_FLAG_SKIP 0x1

class TextureBase{
public:
	TextureBase(uint, uint, VkFormat, const VkComponentMapping *, uint, const class CompositorInterface *);
	virtual ~TextureBase();
	//
	uint w, h;
	uint flags;
	uint formatIndex;
	uint componentMappingHash;
	VkImageLayout imageLayout;

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;

	const class CompositorInterface *pcomp;

	static inline uint GetComponentMappingHash(const VkComponentMapping *pcomponentMapping){
		return pcomponentMapping->r|((uint)pcomponentMapping->g<<8)|((uint)pcomponentMapping->b<<16)|((uint)pcomponentMapping->a<<24);
	}
	static const std::vector<std::pair<VkFormat, uint>> formatSizeMap;
	static const VkComponentMapping defaultComponentMapping;
};

//texture class for shader reads
class TextureStaged : virtual public TextureBase{
public:
	TextureStaged(uint, uint, VkFormat, const VkComponentMapping *, uint, const class CompositorInterface *);
	virtual ~TextureStaged();
	const void * Map() const;
	void Unmap(const VkCommandBuffer *, const VkRect2D *, uint);
	//virtual void Update(const VkCommandBuffer *, const VkRect2D *, uint) = 0;

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	uint stagingMemorySize;

	std::vector<VkBufferImageCopy> bufferImageCopyBuffer; //to avoid repeated dynamic allocations each time texture is updated in multiple regions
};

class TexturePixmap : virtual public TextureBase{
public:
	TexturePixmap(uint, uint, const VkComponentMapping *, uint, const class CompositorInterface *);
	virtual ~TexturePixmap();
	bool Attach(xcb_pixmap_t);
	void Detach();
	void Update(const VkCommandBuffer *, const VkRect2D *, uint);

	const VkComponentMapping *pcomponentMapping;
	
	sint dmafd;
	const class X11Compositor *pcomp11;

	static const VkComponentMapping pixmapComponentMapping;
	static const VkComponentMapping pixmapComponentMapping24;
};

class TextureHostPointer : virtual public TextureBase{
public:
	TextureHostPointer(uint, uint, const VkComponentMapping *, uint, const class CompositorInterface *);
	virtual ~TextureHostPointer();
	bool Attach(unsigned char *);
	void Detach(uint64);
	void Update(const VkCommandBuffer *, const VkRect2D *, uint);

	const VkComponentMapping *pcomponentMapping;

	VkBuffer transferBuffer;
	std::vector<std::tuple<uint64, VkDeviceMemory, VkBuffer>> discards;
};

class TextureDMABuffer : public TexturePixmap{
public:
	TextureDMABuffer(uint, uint, const VkComponentMapping *, uint, const class CompositorInterface *);
	~TextureDMABuffer();
};

class TextureSharedMemory : public TextureHostPointer{
public:
	TextureSharedMemory(uint, uint, const VkComponentMapping *, uint, const class CompositorInterface *);
	~TextureSharedMemory();
};

class TextureSharedMemoryStaged : public TextureStaged{
public:
	TextureSharedMemoryStaged(uint, uint, const VkComponentMapping *, uint, const class CompositorInterface *);
	~TextureSharedMemoryStaged();
};

class TextureCompatible : public TextureStaged{
public:
	TextureCompatible(uint, uint, const VkComponentMapping *, uint, const class CompositorInterface *);
	~TextureCompatible();
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
		INPUT_TEXCOORD_SFLOAT2,
		INPUT_TEXCOORD_UINT2,
		INPUT_COUNT
	};
	static const std::vector<std::tuple<const char *, VkFormat, uint>> semanticMap;
	enum VARIABLE{
		VARIABLE_XY0,
		VARIABLE_XY1,
		VARIABLE_TRANSFORM,
		VARIABLE_SCREEN,
		VARIABLE_MARGIN,
		VARIABLE_TITLEPAD,
		VARIABLE_TITLESPAN,
		VARIABLE_STACKINDEX,
		VARIABLE_FLAGS,
		VARIABLE_TIME,
		VARIABLE_COUNT
	};
	static const std::vector<std::tuple<const char *, uint>> variableMap;
};

class Pipeline{
public:
	Pipeline(ShaderModule *, ShaderModule *, ShaderModule *, const std::vector<std::pair<ShaderModule::INPUT, uint>> *, const VkPipelineInputAssemblyStateCreateInfo *, const VkPipelineRasterizationStateCreateInfo *, const VkPipelineDepthStencilStateCreateInfo *, const VkPipelineColorBlendStateCreateInfo *, const VkPipelineDynamicStateCreateInfo *, const class CompositorInterface *);
	virtual ~Pipeline();

	enum SHADER_MODULE{
		SHADER_MODULE_VERTEX,
		SHADER_MODULE_GEOMETRY,
		SHADER_MODULE_FRAGMENT,
		SHADER_MODULE_COUNT
	}; //note: code in Pipeline() relies on this order
	ShaderModule *pshaderModule[SHADER_MODULE_COUNT];
	const class CompositorInterface *pcomp;
	VkPushConstantRange pushConstantRange;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

class ClientPipeline : public Pipeline{ //, public Desc
public:
	ClientPipeline(ShaderModule *, ShaderModule *, ShaderModule *, const std::vector<std::pair<ShaderModule::INPUT, uint>> *, const class CompositorInterface *);
	~ClientPipeline();
	static const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
	static const VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	static const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
	static const VkDynamicState dynamicStatesScissoring[1];
	static const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo;
	static const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfoScissoring;
	static const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
};

class TextPipeline : public Pipeline{
public:
	TextPipeline(ShaderModule *, ShaderModule *, ShaderModule *, const std::vector<std::pair<ShaderModule::INPUT, uint>> *, const class CompositorInterface *);
	~TextPipeline();
	static const VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo;
	static const VkPipelineColorBlendAttachmentState colorBlendAttachmentState;
	static const VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo;
	static const VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo;
};

}

#endif

