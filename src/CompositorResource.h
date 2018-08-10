#ifndef COMPOSITOR_RESOURCE_H
#define COMPOSITOR_RESOURCE_H

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_xcb.h>
#include <vector>

namespace Compositor{

//texture class for shader reads
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

class ShaderModule{
public:
	ShaderModule(const Blob *, const class CompositorInterface *);
	~ShaderModule();

	const class CompositorInterface *pcomp;
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

