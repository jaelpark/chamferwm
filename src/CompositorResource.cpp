#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <algorithm>
#include <gbm.h>
#include <unistd.h>

#include "spirv_reflect.h"

namespace Compositor{

TextureBase::TextureBase(uint _w, uint _h, VkFormat format, uint flags, const CompositorInterface *_pcomp) : w(_w), h(_h), imageLayout(VK_IMAGE_LAYOUT_UNDEFINED), pcomp(_pcomp){
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
	if(vkBindImageMemory(pcomp->logicalDev,image,deviceMemory,0) != VK_SUCCESS)
		throw Exception("Failed to bind image memory.");

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = format;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_B;//VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;//VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_R;//VK_COMPONENT_SWIZZLE_IDENTITY;
	imageViewCreateInfo.components.a = (flags & FLAG_IGNORE_ALPHA)?VK_COMPONENT_SWIZZLE_ONE:VK_COMPONENT_SWIZZLE_IDENTITY;
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

TextureStaged::TextureStaged(uint _w, uint _h, VkFormat _format, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,_format,_flags,_pcomp){
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
	if(vkBindBufferMemory(pcomp->logicalDev,stagingBuffer,stagingMemory,0) != VK_SUCCESS)
		throw Exception("Failed to bind staging buffer memory.");

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
	imageSubresourceRange.baseArrayLayer = 0;
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

TexturePixmap::TexturePixmap(uint _w, uint _h, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_flags,_pcomp), transferImageLayout(VK_IMAGE_LAYOUT_PREINITIALIZED){
	pcomp11 = dynamic_cast<const X11Compositor *>(pcomp);
}

TexturePixmap::~TexturePixmap(){
	//
}

//only pixmaps that correspond to the created texture in size should be attached
void TexturePixmap::Attach(xcb_pixmap_t pixmap){
	xcb_dri3_buffer_from_pixmap_cookie_t bufferFromPixmapCookie = xcb_dri3_buffer_from_pixmap(pcomp11->pbackend->pcon,pixmap);
	xcb_dri3_buffer_from_pixmap_reply_t *pbufferFromPixmapReply = xcb_dri3_buffer_from_pixmap_reply(pcomp11->pbackend->pcon,bufferFromPixmapCookie,0);

	dmafd = xcb_dri3_buffer_from_pixmap_reply_fds(pcomp11->pbackend->pcon,pbufferFromPixmapReply)[0]; //TODO: get all planes?

	struct gbm_import_fd_data gbmImportFdData = {
		.fd = dmafd,
		.width = w,
		.height = h,
		.stride = pbufferFromPixmapReply->stride,
		.format = GBM_FORMAT_ARGB8888
	};
	pgbmBufferObject = gbm_bo_import(pcomp11->pgbmdev,GBM_BO_IMPORT_FD,&gbmImportFdData,0);//GBM_BO_USE_LINEAR);
	if(!pgbmBufferObject) //TODO: import once for the first buffer, assume same modifiers and format for all?
		throw Exception("Failed to import GBM buffer object.");

	VkSubresourceLayout subresourceLayout = {};
	subresourceLayout.offset = 0;
	subresourceLayout.size = (uint)pbufferFromPixmapReply->size;//(uint)pbufferFromPixmapReply->stride*h;
	subresourceLayout.rowPitch = (uint)pbufferFromPixmapReply->stride;
	subresourceLayout.arrayPitch = subresourceLayout.size;
	subresourceLayout.depthPitch = subresourceLayout.size;

	uint64_t modifier = gbm_bo_get_modifier(pgbmBufferObject);
	sint planeCount = gbm_bo_get_plane_count(pgbmBufferObject);
	DebugPrintf(stdout,"Image modifier: %llu, plane count: %d\n",modifier,planeCount);

	//dmafd = gbm_bo_get_fd(pgbmBufferObject);
	
	VkImageDrmFormatModifierExplicitCreateInfoEXT imageDrmFormatModifierExpCreateInfo = {};
	imageDrmFormatModifierExpCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
	imageDrmFormatModifierExpCreateInfo.drmFormatModifier = modifier;//gbm_bo_get_modifier(pgbmBufferObject);
	imageDrmFormatModifierExpCreateInfo.drmFormatModifierPlaneCount = 1;
	imageDrmFormatModifierExpCreateInfo.pPlaneLayouts = &subresourceLayout;

	VkExternalMemoryImageCreateInfo externalMemoryCreateInfo = {};
	externalMemoryCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	externalMemoryCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
	externalMemoryCreateInfo.pNext = &imageDrmFormatModifierExpCreateInfo;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = w;
	imageCreateInfo.extent.height = h;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM; //todo: format from image reply?
	//imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;//VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT; //need the extension
	imageCreateInfo.initialLayout = transferImageLayout;//VK_IMAGE_LAYOUT_PREINITIALIZED;//VK_IMAGE_LAYOUT_UNDEFINED;
	//imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.flags = 0;
	imageCreateInfo.pNext = &externalMemoryCreateInfo;
	if(vkCreateImage(pcomp->logicalDev,&imageCreateInfo,0,&transferImage) != VK_SUCCESS)
		throw Exception("Failed to create an image.");
	
	VkMemoryFdPropertiesKHR memoryFdProps;
	memoryFdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
	if(((PFN_vkGetMemoryFdPropertiesKHR)vkGetInstanceProcAddr(pcomp11->instance,"vkGetMemoryFdPropertiesKHR"))(pcomp->logicalDev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,dmafd,&memoryFdProps) != VK_SUCCESS)
		throw Exception("Failed to get memory fd properties.");

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(pcomp->logicalDev,transferImage,&memoryRequirements);
	
	VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {};
	memoryDedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	memoryDedicatedAllocateInfo.image = transferImage;

	VkImportMemoryFdInfoKHR importMemoryFdInfo = {};
	importMemoryFdInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR;
	importMemoryFdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
	importMemoryFdInfo.fd = dmafd;
	importMemoryFdInfo.pNext = &memoryDedicatedAllocateInfo;

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.pNext = &importMemoryFdInfo;
	//for(memoryAllocateInfo.memoryTypeIndex = 0; memoryAllocateInfo.memoryTypeIndex < physicalDeviceMemoryProps.memoryTypeCount; memoryAllocateInfo.memoryTypeIndex++){
	for(memoryAllocateInfo.memoryTypeIndex = 0;; memoryAllocateInfo.memoryTypeIndex++){
		if(memoryFdProps.memoryTypeBits & (1<<memoryAllocateInfo.memoryTypeIndex))
			break;
	}
	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&transferMemory) != VK_SUCCESS)
		throw Exception("Failed to allocate transfer image memory."); //NOTE: may return invalid handle, if the buffer that we're trying to import is only shortly available (for example firefox animating it's url menu by resizing). Need xcb_dri3 fences to keep the handle alive most likely.
	if(vkBindImageMemory(pcomp->logicalDev,transferImage,transferMemory,0) != VK_SUCCESS)
		throw Exception("Failed to bind transfer image memory.");

	free(pbufferFromPixmapReply);
}

void TexturePixmap::Detach(){
	vkFreeMemory(pcomp->logicalDev,transferMemory,0);
	vkDestroyImage(pcomp->logicalDev,transferImage,0);

	gbm_bo_destroy(pgbmBufferObject);
	close(dmafd);
}

void TexturePixmap::Update(const VkCommandBuffer *pcommandBuffer, const VkRect2D *prects, uint rectCount){
	//
	VkImageSubresourceRange imageSubresourceRange = {};
	imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageSubresourceRange.baseMipLevel = 0;
	imageSubresourceRange.levelCount = 1;
	imageSubresourceRange.baseArrayLayer = 0;
	imageSubresourceRange.layerCount = 1;

	VkImageMemoryBarrier transferImageMemoryBarrier = {};
	transferImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	transferImageMemoryBarrier.image = transferImage;
	transferImageMemoryBarrier.subresourceRange = imageSubresourceRange;
	transferImageMemoryBarrier.srcAccessMask = 0;
	transferImageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	//transferImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	transferImageMemoryBarrier.oldLayout = transferImageLayout;
	transferImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	//vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
		//0,0,0,0,1,&transferImageMemoryBarrier);

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = imageSubresourceRange;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = imageLayout;//VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	//vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
		//0,0,0,0,1,&imageMemoryBarrier);
	const VkImageMemoryBarrier barriers[] = {imageMemoryBarrier,transferImageMemoryBarrier};
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
		0,0,0,0,2,barriers);
	
	VkImageSubresourceLayers imageSubresourceLayers = {};
	imageSubresourceLayers.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageSubresourceLayers.mipLevel = 0;
	imageSubresourceLayers.baseArrayLayer = 0;
	imageSubresourceLayers.layerCount = 1;

	imageCopyBuffer.reserve(rectCount);
	for(uint i = 0; i < rectCount; ++i){
		imageCopyBuffer[i].srcSubresource = imageSubresourceLayers;
		imageCopyBuffer[i].srcOffset = (VkOffset3D){prects[i].offset.x,prects[i].offset.y,0};
		imageCopyBuffer[i].dstSubresource = imageSubresourceLayers;
		imageCopyBuffer[i].dstOffset = imageCopyBuffer[i].srcOffset;
		imageCopyBuffer[i].extent = (VkExtent3D){prects[i].extent.width,prects[i].extent.height,1};
	}
	vkCmdCopyImage(*pcommandBuffer,transferImage,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,rectCount,imageCopyBuffer.data());

	//create in transfer stage, use in fragment shader stage
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);
	
	imageLayout = imageMemoryBarrier.newLayout;
	transferImageLayout = transferImageMemoryBarrier.newLayout;
}

TextureHostPointer::TextureHostPointer(uint _w, uint _h, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_flags,_pcomp){
	//
	discards.reserve(2);
}

TextureHostPointer::~TextureHostPointer(){
	discards.erase(std::remove_if(discards.begin(),discards.end(),[&](auto &t)->bool{
		vkFreeMemory(pcomp->logicalDev,std::get<1>(t),0);
		vkDestroyBuffer(pcomp->logicalDev,std::get<2>(t),0);
		return true;
	}));
}

bool TextureHostPointer::Attach(unsigned char *pchpixels){
	VkExternalMemoryBufferCreateInfo externalMemoryBufferCreateInfo = {};
	externalMemoryBufferCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO;
	externalMemoryBufferCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = formatSizeMap[formatIndex].second*w*h;
	bufferCreateInfo.size = (bufferCreateInfo.size-1)+pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment-(bufferCreateInfo.size-1)%pcomp->physicalDevExternalMemoryHostProps.minImportedHostPointerAlignment;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.pNext = &externalMemoryBufferCreateInfo;
	if(vkCreateBuffer(pcomp->logicalDev,&bufferCreateInfo,0,&transferBuffer) != VK_SUCCESS){
		//throw Exception("Failed to create a transfer buffer.");
		DebugPrintf(stderr,"Failed to create a transfer buffer.");
		return false;
	}

	VkMemoryHostPointerPropertiesEXT memoryHostPointerProps;
	memoryHostPointerProps.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
	if(((PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetInstanceProcAddr(pcomp->instance,"vkGetMemoryHostPointerPropertiesEXT"))(pcomp->logicalDev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,pchpixels,&memoryHostPointerProps) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to get memory host pointer properties.");
		vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
		return false;
	}
	
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProps;
	vkGetPhysicalDeviceMemoryProperties(pcomp->physicalDev,&physicalDeviceMemoryProps);

	VkMemoryRequirements memoryRequirements;
	vkGetBufferMemoryRequirements(pcomp->logicalDev,transferBuffer,&memoryRequirements);

	VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {};
	memoryDedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	memoryDedicatedAllocateInfo.buffer = transferBuffer;

	VkImportMemoryHostPointerInfoEXT importMemoryHostPointerInfo = {};
	importMemoryHostPointerInfo.sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT;
	importMemoryHostPointerInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
	importMemoryHostPointerInfo.pHostPointer = pchpixels;
	importMemoryHostPointerInfo.pNext = &memoryDedicatedAllocateInfo; //not necessary?

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	memoryAllocateInfo.pNext = &importMemoryHostPointerInfo;
	for(memoryAllocateInfo.memoryTypeIndex = 0;; memoryAllocateInfo.memoryTypeIndex++){
		//if(memoryHostPointerProps.memoryTypeBits & (1<<memoryAllocateInfo.memoryTypeIndex) && physicalDeviceMemoryProps.memoryTypes[memoryAllocateInfo.memoryTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
		if((memoryRequirements.memoryTypeBits & memoryHostPointerProps.memoryTypeBits) & (1<<memoryAllocateInfo.memoryTypeIndex) && physicalDeviceMemoryProps.memoryTypes[memoryAllocateInfo.memoryTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
			break;
	}
	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&transferMemory) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to allocate transfer buffer memory.");
		vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
		return false;
	}
	if(vkBindBufferMemory(pcomp->logicalDev,transferBuffer,transferMemory,0) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to bind staging buffer memory.");
		vkFreeMemory(pcomp->logicalDev,transferMemory,0);
		vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
		return false;
	}
	return true;
}

void TextureHostPointer::Detach(uint64 releaseTag){
	discards.erase(std::remove_if(discards.begin(),discards.end(),[&](auto &t)->bool{
		if(releaseTag < std::get<0>(t)+pcomp->swapChainImageCount+1)
			return false;
		vkFreeMemory(pcomp->logicalDev,std::get<1>(t),0);
		vkDestroyBuffer(pcomp->logicalDev,std::get<2>(t),0);
		return true;
	}));
	//discards.push_back(std::tuple<uint64, VkDeviceMemory, VkBuffer>(releaseTag,transferMemory,transferBuffer));
	vkDeviceWaitIdle(pcomp->logicalDev); //TODO: remove, and fix the buffer freeing
	vkFreeMemory(pcomp->logicalDev,transferMemory,0);
	vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
}

void TextureHostPointer::Update(const VkCommandBuffer *pcommandBuffer, const VkRect2D *prects, uint rectCount){
	//
	VkImageSubresourceRange imageSubresourceRange = {};
	imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageSubresourceRange.baseMipLevel = 0;
	imageSubresourceRange.levelCount = 1;
	imageSubresourceRange.baseArrayLayer = 0;
	imageSubresourceRange.layerCount = 1;

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
	vkCmdCopyBufferToImage(*pcommandBuffer,transferBuffer,image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,rectCount,bufferImageCopyBuffer.data());

	//create in transfer stage, use in fragment shader stage
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);
	
	imageLayout = imageMemoryBarrier.newLayout;
}

//Texture::Texture(uint _w, uint _h, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomp), TextureStaged(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomp), TexturePixmap(_w,_h,_pcomp){
Texture::Texture(uint _w, uint _h, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_flags,_pcomp), TextureStaged(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_flags,_pcomp), TextureHostPointer(_w,_h,_flags,_pcomp){
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
			b.pname = preflectDescSets[i]->bindings[j]->name?mstrdup(preflectDescSets[i]->bindings[j]->name):mstrdup("content"); //hack: name might be null in optimized shader builds
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

	VkDynamicState dynamicStates[1] = {VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = 0;
	if(pcomp->scissoring){
		dynamicStateCreateInfo.pDynamicStates = dynamicStates;
		dynamicStateCreateInfo.dynamicStateCount = 1;
	}else{
		dynamicStateCreateInfo.pDynamicStates = 0;
		dynamicStateCreateInfo.dynamicStateCount = 0;
	}

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

