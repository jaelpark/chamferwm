#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"
#include "CompositorFont.h"
#include <algorithm>

namespace Compositor{

Text::Text(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], class TextEngine *_ptextEngine) : Drawable(_ptextEngine->pcomp->LoadPipeline<TextPipeline>(pshaderName,&vertexBufferLayout),_ptextEngine->pcomp), ptextEngine(_ptextEngine){
	//
	phbBuf = hb_buffer_create();
	hb_buffer_set_direction(phbBuf,HB_DIRECTION_LTR);
	hb_buffer_set_script(phbBuf,HB_SCRIPT_LATIN);
	hb_buffer_set_language(phbBuf,hb_language_from_string("en",-1));

	glyphCount = 0;

	pvertexBuffer = new Buffer(1024,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,ptextEngine->pcomp);
	pindexBuffer = new Buffer(1024,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,ptextEngine->pcomp);
}

Text::~Text(){
	delete pvertexBuffer;
	delete pindexBuffer;

	hb_buffer_destroy(phbBuf);
}

void Text::Set(const char *ptext, const VkCommandBuffer *pcommandBuffer){
	hb_buffer_reset(phbBuf);
	hb_buffer_add_utf8(phbBuf,ptext,-1,0,-1);
	hb_buffer_guess_segment_properties(phbBuf);
	hb_shape(ptextEngine->phbFont,phbBuf,0,0);

	hb_glyph_info_t *pglyphInfo = hb_buffer_get_glyph_infos(phbBuf,&glyphCount);
	hb_glyph_position_t *pglyphPos = hb_buffer_get_glyph_positions(phbBuf,&glyphCount);

	//loop through codepoints, update texture atlas if necessary

	Vertex *pdata = (Vertex*)pvertexBuffer->Map();

	//test triangle
	/*pdata[0].pos = glm::vec2(0.0f,-0.5f);
	pdata[1].pos = glm::vec2(0.5f,0.5f);
	pdata[2].pos = glm::vec2(-0.5f,0.5f);*/

	//test quad
	/*pdata[0].pos = glm::vec2(-1.0f,-1.0f);
	pdata[1].pos = glm::vec2(-1.0f,0.0f);
	pdata[2].pos = glm::vec2(0.0f,-1.0f);
	pdata[3].pos = glm::vec2(0.0f,0.0f);*/

	glm::vec2 scale = 2.0f/glm::vec2(ptextEngine->pcomp->imageExtent.width,ptextEngine->pcomp->imageExtent.height);

	glm::vec2 position(0.0f);
	for(uint i = 0; i < glyphCount; ++i){
		glm::vec2 advance = glm::vec2(pglyphPos[i].x_advance,pglyphPos[i].y_advance)/64.0f;
		glm::vec2 offset = glm::vec2(pglyphPos[i].x_offset,pglyphPos[i].y_offset)/64.0f;

		TextEngine::Glyph *pglyph = ptextEngine->LoadGlyph(pglyphInfo[i].codepoint);
		glm::vec2 xy0 = position+offset+pglyph->offset;
		//xy0.y = floorf(xy0.y);
		glm::vec2 xy1 = xy0+glm::vec2(pglyph->w,-(float)pglyph->h);
		//xy1.y = floorf(xy1.y);

		//printf("%.3f, %.3f - %.3f, %.3f\n",xy0.x,xy0.y,xy1.x,xy1.y);
		pdata[4*i+0].pos = scale*xy0-1.0f+0.5f;
		pdata[4*i+1].pos = scale*glm::vec2(xy1.x,xy0.y)-1.0f+0.5f;
		pdata[4*i+2].pos = scale*glm::vec2(xy0.x,xy1.y)-1.0f+0.5f;
		pdata[4*i+3].pos = scale*glm::vec2(xy1.x,xy1.y)-1.0f+0.5f;

		//BUG!!!!!!!!!! texc not assigned yet
		pdata[4*i+0].texc = pglyph->texc;//use posh to get sample location
		pdata[4*i+1].texc = pglyph->texc;//+glm::uvec2(pglyph->w,0);
		pdata[4*i+2].texc = pglyph->texc;//+glm::uvec2(pglyph->w,0);
		pdata[4*i+3].texc = pglyph->texc;

		position += advance;
	}

	pvertexBuffer->Unmap(pcommandBuffer);

	unsigned short *pindices = (unsigned short*)pindexBuffer->Map();

	//test quad
	/*pindices[0] = 0;
	pindices[1] = 1;
	pindices[2] = 2;
	pindices[3] = 1;
	pindices[4] = 2;
	pindices[5] = 3;*/

	for(uint i = 0; i < glyphCount; ++i){
		pindices[6*i+0] = 4*i;
		pindices[6*i+1] = 4*i+1;
		pindices[6*i+2] = 4*i+2;
		pindices[6*i+3] = 4*i+1;
		pindices[6*i+4] = 4*i+2;
		pindices[6*i+5] = 4*i+3;
	}

	pindexBuffer->Unmap(pcommandBuffer);
}

void Text::Draw(const VkCommandBuffer *pcommandBuffer){
	//
	std::vector<std::pair<ShaderModule::VARIABLE, const void *>> varAddrs = {
		//{ShaderModule::VARIABLE_FLAGS,&flags},
	};
	BindShaderResources(&varAddrs,pcommandBuffer);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(*pcommandBuffer,0,1,&pvertexBuffer->buffer,&offset);
	vkCmdBindIndexBuffer(*pcommandBuffer,pindexBuffer->buffer,0,VK_INDEX_TYPE_UINT16);
	//vkCmdDraw(*pcommandBuffer,3,1,0,0);
	//vkCmdDrawIndexed(*pcommandBuffer,6*glyphCount,1,0,0,0);
	vkCmdDrawIndexed(*pcommandBuffer,6,1,0,0,0);

	passignedSet->fenceTag = pcomp->frameTag;
}

void Text::UpdateDescSets(){
	VkDescriptorImageInfo descImageInfo = {};
	descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descImageInfo.imageView = ptextEngine->pfontAtlas->imageView;
	descImageInfo.sampler = pcomp->pointSampler;

	std::vector<VkWriteDescriptorSet> writeDescSets;
	for(uint i = 0; i < Pipeline::SHADER_MODULE_COUNT; ++i){
		if(!passignedSet->p->pshaderModule[i])
			continue;
		auto m1 = std::find_if(passignedSet->p->pshaderModule[i]->bindings.begin(),passignedSet->p->pshaderModule[i]->bindings.end(),[&](auto &r)->bool{
			return r.type == VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE && strcmp(r.pname,"fontAtlas") == 0;
		});
		if(m1 != passignedSet->p->pshaderModule[i]->bindings.end()){
			VkWriteDescriptorSet &writeDescSet = writeDescSets.emplace_back();
			writeDescSet = (VkWriteDescriptorSet){};
			writeDescSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writeDescSet.dstSet = passignedSet->pdescSets[i][(*m1).setIndex];
			writeDescSet.dstBinding = (*m1).binding;
			writeDescSet.dstArrayElement = 0;
			writeDescSet.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			writeDescSet.descriptorCount = 1;
			writeDescSet.pImageInfo = &descImageInfo;
		}
	}

	vkUpdateDescriptorSets(pcomp->logicalDev,writeDescSets.size(),writeDescSets.data(),0,0);
}

std::vector<std::pair<ShaderModule::INPUT, uint>> Text::vertexBufferLayout = {
	{ShaderModule::INPUT_POSITION_SFLOAT2,offsetof(Text::Vertex,pos)},
	{ShaderModule::INPUT_TEXCOORD_UINT2,offsetof(Text::Vertex,texc)}
};

TextEngine::TextEngine(CompositorInterface *_pcomp) : pcomp(_pcomp), pfontAtlas(0), updateAtlas(false){
	//TODO: use fontconfig library to load available fonts
	if(FT_Init_FreeType(&library) != FT_Err_Ok)
		throw Exception("Failed to initialize Freetype2.");
	/*if(FTC_Manager_New(library,0,0,0,&FaceRequester,0,&fontCacheManager) != FT_Err_Ok)
		throw Exception("Failed to create a font cache manager.");*/

	/*static FTC_FaceID faceId = (FTC_FaceID)"DroidSans";
	if(FTC_Manager_LookupFace(fontCacheManager,faceId,&fontFace) != FT_Err_Ok)
		throw Exception("Failed to load font.");*/

	//
	if(FT_New_Face(library,"/usr/share/fonts/droid/DroidSans.ttf",0,&fontFace) != FT_Err_Ok)
		throw Exception("Failed to load font.");
	//FT_Set_Char_Size(fontFace,0,50*64,96,96);
	FT_Set_Char_Size(fontFace,0,16<<6,282,282); //*64
	
	phbFont = hb_ft_font_create(fontFace,0);

	glyphMap.reserve(1024);
}

TextEngine::~TextEngine(){
	if(pfontAtlas)
		delete pfontAtlas;
		//pcomp->ReleaseTexture(pfontAtlas);
	
	for(auto m : glyphMap)
		delete []m.second.pbuffer;
	glyphMap.clear();

	hb_font_destroy(phbFont);
	FT_Done_Face(fontFace);
	//FTC_Manager_Done(fontCacheManager);
	FT_Done_FreeType(library);
}

bool TextEngine::UpdateAtlas(const VkCommandBuffer *pcommandBuffer){
	if(!updateAtlas)
		return false;
	updateAtlas = false;
	
	if(pfontAtlas){
		vkDeviceWaitIdle(pcomp->logicalDev); //TODO: implement caching for TextureStaged interface
		delete pfontAtlas;
		//pcomp->ReleaseTexture(pfontAtlas);
	}
	
	//uint m = (1+16*64)*ceilf(sqrtf(glyphMap.size()));
	uint m = (1+(fontFace->size->metrics.height>>6))*ceilf(sqrtf(glyphMap.size())); // /64
	//uint m = (1+16*282/64)*ceilf(sqrtf(glyphMap.size())); //https://www.freetype.org/freetype2/docs/glyphs/glyphs-2.html
	m = ((m-1)/128+1)*128; //round up to nearest multiple of 128 to improve reuse of the texture
	DebugPrintf(stdout,"Font Atlas: %lu glyphs, %ux%u\n",glyphMap.size(),m,m);

	//pfontAtlas = pcomp->CreateTexture(m,m,0);
	pfontAtlas = new TextureStaged(m,m,VK_FORMAT_R8_UNORM,&TextureBase::defaultComponentMapping,0,pcomp);

	unsigned char *pdata = (unsigned char *)pfontAtlas->Map();

	glm::uvec2 position(0);
	for(uint i = 0, n = glyphMap.size(); i < n; ++i){
		if(position.x+glyphMap[i].second.w >= m){
			position.x = 0;
			position.y += fontFace->size->metrics.height*64+1;
		}

		for(uint y = 0; y < glyphMap[i].second.h; ++y){
			uint offsetDst = m*(y+position.y)+position.x;
			uint offsetSrc = glyphMap[i].second.w*y;
			memcpy(pdata+offsetDst,glyphMap[i].second.pbuffer+offsetSrc,glyphMap[i].second.pitch);//width
		}

		position.x += glyphMap[i].second.w+1;

		glyphMap[i].second.texc = position;
	}

	/*FILE *pf = fopen("/tmp/output.bin","wb");
	fwrite(pdata,1,m*m,pf);
	fclose(pf);*/

	VkRect2D atlasRect;
	atlasRect.offset = {0,0};
	atlasRect.extent = {m,m};
	pfontAtlas->Unmap(pcommandBuffer,&atlasRect,1);

	return true;
}

TextEngine::Glyph * TextEngine::LoadGlyph(uint codePoint){
	auto m = std::find_if(glyphMap.begin(),glyphMap.end(),[&](auto r)->bool{
		return r.first == codePoint;
	});
	if(m != glyphMap.end())
		return &(*m).second;
	
	FT_Load_Glyph(fontFace,codePoint,FT_LOAD_RENDER);
	FT_Bitmap *pbitmap = &fontFace->glyph->bitmap;
	
	Glyph glyph;
	glyph.w = pbitmap->width;
	glyph.h = pbitmap->rows;
	glyph.pitch = pbitmap->pitch;
	glyph.offset = glm::vec2(fontFace->glyph->bitmap_left,fontFace->glyph->bitmap_top);
	glyph.pbuffer = new unsigned char[pbitmap->rows*pbitmap->pitch];
	memcpy(glyph.pbuffer,pbitmap->buffer,pbitmap->rows*pbitmap->pitch);
	glyphMap.push_back(std::pair<uint, Glyph>(codePoint,glyph));

	updateAtlas = true;
	
	return &glyphMap.back().second;
}

/*FT_Error TextEngine::FaceRequester(FTC_FaceID faceId, FT_Library library, FT_Pointer ptr, FT_Face *pfontFace){
	//
	return FT_New_Face(library,"/usr/share/fonts/droid/DroidSans.ttf",0,pfontFace);
}*/

}

