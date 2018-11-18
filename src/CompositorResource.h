#ifndef COMPOSITOR_RESOURCE_H
#define COMPOSITOR_RESOURCE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>

struct __GLXFBConfigRec;
typedef struct __GLXFBConfigRec *GLXFBConfig;

typedef unsigned long int XID;
typedef XID GLXPixmap;

namespace Compositor{

class TextureBase{
public:
	TextureBase(uint, uint, VkFormat, const class CompositorInterface *);
	virtual ~TextureBase();
	//
	uint w, h;
	uint formatIndex;
	VkImageLayout imageLayout;

	VkImage image;
	VkImageView imageView;
	VkDeviceMemory deviceMemory;

	const class CompositorInterface *pcomp;

	static const std::vector<std::pair<VkFormat, uint>> formatSizeMap;
};

//texture class for shader reads
class TextureStaged : virtual public TextureBase{
public:
	TextureStaged(uint, uint, VkFormat, const class CompositorInterface *);
	virtual ~TextureStaged();
	const void * Map() const;
	void Unmap(const VkCommandBuffer *, const VkRect2D *, uint);

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	uint stagingMemorySize;

	std::vector<VkBufferImageCopy> bufferImageCopyBuffer; //to avoid dynamic allocations each time texture is updated in multiple regions

	//const class CompositorInterface *pcomp;
};

class TexturePixmap : virtual public TextureBase{
public:
	TexturePixmap(uint, uint, const class CompositorInterface *);
	virtual ~TexturePixmap();
	void Attach(xcb_pixmap_t);
	void Detach();
	void Update(const VkCommandBuffer *);

	GLXPixmap glxpixmap;
	uint pixmapTexture;

	uint memoryObject;
	uint sharedTexture;

	const class X11Compositor *pcomp11;
};

class Texture : public TextureStaged, public TexturePixmap{
public:
	Texture(uint, uint, const class CompositorInterface *);
	~Texture();
};


class ShaderModule{
public:
	ShaderModule(const char *, const Blob *, const class CompositorInterface *);
	~ShaderModule();

	const class CompositorInterface *pcomp;
	const char *pname;
	VkShaderModule shaderModule;
	VkDescriptorSetLayout *pdescSetLayouts;
	uint setCount;

	struct Binding{
		const char *pname;
		VkDescriptorType type;
		uint setIndex;
		uint binding;
	};
	std::vector<Binding> bindings;
};

class Pipeline{
public:
	Pipeline(ShaderModule *, ShaderModule *, ShaderModule *, const class CompositorInterface *);
	~Pipeline();

	enum SHADER_MODULE{
		SHADER_MODULE_VERTEX,
		SHADER_MODULE_GEOMETRY,
		SHADER_MODULE_FRAGMENT,
		SHADER_MODULE_COUNT
	}; //note: code in Pipeline() relies on this order
	ShaderModule *pshaderModule[SHADER_MODULE_COUNT];
	const class CompositorInterface *pcomp;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
};

}

#endif

