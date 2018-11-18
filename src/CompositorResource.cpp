#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <algorithm>

#include "spirv_reflect.h"

#include "glad/gl.h"
#include "glad/glx.h"

namespace Compositor{

TextureBase::TextureBase(uint _w, uint _h, VkFormat format, const CompositorInterface *_pcomp) : w(_w), h(_h), imageLayout(VK_IMAGE_LAYOUT_UNDEFINED), pcomp(_pcomp){
	//
	auto m = std::find_if(formatSizeMap.begin(),formatSizeMap.end(),[&](auto &r)->bool{
		return r.first == format;
	});
	formatIndex = m-formatSizeMap.begin();

	//DebugPrintf(stdout,"*** creating texture: %u, (%ux%u)\n",(*m).second,w,h);

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
	
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProps;
	vkGetPhysicalDeviceMemoryProperties(pcomp->physicalDev,&physicalDeviceMemoryProps);

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(pcomp->logicalDev,image,&memoryRequirements);
	
	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
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
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_B;//VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;//VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_R;//VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if(vkCreateImageView(pcomp->logicalDev,&imageViewCreateInfo,0,&imageView) != VK_SUCCESS)
		throw Exception("Failed to create texture image view.");

}

TextureBase::~TextureBase(){
	vkDestroyImageView(pcomp->logicalDev,imageView,0);

	vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
	vkDestroyImage(pcomp->logicalDev,image,0);
}

const std::vector<std::pair<VkFormat, uint>> TextureBase::formatSizeMap = {
	{VK_FORMAT_R8G8B8A8_UNORM,4}
};

TextureStaged::TextureStaged(uint _w, uint _h, VkFormat _format, const CompositorInterface *_pcomp) : TextureBase(_w,_h,_format,_pcomp){
	//
	//staging buffer
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = formatSizeMap[formatIndex].second*w*h;//(*m).second*w*h;
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
}

TextureStaged::~TextureStaged(){
	vkFreeMemory(pcomp->logicalDev,stagingMemory,0);
	vkDestroyBuffer(pcomp->logicalDev,stagingBuffer,0);
}

const void * TextureStaged::Map() const{
	void *pdata;
	if(vkMapMemory(pcomp->logicalDev,stagingMemory,0,stagingMemorySize,0,&pdata) != VK_SUCCESS)
		return 0;
	return pdata;
}

void TextureStaged::Unmap(const VkCommandBuffer *pcommandBuffer, const VkRect2D *prects, uint rectCount){
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
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = imageLayout;//VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);

	//transfer "stage"
	bufferImageCopyBuffer.reserve(rectCount);
	for(uint i = 0; i < rectCount; ++i){
		bufferImageCopyBuffer[i].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferImageCopyBuffer[i].imageSubresource.mipLevel = 0;
		bufferImageCopyBuffer[i].imageSubresource.baseArrayLayer = 0;
		bufferImageCopyBuffer[i].imageSubresource.layerCount = 1;
		bufferImageCopyBuffer[i].imageExtent.width = prects[i].extent.width;//w/4; //w
		bufferImageCopyBuffer[i].imageExtent.height = prects[i].extent.height;//h
		bufferImageCopyBuffer[i].imageExtent.depth = 1;
		bufferImageCopyBuffer[i].imageOffset = (VkOffset3D){prects[i].offset.x,prects[i].offset.y,0};//(VkOffset3D){w/4,0,0}; //x,y
		bufferImageCopyBuffer[i].bufferOffset = (w*prects[i].offset.y+prects[i].offset.x)*formatSizeMap[formatIndex].second;//w/4*4; //(w*y+x)*format
		bufferImageCopyBuffer[i].bufferRowLength = w;
		bufferImageCopyBuffer[i].bufferImageHeight = h;
	}
	vkCmdCopyBufferToImage(*pcommandBuffer,stagingBuffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,rectCount,bufferImageCopyBuffer.data());

	//create in transfer stage, use in fragment shader stage
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);
	
	imageLayout = imageMemoryBarrier.newLayout;
}

TexturePixmap::TexturePixmap(uint _w, uint _h, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomp){
	pcomp11 = dynamic_cast<const X11Compositor *>(pcomp);
	if(!pcomp11 || !pcomp11->pbackend->pdisplay) //hack, solve this!
		return;
	//
	VkMemoryGetFdInfoKHR memoryfdInfo = {};
	memoryfdInfo.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
	memoryfdInfo.pNext = 0;
	memoryfdInfo.memory = deviceMemory;
	memoryfdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
	
	sint memoryfd;
	if(pcomp->vkGetMemoryFdKHR(pcomp->logicalDev,&memoryfdInfo,&memoryfd) != VK_SUCCESS)
		throw Exception("Failed to get memory file descriptor.");

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(pcomp->logicalDev,image,&memoryRequirements);

	glCreateMemoryObjectsEXT(1,&memoryObject);
	glImportMemoryFdEXT(memoryObject,memoryRequirements.size,GL_HANDLE_TYPE_OPAQUE_FD_EXT,memoryfd);

	glCreateTextures(GL_TEXTURE_2D,1,&sharedTexture);
	glTextureStorageMem2DEXT(sharedTexture,1,GL_RGBA8,w,h,memoryObject,0);

	glGenTextures(1,&pixmapTexture);
}

TexturePixmap::~TexturePixmap(){
	if(!pcomp11 || !pcomp11->pbackend->pdisplay)
		return;
	glDeleteTextures(1,&pixmapTexture);

	glDeleteTextures(1,&sharedTexture);
}

//only pixmaps that correspond to the created texture in size should be attached
void TexturePixmap::Attach(xcb_pixmap_t pixmap){
	if(!pcomp11 || !pcomp11->pbackend->pdisplay)
		return;
	static const sint pixmapAttribs[] = {
		GLX_TEXTURE_TARGET_EXT,GLX_TEXTURE_2D_EXT,
		GLX_TEXTURE_FORMAT_EXT,GLX_TEXTURE_FORMAT_RGBA_EXT,
		None};
	glxpixmap = glXCreatePixmap(pcomp11->pbackend->pdisplay,pcomp11->pfbconfig[0],pixmap,pixmapAttribs);
}

void TexturePixmap::Detach(){
	if(!pcomp11 || !pcomp11->pbackend->pdisplay)
		return;
	glXDestroyPixmap(pcomp11->pbackend->pdisplay,glxpixmap);
}

void TexturePixmap::Update(const VkCommandBuffer *pcommandBuffer){
	if(!pcomp11 || !pcomp11->pbackend->pdisplay)
		return;
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
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = imageLayout;//VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);

	glBindTexture(GL_TEXTURE_2D,pixmapTexture);

	glXBindTexImageEXT(pcomp11->pbackend->pdisplay,glxpixmap,GLX_FRONT_LEFT_EXT,0);
	glCopyImageSubData(pixmapTexture,GL_TEXTURE_2D,0,0,0,0,
		sharedTexture,GL_TEXTURE_2D,0,0,0,0,w,h,1); //TODO: damaged regions
	glXReleaseTexImageEXT(pcomp11->pbackend->pdisplay,glxpixmap,GLX_FRONT_LEFT_EXT);

	//semaphore mandatory
}

Texture::Texture(uint _w, uint _h, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomp), TextureStaged(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomp), TexturePixmap(_w,_h,_pcomp){
	//
}

Texture::~Texture(){
	//
}

ShaderModule::ShaderModule(const char *_pname, const Blob *pblob, const CompositorInterface *_pcomp) : pcomp(_pcomp), pname(mstrdup(_pname)){
	VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
	shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	shaderModuleCreateInfo.pCode = reinterpret_cast<const uint32_t *>(pblob->GetBufferPointer());
	shaderModuleCreateInfo.codeSize = pblob->GetBufferLength();

	if(vkCreateShaderModule(pcomp->logicalDev,&shaderModuleCreateInfo,0,&shaderModule) != VK_SUCCESS)
		throw Exception("Failed to create a shader module.");

	SpvReflectShaderModule reflectShaderModule;
	if(spvReflectCreateShaderModule(shaderModuleCreateInfo.codeSize,shaderModuleCreateInfo.pCode,&reflectShaderModule) != SPV_REFLECT_RESULT_SUCCESS)
		throw Exception("Failed to reflect shader module.");
	
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
			pbindings[j].descriptorCount = 1; //one binding can have multiple descriptors (combined image samplers)
			for(uint k = 0; k < preflectDescSets[i]->bindings[j]->array.dims_count; ++k)
				pbindings[j].descriptorCount *= preflectDescSets[i]->bindings[j]->array.dims[k];
			pbindings[j].stageFlags = (VkShaderStageFlagBits)reflectShaderModule.shader_stage;
			pbindings[j].pImmutableSamplers = &pcomp->pointSampler;

			Binding &b = bindings.emplace_back();
			b.pname = mstrdup(preflectDescSets[i]->bindings[j]->name);
			b.type = pbindings[j].descriptorType;
			b.setIndex = i;
			b.binding = pbindings[j].binding;
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
	mstrfree(pname);
	for(Binding &b : bindings)
		mstrfree(b.pname);
	for(uint i = 0; i < setCount; ++i)
		vkDestroyDescriptorSetLayout(pcomp->logicalDev,pdescSetLayouts[i],0);
	delete []pdescSetLayouts;
	vkDestroyShaderModule(pcomp->logicalDev,shaderModule,0);
}

Pipeline::Pipeline(ShaderModule *_pvertexShader, ShaderModule *_pgeometryShader, ShaderModule *_pfragmentShader, const CompositorInterface *_pcomp) : pshaderModule{_pvertexShader,_pgeometryShader,_pfragmentShader}, pcomp(_pcomp){
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

	for(uint i = 0, stageBit[] = {VK_SHADER_STAGE_VERTEX_BIT,VK_SHADER_STAGE_GEOMETRY_BIT,VK_SHADER_STAGE_FRAGMENT_BIT}; i < SHADER_MODULE_COUNT; ++i){
		shaderStageCreateInfo[i] = (VkPipelineShaderStageCreateInfo){};
		shaderStageCreateInfo[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo[i].stage = (VkShaderStageFlagBits)stageBit[i];
		shaderStageCreateInfo[i].module = pshaderModule[i]->shaderModule;
		shaderStageCreateInfo[i].pName = "main";
	}

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
	//viewportStateCreateInfo.pScissors = 0;//&scissor; //+ dynamic states below!

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
	//colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	//premultiplied alpha blending
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
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
	pushConstantRange.size = 40;

	uint setCount = 0;
	for(uint i = 0; i < SHADER_MODULE_COUNT; setCount += pshaderModule[i]->setCount, ++i);
	VkDescriptorSetLayout *pcombinedSets = new VkDescriptorSetLayout[setCount];

	for(uint i = 0, descPointer = 0; i < SHADER_MODULE_COUNT; ++i){
		std::copy(pshaderModule[i]->pdescSetLayouts,pshaderModule[i]->pdescSetLayouts+pshaderModule[i]->setCount,pcombinedSets+descPointer);
		descPointer += pshaderModule[i]->setCount;
	}

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = setCount;
	layoutCreateInfo.pSetLayouts = pcombinedSets;
	layoutCreateInfo.pushConstantRangeCount = 1;
	layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(pcomp->logicalDev,&layoutCreateInfo,0,&pipelineLayout) != VK_SUCCESS)
		throw Exception("Failed to crate a pipeline layout.");
	
	delete []pcombinedSets;

	//VkDynamicState dynamicStates[1] = {VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = 0;
	dynamicStateCreateInfo.pDynamicStates = 0;//dynamicStates;
	dynamicStateCreateInfo.dynamicStateCount = 0;

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
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = pcomp->renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = 0;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	if(vkCreateGraphicsPipelines(pcomp->logicalDev,0,1,&graphicsPipelineCreateInfo,0,&pipeline) != VK_SUCCESS)
		throw Exception("Failed to create a graphics pipeline.");
}

Pipeline::~Pipeline(){
	vkDestroyPipeline(pcomp->logicalDev,pipeline,0);
	vkDestroyPipelineLayout(pcomp->logicalDev,pipelineLayout,0);
}

}

