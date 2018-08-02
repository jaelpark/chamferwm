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
	ShaderModule(Blob *, const class CompositorInterface *);
	~ShaderModule();

	const class CompositorInterface *pcomp;
	VkShaderModule shaderModule;
	VkDescriptorSetLayout *pdescSetLayouts;
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

}

#endif

