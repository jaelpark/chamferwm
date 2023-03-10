#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"

#include <algorithm>
#include <unistd.h>
#include <libdrm/drm_fourcc.h>
//#include <fcntl.h>

#include "spirv_reflect.h"

namespace Compositor{

TextureBase::TextureBase(uint _w, uint _h, VkFormat format, const VkComponentMapping *pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : w(_w), h(_h), flags(_flags), imageLayout(VK_IMAGE_LAYOUT_UNDEFINED), image(0), imageView(0), deviceMemory(0), pcomp(_pcomp){
	//
	auto m = std::find_if(formatSizeMap.begin(),formatSizeMap.end(),[&](auto &r)->bool{
		return r.first == format;
	});
	formatIndex = m-formatSizeMap.begin();

	//DebugPrintf(stdout,"*** creating texture: %u, (%ux%u)\n",(*m).second,w,h);
	componentMappingHash = GetComponentMappingHash(pcomponentMapping);

	if(flags & TEXTURE_BASE_FLAG_SKIP)
		return; //only one image is needed by the derived class

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
	imageViewCreateInfo.components = *pcomponentMapping;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if(vkCreateImageView(pcomp->logicalDev,&imageViewCreateInfo,0,&imageView) != VK_SUCCESS)
		throw Exception("Failed to create texture image view.");

}

TextureBase::~TextureBase(){
	if(flags & TEXTURE_BASE_FLAG_SKIP)
		return;

	vkDestroyImageView(pcomp->logicalDev,imageView,0);

	vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
	vkDestroyImage(pcomp->logicalDev,image,0);
}

const std::vector<std::pair<VkFormat, uint>> TextureBase::formatSizeMap = {
	{VK_FORMAT_R8_UNORM,1},
	{VK_FORMAT_R8G8B8A8_UNORM,4}
};

const VkComponentMapping TextureBase::defaultComponentMapping = {
	VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY,
	VK_COMPONENT_SWIZZLE_IDENTITY
};

TextureStaged::TextureStaged(uint _w, uint _h, VkFormat _format, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,_format,_pcomponentMapping,_flags,_pcomp){
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
	imageMemoryBarrier.srcAccessMask = 0u;
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

TexturePixmap::TexturePixmap(uint _w, uint _h, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags|TEXTURE_BASE_FLAG_SKIP,_pcomp), pcomponentMapping(_pcomponentMapping){
	pcomp11 = dynamic_cast<const X11Compositor *>(pcomp);
}

TexturePixmap::~TexturePixmap(){
	//
}

//only pixmaps that correspond to the created texture in size should be attached
bool TexturePixmap::Attach(xcb_pixmap_t pixmap){
	xcb_dri3_buffers_from_pixmap_cookie_t buffersFromPixmapCookie = xcb_dri3_buffers_from_pixmap(pcomp11->pbackend->pcon,pixmap);
	xcb_dri3_buffers_from_pixmap_reply_t *pbuffersFromPixmapReply = xcb_dri3_buffers_from_pixmap_reply(pcomp11->pbackend->pcon,buffersFromPixmapCookie,0);
	if(!pbuffersFromPixmapReply){
		DebugPrintf(stderr,"Failed to get buffers from pixmap (DRI3 ext).\n");
		return false;
	}

	int *pdmafds = xcb_dri3_buffers_from_pixmap_buffers(pbuffersFromPixmapReply);
	if(!pdmafds){
		DebugPrintf(stderr,"NULL DMA-buf fd.\n");
		free(pbuffersFromPixmapReply);
		return false;
	}
	//dmafd = pdmafds[0];

	uint *pstrides = xcb_dri3_buffers_from_pixmap_strides(pbuffersFromPixmapReply);
	uint *poffsets = xcb_dri3_buffers_from_pixmap_offsets(pbuffersFromPixmapReply); //---

	uint64 modifier = pbuffersFromPixmapReply->modifier;
	//-------------------------------------

	//AMD: 0x1002, nvidia: 0x10de
	if(pcomp->physicalDevProps.vendorID == 0x8086){ //trust the query only on intel for now
		if(std::find_if(pcomp->drmFormatModifiers.begin(),pcomp->drmFormatModifiers.end(),[&](auto &p)->bool{
			return modifier == p.first;
		}) != pcomp->drmFormatModifiers.end()){
			printf("DRM format modifier %llu found during query.\n",modifier);
		}else{
			VkPhysicalDeviceImageDrmFormatModifierInfoEXT drmInfo = {};
			drmInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_DRM_FORMAT_MODIFIER_INFO_EXT;
			drmInfo.drmFormatModifier = modifier;
			drmInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			VkPhysicalDeviceExternalImageFormatInfo externalImageFormatInfo = {};
			externalImageFormatInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_IMAGE_FORMAT_INFO;
			externalImageFormatInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;
			externalImageFormatInfo.pNext = &drmInfo;

			VkPhysicalDeviceImageFormatInfo2 imageFormatInfo2 = {};
			imageFormatInfo2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2;
			imageFormatInfo2.pNext = &externalImageFormatInfo;
			imageFormatInfo2.format = VK_FORMAT_R8G8B8A8_UNORM;
			imageFormatInfo2.type = VK_IMAGE_TYPE_2D;
			imageFormatInfo2.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
			imageFormatInfo2.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			imageFormatInfo2.flags = 0;

			VkExternalImageFormatProperties externalImageFormatProps = {};
			externalImageFormatProps.sType = VK_STRUCTURE_TYPE_EXTERNAL_IMAGE_FORMAT_PROPERTIES;

			VkImageFormatProperties2 imageFormatProps2 = {};
			imageFormatProps2.sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2;
			imageFormatProps2.pNext = &externalImageFormatProps;
			if(vkGetPhysicalDeviceImageFormatProperties2(pcomp->physicalDev,&imageFormatInfo2,&imageFormatProps2) == VK_ERROR_FORMAT_NOT_SUPPORTED){
				DebugPrintf(stdout,"vkGetPhysicalDeviceImageFormatProperties2 failed for modifier %llu.\n",modifier);
				//try specify the format modifier explicitly
				for(auto &p : pcomp->drmFormatModifiers){
					printf("  Try: %llu, %u (accept %u planes)\n",p.first,p.second,pbuffersFromPixmapReply->nfd);
					if(p.first == 0 || p.second != pbuffersFromPixmapReply->nfd)
						continue;
					drmInfo.drmFormatModifier = p.first;//I915_FORMAT_MOD_X_TILED;
					if(vkGetPhysicalDeviceImageFormatProperties2(pcomp->physicalDev,&imageFormatInfo2,&imageFormatProps2) == VK_SUCCESS){
						modifier = p.first;
						break;
					}
				}
				if(modifier == pbuffersFromPixmapReply->modifier)
					DebugPrintf(stderr,"Unable to find compatible DRM modifier: pixmap: %llu. Check format query list output.\n");
			}
		}
	}

	VkSubresourceLayout subresourceLayout[256];
	for(uint i = 0; i < pbuffersFromPixmapReply->nfd; ++i){
		subresourceLayout[i].offset = poffsets[i];
		subresourceLayout[i].size = 0;
		subresourceLayout[i].rowPitch = pstrides[i];
		subresourceLayout[i].arrayPitch = 0;
		subresourceLayout[i].depthPitch = 0;
	}

	VkImageDrmFormatModifierExplicitCreateInfoEXT imageDrmFormatModifierExpCreateInfo = {};
	imageDrmFormatModifierExpCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT;
	imageDrmFormatModifierExpCreateInfo.drmFormatModifier = modifier;
	imageDrmFormatModifierExpCreateInfo.drmFormatModifierPlaneCount = pbuffersFromPixmapReply->nfd;
	imageDrmFormatModifierExpCreateInfo.pPlaneLayouts = subresourceLayout;

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
	imageCreateInfo.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT; //need the extension
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;//transferImageLayout;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;//VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.flags = 0;
	imageCreateInfo.pNext = &externalMemoryCreateInfo;
	if(vkCreateImage(pcomp->logicalDev,&imageCreateInfo,0,&image) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to create an image.\n");
		free(pbuffersFromPixmapReply);
		return false;
	}
	
	VkMemoryFdPropertiesKHR memoryFdProps = {};
	memoryFdProps.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR;
	if(((PFN_vkGetMemoryFdPropertiesKHR)vkGetInstanceProcAddr(pcomp11->instance,"vkGetMemoryFdPropertiesKHR"))(pcomp->logicalDev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,pdmafds[0],&memoryFdProps) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to get memory fd properties.\n");
		free(pbuffersFromPixmapReply);
		return false;
	}

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(pcomp->logicalDev,image,&memoryRequirements);

	/*if((dmafd = fcntl(pdmafds[0],F_DUPFD_CLOEXEC,0)) < 0){
		DebugPrintf(stderr,"Failed to duplicate DMA FD %d. fcntl error %d.\n",pdmafds[0],dmafd);
		free(pbuffersFromPixmapReply);
		return false;
	}*/
	dmafd = pdmafds[0];
	
	VkMemoryDedicatedAllocateInfo memoryDedicatedAllocateInfo = {};
	memoryDedicatedAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO;
	memoryDedicatedAllocateInfo.image = image;

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
	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&deviceMemory) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to allocate transfer image memory.\n");
		//close(dmafd);
		free(pbuffersFromPixmapReply);
		return false;
	}
	if(vkBindImageMemory(pcomp->logicalDev,image,deviceMemory,0) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to bind transfer image memory.\n");
		vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
		free(pbuffersFromPixmapReply);
		return false;
	}

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageViewCreateInfo.components = *pcomponentMapping;
	/*imageViewCreateInfo.components = pbuffersFromPixmapReply->bpp == 24
		?pixmapComponentMapping24
		:pixmapComponentMapping;*/
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if(vkCreateImageView(pcomp->logicalDev,&imageViewCreateInfo,0,&imageView) != VK_SUCCESS){
		vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
		free(pbuffersFromPixmapReply);
		DebugPrintf(stderr,"Failed to create texture image view.\n");
		return false;
	}
	
	free(pbuffersFromPixmapReply);

	imageLayout = imageCreateInfo.initialLayout;

	return true;
}

void TexturePixmap::Detach(){
	vkDeviceWaitIdle(pcomp->logicalDev); //TODO: remove, and fix the buffer freeing
	if(deviceMemory){ //check if attached
		vkDestroyImageView(pcomp->logicalDev,imageView,0);

		vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
		vkDestroyImage(pcomp->logicalDev,image,0);

		imageView = 0;
		deviceMemory = 0;
		image = 0;
	}

	//close(dmafd); //closed by vkFreeMemory
}

void TexturePixmap::Update(const VkCommandBuffer *pcommandBuffer, const VkRect2D *prects, uint rectCount){
	if(imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		return;

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
	imageMemoryBarrier.srcAccessMask = 0u;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);

	imageLayout = imageMemoryBarrier.newLayout;
}

const VkComponentMapping TexturePixmap::pixmapComponentMapping = {
	VK_COMPONENT_SWIZZLE_B,
	VK_COMPONENT_SWIZZLE_G,
	VK_COMPONENT_SWIZZLE_R,
	VK_COMPONENT_SWIZZLE_IDENTITY
};

const VkComponentMapping TexturePixmap::pixmapComponentMapping24 = {
	VK_COMPONENT_SWIZZLE_B,
	VK_COMPONENT_SWIZZLE_G,
	VK_COMPONENT_SWIZZLE_R,
	VK_COMPONENT_SWIZZLE_ONE
};

TextureHostPointer::TextureHostPointer(uint _w, uint _h, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags|TEXTURE_BASE_FLAG_SKIP,_pcomp), transferBuffer(0), pcomponentMapping(_pcomponentMapping){
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
		DebugPrintf(stderr,"Failed to create a transfer buffer.\n");
		return false;
	}

	VkMemoryHostPointerPropertiesEXT memoryHostPointerProps = {};
	memoryHostPointerProps.sType = VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT;
	if(((PFN_vkGetMemoryHostPointerPropertiesEXT)vkGetInstanceProcAddr(pcomp->instance,"vkGetMemoryHostPointerPropertiesEXT"))(pcomp->logicalDev,VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT,pchpixels,&memoryHostPointerProps) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to get memory host pointer properties.\n");
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
	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&deviceMemory) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to allocate transfer buffer memory.\n");
		vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
		return false;
	}
	if(vkBindBufferMemory(pcomp->logicalDev,transferBuffer,deviceMemory,0) != VK_SUCCESS){
		DebugPrintf(stderr,"Failed to bind transfer buffer memory.\n");
		vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
		vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
		return false;
	}

	VkExternalMemoryImageCreateInfo externalMemoryCreateInfo = {};
	externalMemoryCreateInfo.sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO;
	externalMemoryCreateInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.extent.width = w;
	imageCreateInfo.extent.height = h;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageCreateInfo.tiling = VK_IMAGE_TILING_LINEAR;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.flags = 0;
	imageCreateInfo.pNext = &externalMemoryCreateInfo;
	if(vkCreateImage(pcomp->logicalDev,&imageCreateInfo,0,&image) != VK_SUCCESS)
		throw Exception("Failed to create an image.");

	if(vkBindImageMemory(pcomp->logicalDev,image,deviceMemory,0) != VK_SUCCESS)
		throw Exception("Failed to bind image memory.");

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.image = image;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	imageViewCreateInfo.components = *pcomponentMapping;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	if(vkCreateImageView(pcomp->logicalDev,&imageViewCreateInfo,0,&imageView) != VK_SUCCESS)
		throw Exception("Failed to create texture image view.");

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
	if(transferBuffer){
		vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
		vkDestroyBuffer(pcomp->logicalDev,transferBuffer,0);
		vkDestroyImage(pcomp->logicalDev,image,0);

		imageView = 0;
		deviceMemory = 0;
		transferBuffer = 0;
		image = 0;
	}
}

void TextureHostPointer::Update(const VkCommandBuffer *pcommandBuffer, const VkRect2D *prects, uint rectCount){
	if(imageLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		return;

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
	imageMemoryBarrier.srcAccessMask = 0u;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	//vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
		0,0,0,0,1,&imageMemoryBarrier);

	imageLayout = imageMemoryBarrier.newLayout;
}

TextureDMABuffer::TextureDMABuffer(uint _w, uint _h, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags|TEXTURE_BASE_FLAG_SKIP,_pcomp), TexturePixmap(_w,_h,_pcomponentMapping,_flags,_pcomp){
	//
}

TextureDMABuffer::~TextureDMABuffer(){
	//
}

TextureSharedMemory::TextureSharedMemory(uint _w, uint _h, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags|TEXTURE_BASE_FLAG_SKIP,_pcomp), TextureHostPointer(_w,_h,_pcomponentMapping,_flags,_pcomp){
	//
}

TextureSharedMemory::~TextureSharedMemory(){
	//
}

TextureSharedMemoryStaged::TextureSharedMemoryStaged(uint _w, uint _h, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags,_pcomp), TextureStaged(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags,_pcomp){
	//
}

TextureSharedMemoryStaged::~TextureSharedMemoryStaged(){
	//
}

TextureCompatible::TextureCompatible(uint _w, uint _h, const VkComponentMapping *_pcomponentMapping, uint _flags, const CompositorInterface *_pcomp) : TextureBase(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags,_pcomp), TextureStaged(_w,_h,VK_FORMAT_R8G8B8A8_UNORM,_pcomponentMapping,_flags,_pcomp){
	//
}

TextureCompatible::~TextureCompatible(){
	//
}

Buffer::Buffer(uint _size, VkBufferUsageFlagBits usage, const CompositorInterface *_pcomp) : pcomp(_pcomp), size(_size){
	//
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
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
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = size;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT|usage;//VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if(vkCreateBuffer(pcomp->logicalDev,&bufferCreateInfo,0,&buffer) != VK_SUCCESS)
		throw Exception("Failed to create a buffer.");

	vkGetBufferMemoryRequirements(pcomp->logicalDev,buffer,&memoryRequirements);
	
	memoryAllocateInfo.allocationSize = memoryRequirements.size;
	for(memoryAllocateInfo.memoryTypeIndex = 0; memoryAllocateInfo.memoryTypeIndex < physicalDeviceMemoryProps.memoryTypeCount; memoryAllocateInfo.memoryTypeIndex++){
		if(memoryRequirements.memoryTypeBits & (1<<memoryAllocateInfo.memoryTypeIndex) && physicalDeviceMemoryProps.memoryTypes[memoryAllocateInfo.memoryTypeIndex].propertyFlags & (VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
			break;
	}

	if(vkAllocateMemory(pcomp->logicalDev,&memoryAllocateInfo,0,&deviceMemory) != VK_SUCCESS)
		throw Exception("Failed to allocate buffer memory.");
	if(vkBindBufferMemory(pcomp->logicalDev,buffer,deviceMemory,0) != VK_SUCCESS)
		throw Exception("Failed to bind buffer memory.");
}

Buffer::~Buffer(){
	vkFreeMemory(pcomp->logicalDev,deviceMemory,0);
	vkDestroyBuffer(pcomp->logicalDev,buffer,0);

	vkFreeMemory(pcomp->logicalDev,stagingMemory,0);
	vkDestroyBuffer(pcomp->logicalDev,stagingBuffer,0);
}

const void * Buffer::Map() const{
	void *pdata;
	if(vkMapMemory(pcomp->logicalDev,stagingMemory,0,stagingMemorySize,0,&pdata) != VK_SUCCESS)
		return 0;
	return pdata;
}

void Buffer::Unmap(const VkCommandBuffer *pcommandBuffer){
	vkUnmapMemory(pcomp->logicalDev,stagingMemory);
	
	VkBufferMemoryBarrier bufferMemoryBarrier = {};
	bufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bufferMemoryBarrier.buffer = buffer;
	bufferMemoryBarrier.offset = 0;
	bufferMemoryBarrier.size = size;//VK_WHOLE_SIZE;
	/*bufferMemoryBarrier.srcAccessMask = 0;
	bufferMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,
		0,0,1,&bufferMemoryBarrier,0,0);*/
	//not needed
	//https://www.khronos.org/registry/vulkan/specs/1.0/html/vkspec.html#synchronization-submission-host-writes
	
	VkBufferCopy bufferCopy = {};
	bufferCopy.srcOffset = 0;
	bufferCopy.dstOffset = 0;
	bufferCopy.size = size;
	vkCmdCopyBuffer(*pcommandBuffer,stagingBuffer,buffer,1,&bufferCopy);

	bufferMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bufferMemoryBarrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;//VK_ACCESS_SHADER_READ_BIT;
	//vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,0,
	vkCmdPipelineBarrier(*pcommandBuffer,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,0,
		0,0,1,&bufferMemoryBarrier,0,0);
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
	
	uint inputCount;
	if(spvReflectEnumerateInputVariables(&reflectShaderModule,&inputCount,0) != SPV_REFLECT_RESULT_SUCCESS)
		throw Exception("Failed to enumerate input variables.");
	
	//input variables
	SpvReflectInterfaceVariable **preflectInputVars = new SpvReflectInterfaceVariable*[inputCount];
	spvReflectEnumerateInputVariables(&reflectShaderModule,&inputCount,preflectInputVars);

	for(uint i = 0; i < inputCount; ++i){
		if(!preflectInputVars[i]->semantic){
			if(preflectInputVars[i]->location != ~0u)
				DebugPrintf(stdout,"warning: unsemantic vertex attribute in %s at location %u.\n",_pname,preflectInputVars[i]->location);
			continue;
		}
		auto m = std::find_if(semanticMap.begin(),semanticMap.end(),[&](auto &r)->bool{
			return strcasecmp(std::get<0>(r),preflectInputVars[i]->semantic) == 0
				&& std::get<1>(r) == (VkFormat)preflectInputVars[i]->format;
		});
		if(m == semanticMap.end()){
			if((VkShaderStageFlagBits)reflectShaderModule.shader_stage == VK_SHADER_STAGE_VERTEX_BIT && preflectInputVars[i]->location != ~0u)
				DebugPrintf(stdout,"warning: unrecognized semantic %s in %s at location %u.\n",preflectInputVars[i]->semantic,_pname,preflectInputVars[i]->location);
			continue;
		}
		Input &in = inputs.emplace_back();
		in.location = preflectInputVars[i]->location;
		in.binding = 0; //one vertex buffer bound, per vertex
		in.semanticMapIndex = m-semanticMap.begin();
	}
	/*std::sort(inputs.begin(),inputs.end(),[](const Input &a, const Input &b){
		return a.location < b.location;
	});
	inputStride = 0;
	for(auto &m : inputs){
		m.offset = inputStride;
		inputStride += std::get<2>(semanticMap[m.semanticMapIndex]);
	}*/

	delete []preflectInputVars;

	//push constants
	if(spvReflectEnumeratePushConstantBlocks(&reflectShaderModule,&pushConstantBlockCount,0) != SPV_REFLECT_RESULT_SUCCESS)
		throw Exception("Failed to enumerate push constant blocks.");
	
	SpvReflectBlockVariable **preflectBlockVars = new SpvReflectBlockVariable*[pushConstantBlockCount];
	spvReflectEnumeratePushConstantBlocks(&reflectShaderModule,&pushConstantBlockCount,preflectBlockVars);

	pPushConstantRanges = new VkPushConstantRange[pushConstantBlockCount];
	for(uint i = 0; i < pushConstantBlockCount; ++i){
		pPushConstantRanges[i].stageFlags = (VkShaderStageFlagBits)reflectShaderModule.shader_stage;
		pPushConstantRanges[i].offset = preflectBlockVars[i]->offset;
		pPushConstantRanges[i].size = preflectBlockVars[i]->size;

		for(uint j = 0; j < preflectBlockVars[i]->member_count; ++j){
			auto m = std::find_if(variableMap.begin(),variableMap.end(),[&](auto &r)->bool{
				return strcmp(std::get<0>(r),preflectBlockVars[i]->members[j].name) == 0;
			});
			if(m == variableMap.end()){
				DebugPrintf(stdout,"warning: unrecognized variable %s in %s with offset %u.\n",preflectBlockVars[i]->members[j].name,_pname,preflectBlockVars[i]->members[j].offset);
				continue;
			}
			//printf("---%u: %s\n",j,preflectBlockVars[i]->members[j].name);
			Variable &var = variables.emplace_back();
			var.offset = preflectBlockVars[i]->members[j].offset;
			var.variableMapIndex = m-variableMap.begin();
		}
	}
	//alt: one range allowed only
	/*pushConstantRange.stageFlags = (VkShaderStageFlagBits)reflectShaderModule.shader_stage;
	pushConstantRange.offset = preflectBlockVars[0]->offset;
	pushConstantRange.size = preflectBlockVars[0]->size;*/

	delete []preflectBlockVars;
	
	if(spvReflectEnumerateDescriptorSets(&reflectShaderModule,&setCount,0) != SPV_REFLECT_RESULT_SUCCESS)
		throw Exception("Failed to enumerate descriptor sets.");

	//descriptor sets
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
	delete []pPushConstantRanges;
	for(uint i = 0; i < setCount; ++i)
		vkDestroyDescriptorSetLayout(pcomp->logicalDev,pdescSetLayouts[i],0);
	delete []pdescSetLayouts;
	vkDestroyShaderModule(pcomp->logicalDev,shaderModule,0);
}

const std::vector<std::tuple<const char *, VkFormat, uint>> ShaderModule::semanticMap = {
	{"POSITION",VK_FORMAT_R32G32_SFLOAT,8},
	{"POSITION",VK_FORMAT_R32G32_UINT,8},
	{"TEXCOORD",VK_FORMAT_R32G32_SFLOAT,8}, //R16G16?
	{"TEXCOORD",VK_FORMAT_R32G32_UINT,8} //R16G16?
};

const std::vector<std::tuple<const char *, uint>> ShaderModule::variableMap = {
	{"xy0",8},
	{"xy1",8},
	{"transform",16},
	{"screen",8},
	{"margin",8},
	{"titlePad",8},
	{"titleSpan",8},
	{"stackIndex",4},
	{"flags",4},
	{"time",4}
};

Pipeline::Pipeline(ShaderModule *_pvertexShader, ShaderModule *_pgeometryShader, ShaderModule *_pfragmentShader, const std::vector<std::pair<ShaderModule::INPUT, uint>> *pvertexBufferLayout, const VkPipelineInputAssemblyStateCreateInfo *pinputAssemblyStateCreateInfo, const VkPipelineRasterizationStateCreateInfo *prasterizationStateCreateInfo, const VkPipelineDepthStencilStateCreateInfo *pdepthStencilStateCreateInfo, const VkPipelineColorBlendStateCreateInfo *pcolorBlendStateCreateInfo, const VkPipelineDynamicStateCreateInfo *pdynamicStateCreateInfo, const CompositorInterface *_pcomp) : pshaderModule{_pvertexShader,_pgeometryShader,_pfragmentShader}, pcomp(_pcomp){
	VkVertexInputAttributeDescription *pvertexInputAttributeDescs = new VkVertexInputAttributeDescription[_pvertexShader->inputs.size()];
	uint vertexAttributeDescCount = 0;
	uint inputStride = 0;
	if(pvertexBufferLayout){
		for(uint i = 0, n = _pvertexShader->inputs.size(); i < n; ++i){
			auto m = std::find_if(pvertexBufferLayout->begin(),pvertexBufferLayout->end(),[&](auto &r)->bool{
				return (uint)r.first == _pvertexShader->inputs[i].semanticMapIndex;
			});
			if(m == pvertexBufferLayout->end()){
				DebugPrintf(stdout,"warning: semantic %s in %s at location %u not supported by the input buffer.\n",std::get<0>(ShaderModule::semanticMap[_pvertexShader->inputs[i].semanticMapIndex]),_pvertexShader->pname,_pvertexShader->inputs[i].location);
				continue;
			}
			pvertexInputAttributeDescs[vertexAttributeDescCount].offset = m->second;
			pvertexInputAttributeDescs[vertexAttributeDescCount].location = _pvertexShader->inputs[i].location;
			pvertexInputAttributeDescs[vertexAttributeDescCount].binding = _pvertexShader->inputs[i].binding;
			pvertexInputAttributeDescs[vertexAttributeDescCount].format = std::get<1>(ShaderModule::semanticMap[_pvertexShader->inputs[i].semanticMapIndex]);
			pvertexInputAttributeDescs[vertexAttributeDescCount].offset = m->second;
			
			inputStride += std::get<2>(ShaderModule::semanticMap[_pvertexShader->inputs[i].semanticMapIndex]);

			++vertexAttributeDescCount;
		}
	}

	//per-vertex data only, instancing not used
	VkVertexInputBindingDescription vertexInputBindingDesc = {};
	vertexInputBindingDesc.binding = 0;
	vertexInputBindingDesc.stride = inputStride;//_pvertexShader->inputStride;
	vertexInputBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = (uint)(vertexAttributeDescCount > 0);
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDesc;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = vertexAttributeDescCount;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = pvertexInputAttributeDescs;

	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[3];

	uint stageCount = 0;
	uint setCount = 0;
	for(uint i = 0, stageBit[] = {VK_SHADER_STAGE_VERTEX_BIT,VK_SHADER_STAGE_GEOMETRY_BIT,VK_SHADER_STAGE_FRAGMENT_BIT}; i < SHADER_MODULE_COUNT; ++i){
		if(!pshaderModule[i])
			continue;
		setCount += pshaderModule[i]->setCount;

		shaderStageCreateInfo[stageCount] = (VkPipelineShaderStageCreateInfo){};
		shaderStageCreateInfo[stageCount].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStageCreateInfo[stageCount].stage = (VkShaderStageFlagBits)stageBit[i];
		shaderStageCreateInfo[stageCount].module = pshaderModule[i]->shaderModule;
		shaderStageCreateInfo[stageCount].pName = "main";

		++stageCount;
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

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; 

	pushConstantRange.stageFlags = 0;
	pushConstantRange.offset = ~0u;
	pushConstantRange.size = 0;
	for(uint i = 0; i < SHADER_MODULE_COUNT; ++i){
		if(!pshaderModule[i])
			continue;
		if(pshaderModule[i]->pushConstantBlockCount > 0){
			pushConstantRange.stageFlags |= pshaderModule[i]->pPushConstantRanges[0].stageFlags;
			pushConstantRange.offset = std::min(pushConstantRange.offset,pshaderModule[i]->pPushConstantRanges[0].offset);
			pushConstantRange.size = std::max(pushConstantRange.size,pshaderModule[i]->pPushConstantRanges[0].size);
		}
	}

	VkDescriptorSetLayout *pcombinedSets = new VkDescriptorSetLayout[setCount];

	for(uint i = 0, p = 0; i < SHADER_MODULE_COUNT; ++i){
		if(!pshaderModule[i])
			continue;
		std::copy(pshaderModule[i]->pdescSetLayouts,pshaderModule[i]->pdescSetLayouts+pshaderModule[i]->setCount,pcombinedSets+p);
		p += pshaderModule[i]->setCount;
	}

	VkPipelineLayoutCreateInfo layoutCreateInfo = {};
	layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCreateInfo.setLayoutCount = setCount;
	layoutCreateInfo.pSetLayouts = pcombinedSets;
	layoutCreateInfo.pushConstantRangeCount = (bool)(pushConstantRange.stageFlags != 0);
	layoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	if(vkCreatePipelineLayout(pcomp->logicalDev,&layoutCreateInfo,0,&pipelineLayout) != VK_SUCCESS)
		throw Exception("Failed to create a pipeline layout.");
	
	delete []pcombinedSets;

	/*VkDynamicState dynamicStates[1] = {VK_DYNAMIC_STATE_SCISSOR};

	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = 0;
	if(pcomp->scissoring){
		dynamicStateCreateInfo.pDynamicStates = dynamicStates;
		dynamicStateCreateInfo.dynamicStateCount = 1;
	}else{
		dynamicStateCreateInfo.pDynamicStates = 0;
		dynamicStateCreateInfo.dynamicStateCount = 0;
	}*/

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.stageCount = stageCount;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfo;
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = pinputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = prasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = pdepthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = pcolorBlendStateCreateInfo;
	//graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = pdynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = pcomp->renderPass;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = 0;
	graphicsPipelineCreateInfo.basePipelineIndex = -1;

	if(vkCreateGraphicsPipelines(pcomp->logicalDev,0,1,&graphicsPipelineCreateInfo,0,&pipeline) != VK_SUCCESS)
		throw Exception("Failed to create a graphics pipeline.");
	
	delete []pvertexInputAttributeDescs;
}

Pipeline::~Pipeline(){
	vkDestroyPipeline(pcomp->logicalDev,pipeline,0);
	vkDestroyPipelineLayout(pcomp->logicalDev,pipelineLayout,0);
}

ClientPipeline::ClientPipeline(ShaderModule *_pvertexShader, ShaderModule *_pgeometryShader, ShaderModule *_pfragmentShader, const std::vector<std::pair<ShaderModule::INPUT, uint>> *pvertexBufferLayout, const CompositorInterface *_pcomp) : Pipeline(_pvertexShader,_pgeometryShader,_pfragmentShader,pvertexBufferLayout,&inputAssemblyStateCreateInfo,&rasterizationStateCreateInfo,0,&colorBlendStateCreateInfo,_pcomp->scissoring?&dynamicStateCreateInfoScissoring:&dynamicStateCreateInfo,_pcomp){
	//
}

ClientPipeline::~ClientPipeline(){
	//
}

const VkPipelineInputAssemblyStateCreateInfo ClientPipeline::inputAssemblyStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,//VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	.primitiveRestartEnable = VK_FALSE
};

const VkPipelineRasterizationStateCreateInfo ClientPipeline::rasterizationStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	.depthClampEnable = VK_FALSE,
	.rasterizerDiscardEnable = VK_FALSE,
	.polygonMode = VK_POLYGON_MODE_FILL,
	.cullMode = VK_CULL_MODE_NONE,
	.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
	.depthBiasEnable = VK_FALSE,
	.depthBiasConstantFactor = 0.0f,
	.depthBiasClamp = 0.0f,
	.depthBiasSlopeFactor = 0.0f,
	.lineWidth = 1.0f
};

const VkPipelineColorBlendAttachmentState ClientPipeline::colorBlendAttachmentState = {
	.blendEnable = VK_TRUE,
	//premultiplied alpha blending
	.srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
};

const VkDynamicState ClientPipeline::dynamicStatesScissoring[1] = {VK_DYNAMIC_STATE_SCISSOR};

const VkPipelineDynamicStateCreateInfo ClientPipeline::dynamicStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	.pNext = 0,
	.flags = 0,
	.dynamicStateCount = 0,
	.pDynamicStates = 0
};

const VkPipelineDynamicStateCreateInfo ClientPipeline::dynamicStateCreateInfoScissoring = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
	.pNext = 0,
	.flags = 0,
	.dynamicStateCount = 1,
	.pDynamicStates = dynamicStatesScissoring
};

const VkPipelineColorBlendStateCreateInfo ClientPipeline::colorBlendStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	.logicOpEnable = VK_FALSE,
	.logicOp = VK_LOGIC_OP_COPY,
	.attachmentCount = 1,
	.pAttachments = &colorBlendAttachmentState,
	.blendConstants = {0.0f,0.0f,0.0f,0.0f}
};

TextPipeline::TextPipeline(ShaderModule *_pvertexShader, ShaderModule *_pgeometryShader, ShaderModule *_pfragmentShader, const std::vector<std::pair<ShaderModule::INPUT, uint>> *pvertexBufferLayout, const CompositorInterface *_pcomp) : Pipeline(_pvertexShader,_pgeometryShader,_pfragmentShader,pvertexBufferLayout,&inputAssemblyStateCreateInfo,&rasterizationStateCreateInfo,0,&colorBlendStateCreateInfo,&ClientPipeline::dynamicStateCreateInfoScissoring,_pcomp){
	//
}

TextPipeline::~TextPipeline(){
	//
}

const VkPipelineInputAssemblyStateCreateInfo TextPipeline::inputAssemblyStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
	.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
	.primitiveRestartEnable = VK_FALSE
};

const VkPipelineRasterizationStateCreateInfo TextPipeline::rasterizationStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
	.depthClampEnable = VK_FALSE,
	.rasterizerDiscardEnable = VK_FALSE,
	.polygonMode = VK_POLYGON_MODE_FILL,
	//.polygonMode = VK_POLYGON_MODE_LINE,
	.cullMode = VK_CULL_MODE_NONE,
	.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
	.depthBiasEnable = VK_FALSE,
	.depthBiasConstantFactor = 0.0f,
	.depthBiasClamp = 0.0f,
	.depthBiasSlopeFactor = 0.0f,
	.lineWidth = 1.0f
};

const VkPipelineColorBlendAttachmentState TextPipeline::colorBlendAttachmentState = {
	.blendEnable = VK_TRUE,
	//alpha blending
	.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
	.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.colorBlendOp = VK_BLEND_OP_ADD,
	.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
	.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
	.alphaBlendOp = VK_BLEND_OP_ADD,
	.colorWriteMask = VK_COLOR_COMPONENT_R_BIT|VK_COLOR_COMPONENT_G_BIT|VK_COLOR_COMPONENT_B_BIT|VK_COLOR_COMPONENT_A_BIT
};

const VkPipelineColorBlendStateCreateInfo TextPipeline::colorBlendStateCreateInfo = {
	.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
	.logicOpEnable = VK_FALSE,
	.logicOp = VK_LOGIC_OP_COPY,
	.attachmentCount = 1,
	.pAttachments = &colorBlendAttachmentState,
	.blendConstants = {0.0f,0.0f,0.0f,0.0f}
};

}

