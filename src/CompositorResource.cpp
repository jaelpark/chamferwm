#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <algorithm>

#include "spirv_reflect.h"

namespace Compositor{

Texture::Texture(uint _w, uint _h, VkFormat format, const CompositorInterface *_pcomp) : imageLayout(VK_IMAGE_LAYOUT_UNDEFINED), w(_w), h(_h), pcomp(_pcomp){
	//staging buffer
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = (*std::find_if(formatSizeMap.begin(),formatSizeMap.end(),[&](auto &r)->bool{
		return r.first == format;
	})).second*w*h;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(vkCreateBuffer(pcomp->logicalDev,&bufferCreateInfo,0,&stagingBuffer) != VK_SUCCESS)
		throw Exception("Failed to create a staging buffer.");
	
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProps;
	vkGetPhysicalDeviceMemoryProperties(pcomp->physicalDev,&physicalDeviceMemoryProps);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(pcomp->logicalDev,stagingBuffer,&memoryRequirements);

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	for(memoryAllocateInfo.memoryTypeIndex = 0; memoryAllocateInfo.memoryTypeIndex < physicalDeviceMemoryProps.memoryTypeCount; memoryAllocateInfo.memoryTypeIndex++){
		if(memoryRequirements.memoryTypeBits & (1<<memoryAllocateInfo.memoryTypeIndex) && physicalDeviceMemoryProps.memoryTypes[memoryAllocateInfo.memoryTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
			break;
	}
	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&stagingMemory) != VK_SUCCESS)
		throw Exception("Failed to allocate staging buffer memory.");
	vkBindBufferMemory(pcomp->logicalDev,stagingBuffer,stagingMemory,0);

	stagingMemorySize = memoryRequirements.size;

	//image
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = w;
	imageCreateInfo.extent.height = h;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = format;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.flags = 0;
	if(vkCreateImage(pcomp->logicalDev,&imageCreateInfo,0,&image) != VK_SUCCESS)
		throw Exception("Failed to create an image.");
	
	vkGetImageMemoryRequirements(pcomp->logicalDev,image,&memoryRequirements);
	
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	for(memoryAllocateInfo.memoryTypeIndex = 0; memoryAllocateInfo.memoryTypeIndex < physicalDeviceMemoryProps.memoryTypeCount; memoryAllocateInfo.memoryTypeIndex++){
		if(memoryRequirements.memoryTypeBits & (1<<memoryAllocateInfo.memoryTypeIndex) && physicalDeviceMemoryProps.memoryTypes[memoryAllocateInfo.memoryTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			break;
	}

	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&deviceMemory) != VK_SUCCESS)
		throw Exception("Failed to allocate image memory.");
	vkBindImageMemory(pcomp->logicalDev,image,deviceMemory,0);

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if(vkCreateImageView(pcomp->logicalDev,&imageViewCreateInfo,0,&imageView) != VK_SUCCESS)
		throw Exception("Failed to create texture image view.");
}

Texture::~Texture(){
	vkDestroyImageView(pcomp->logicalDev,imageView,0);

	vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
	vkDestroyImage(pcomp->logicalDev,image,0);
	
	vkFreeMemory(pcomp->logicalDev,stagingMemory,0);
	vkDestroyBuffer(pcomp->logicalDev,stagingBuffer,0);
}

const void * Texture::Map() const{
	void *pdata;
	if(vkMapMemory(pcomp->logicalDev,stagingMemory,0,stagingMemorySize,0,&pdata) != VK_SUCCESS)
		return 0;
	return pdata;
}

void Texture::Unmap(const VkCommandBuffer *pcommandBuffer){
	vkUnmapMemory(pcomp->logicalDev,stagingMemory);

	VkImageSubresourceRange imageSubresourceRange = {};
	imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageSubresourceRange.baseMipLevel = 0;
	imageSubresourceRange.levelCount = 1;
	imageSubresourceRange.layerCount = 1;

	//create in host stage (map), use in transfer stage
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = imageSubresourceRange;
	/*imageMemoryBarrier.srcAccessMask =
		imageLayout == VK_IMAGE_LAYOUT_UNDEFINED?0:
		imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL?VK_ACCESS_SHADER_READ_BIT:0;*/
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = imageLayout;//VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);

	//transfer "stage"
	VkBufferImageCopy bufferImageCopy = {};
	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.mipLevel = 0;
	bufferImageCopy.imageSubresource.baseArrayLayer = 0;
	bufferImageCopy.imageSubresource.layerCount = 1;
	bufferImageCopy.imageExtent.width = w;
	bufferImageCopy.imageExtent.height = h;
	bufferImageCopy.imageExtent.depth = 1;
	bufferImageCopy.imageOffset = (VkOffset3D){0,0,0};
	bufferImageCopy.bufferOffset = 0;
	vkCmdCopyBufferToImage(*pcommandBuffer,stagingBuffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&bufferImageCopy);

	//create in transfer stage, use in fragment shader stage
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);
	
	imageLayout = imageMemoryBarrier.newLayout;
}

ShaderModule::ShaderModule(Blob *pblob, const CompositorInterface *_pcomp) : pcomp(_pcomp){
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(pblob->GetBufferPointer());
	shaderModuleCreateInfo.codeSize = pblob->GetBufferLength();

	if(vkCreateShaderModule(pcomp->logicalDev,&shaderModuleCreateInfo,0,&shaderModule) != VK_SUCCESS)
		throw Exception("Failed to create a shader module.");

	SpvReflectShaderModule reflectShaderModule;
	if(spvReflectCreateShaderModule(shaderModuleCreateInfo.codeSize,shaderModuleCreateInfo.pCode,&reflectShaderModule) != SPV_REFLECT_RESULT_SUCCESS)
		throw Exception("Failed to reflect shader module.");
	
	uint setCount;
	if(spvReflectEnumerateDescriptorSets(&reflectShaderModule,&setCount,0) != SPV_REFLECT_RESULT_SUCCESS)
		throw Exception("Failed to enumerate descriptor sets.");
	
	SpvReflectDescriptorSet **preflectDescSets = new SpvReflectDescriptorSet*[setCount];
	spvReflectEnumerateDescriptorSets(&reflectShaderModule,&setCount,preflectDescSets);

	pdescSetLayouts = new VkDescriptorSetLayout[setCount];
	for(uint i = 0; i < setCount; ++i){
		VkDescriptorSetLayoutBinding *pbindings = new VkDescriptorSetLayoutBinding[preflectDescSets[i]->binding_count];
		for(uint j = 0; j < preflectDescSets[i]->binding_count; ++j){
			pbindings[j] = (VkDescriptorSetLayoutBinding){};
			pbindings[j].binding = preflectDescSets[i]->bindings[j]->binding;
			pbindings[j].descriptorType = (VkDescriptorType)preflectDescSets[i]->bindings[j]->descriptor_type;
			pbindings[j].descriptorCount = 1;
			for(uint k = 0; k < preflectDescSets[i]->bindings[j]->array.dims_count; ++k)
				pbindings[j].descriptorCount *= preflectDescSets[i]->bindings[j]->array.dims[k];
			pbindings[j].stageFlags = (VkShaderStageFlagBits)reflectShaderModule.shader_stage;
			pbindings[j].pImmutableSamplers = &pcomp->pointSampler;
		}

		VkDescriptorSetLayoutCreateInfo descSetLayoutCreateInfo = {};
		descSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descSetLayoutCreateInfo.bindingCount = preflectDescSets[i]->binding_count;
		descSetLayoutCreateInfo.pBindings = pbindings;
		if(vkCreateDescriptorSetLayout(pcomp->logicalDev,&descSetLayoutCreateInfo,0,&pdescSetLayouts[i]) != VK_SUCCESS)
			throw Exception("Failed to create a descriptor set layout.");

		delete []pbindings;
	}

	delete []preflectDescSets;
	spvReflectDestroyShaderModule(&reflectShaderModule);
}

ShaderModule::~ShaderModule(){
	//
	delete []pdescSetLayouts;
	vkDestroyShaderModule(pcomp->logicalDev,shaderModule,0);
}

const std::vector<std::pair<VkFormat, uint>> Texture::formatSizeMap = {
	{VK_FORMAT_R8G8B8A8_UNORM,4}
};

CompositorPipeline::CompositorPipeline(const CompositorInterface *_pcomp) : pcomp(_pcomp){
	//
}

CompositorPipeline::~CompositorPipeline(){
	vkDestroyShaderModule(pcomp->logicalDev,vertexShader,0);
	vkDestroyShaderModule(pcomp->logicalDev,geometryShader,0);
	vkDestroyShaderModule(pcomp->logicalDev,fragmentShader,0);
	vkDestroyPipeline(pcomp->logicalDev,pipeline,0);
	vkDestroyPipelineLayout(pcomp->logicalDev,pipelineLayout,0);
}

CompositorPipeline * CompositorPipeline::CreateDefault(const CompositorInterface *pcomp){
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;
	//
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = 0;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	//inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[3];

	VkShaderModule vertexShader = pcomp->CreateShaderModuleFromFile("/mnt/data/Asiakirjat/projects/super-xwm/build/frame_vertex.spv");
	shaderStageCreateInfo[0] = (VkPipelineShaderStageCreateInfo){};
	shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfo[0].module = vertexShader;
	shaderStageCreateInfo[0].pName = "main";

	VkShaderModule geometryShader = pcomp->CreateShaderModuleFromFile("/mnt/data/Asiakirjat/projects/super-xwm/build/frame_geometry.spv");
	shaderStageCreateInfo[1] = (VkPipelineShaderStageCreateInfo){};
	shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_GEOMETRY_BIT;
	shaderStageCreateInfo[1].module = geometryShader;
	shaderStageCreateInfo[1].pName = "main";

	VkShaderModule fragmentShader = pcomp->CreateShaderModuleFromFile("/mnt/data/Asiakirjat/projects/super-xwm/build/frame_fragment.spv");
	shaderStageCreateInfo[2] = (VkPipelineShaderStageCreateInfo){};
	shaderStageCreateInfo[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[2].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfo[2].module = fragmentShader;
	shaderStageCreateInfo[2].pName = "main";
	if(!vertexShader || !geometryShader || !fragmentShader)
		throw Exception("Unable to load required shaders.\n");

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)pcomp->imageExtent.width;
	viewport.height = (float)pcomp->imageExtent.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0,0};
	scissor.extent = pcomp->imageExtent;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.lineWidth = 1.0f;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	//rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	//rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; 
	//depth stencil

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	//...

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;
	colorBlendStateCreateInfo.blendConstants[0] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[1] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[2] = 0.0f;
	colorBlendStateCreateInfo.blendConstants[3] = 0.0f;

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = 32;

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = 0;
	layoutCreateInfo.pSetLayouts = 0;
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(pcomp->logicalDev,&layoutCreateInfo,0,&pipelineLayout) != VK_SUCCESS)
		return 0;

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = sizeof(shaderStageCreateInfo)/sizeof(shaderStageCreateInfo[0]);
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfo;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo; //!!
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = 0;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = 0;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = pcomp->renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = 0;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	if(vkCreateGraphicsPipelines(pcomp->logicalDev,0,1,&graphicsPipelineCreateInfo,0,&pipeline) != VK_SUCCESS)
		return 0;

	CompositorPipeline *pcompPipeline = new CompositorPipeline(pcomp);
	pcompPipeline->vertexShader = vertexShader;
	pcompPipeline->geometryShader = geometryShader;
	pcompPipeline->fragmentShader = fragmentShader;
	pcompPipeline->pipelineLayout = pipelineLayout;
	pcompPipeline->pipeline = pipeline;

	return pcompPipeline;
}

}

