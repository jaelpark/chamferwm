#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorResource.h"
#include "compositor.h"
#include "CompositorFont.h"
#include <algorithm>
#include <fontconfig/fontconfig.h>

namespace Compositor{

FontAtlas::FontAtlas(uint _size, TextEngine *_ptextEngine) : ptextEngine(_ptextEngine), size(_size), fontAtlasCursor(0), refCount(1), releaseTag(_ptextEngine->pcomp->frameTag){
	//
	ptexture = new TextureStaged(size,size,VK_FORMAT_R8_UNORM,&TextureBase::defaultComponentMapping,0,ptextEngine->pcomp);
}

FontAtlas::~FontAtlas(){
	delete ptexture;
}

void FontAtlas::Update(hb_glyph_info_t *pglyphInfo, uint glyphCount, const VkCommandBuffer *pcommandBuffer){
	for(uint i = 0; i < glyphCount; ++i)
		if(std::find_if(glyphCollection.begin(),glyphCollection.end(),[&](auto r)->bool{
			return r.codepoint == pglyphInfo[i].codepoint;
		}) == glyphCollection.end()){
			TextEngine::Glyph *pglyph = ptextEngine->LoadGlyph(pglyphInfo[i].codepoint);
			if(fontAtlasCursor.x+pglyph->w >= size){
				fontAtlasCursor.x = 0;
				fontAtlasCursor.y += (ptextEngine->fontFace->size->metrics.height>>6)+1;
			}

			GlyphEntry glyphEntry;
			glyphEntry.codepoint = pglyphInfo[i].codepoint;
			glyphEntry.texc = fontAtlasCursor;
			glyphEntry.pglyph = pglyph;
			glyphCollection.push_back(glyphEntry);

			fontAtlasCursor.x += pglyph->w+1;
		}

	unsigned char *pdata = (unsigned char *)ptexture->Map();

	for(auto &glyphEntry : glyphCollection){
		TextEngine::Glyph *pglyph = ptextEngine->LoadGlyph(glyphEntry.codepoint);
		for(uint y = 0; y < pglyph->h; ++y){
			uint offsetDst = size*(glyphEntry.texc.y+y)+glyphEntry.texc.x;
			uint offsetSrc = pglyph->w*y;
			memcpy(pdata+offsetDst,pglyph->pbuffer+offsetSrc,pglyph->w);
		}
	}

	/*FILE *pf = fopen("/tmp/output.bin","wb");
	fwrite(pdata,1,pfontAtlas->size*pfontAtlas->size,pf);
	fclose(pf);*/

	//loop over glyphCollection, map everything according to their texcoord

	//TODO: partial update
	/*std::vector<uint> glyphIndexMap; //todo: move to TextEngine, clear() after use
	for(uint i = 0; i < glyphCount; ++i)
		if(std::find_if(pfontAtlas->glyphCollection.begin(),pfontAtlas->glyphCollection.end(),[&](auto r)->bool{
			return r.first == pglyphInfo[i].codepoint;
		}) == pfontAtlas->glyphCollection.end()){
			auto m = std::find_if(ptextEngine->glyphMap.begin(),ptextEngine->glyphMap.end(),[&](auto r1)->bool{
				return pglyphInfo[i].codepoint == r1.codepoint;
			});
			glyphIndexMap.push_back(m-ptextEngine->glyphMap.begin());
		}

	for(uint i : glyphIndexMap){
		if(pfontAtlas->fontAtlasCursor.x+ptextEngine->glyphMap[i].w >= pfontAtlas->size){
			pfontAtlas->fontAtlasCursor.x = 0;
			pfontAtlas->fontAtlasCursor.y += (ptextEngine->fontFace->size->metrics.height>>6)+1;
		}
		ptextEngine->glyphMap[i].texc = pfontAtlas->fontAtlasCursor;
		pfontAtlas->fontAtlasCursor.x += ptextEngine->glyphMap[i].w+1;

		for(uint y = 0; y < ptextEngine->glyphMap[i].h; ++y){
			uint offsetDst = pfontAtlas->size*(ptextEngine->glyphMap[i].texc.y+y)+ptextEngine->glyphMap[i].texc.x;
			uint offsetSrc = ptextEngine->glyphMap[i].w*y;
			memcpy(pdata+offsetDst,ptextEngine->glyphMap[i].pbuffer+offsetSrc,ptextEngine->glyphMap[i].w);
		}
	}*/

	VkRect2D atlasRect;
	atlasRect.offset = {0,0};
	atlasRect.extent = {size,size};
	ptexture->Unmap(pcommandBuffer,&atlasRect,1);
}

Text::Text(const char *pshaderName[Pipeline::SHADER_MODULE_COUNT], TextEngine *_ptextEngine) : Drawable(_ptextEngine->pcomp->LoadPipeline<TextPipeline>(pshaderName,&vertexBufferLayout),_ptextEngine->pcomp), glyphCount(0), textLength(0.0f), ptextEngine(_ptextEngine), pfontAtlas(0){
	//
	phbBuf = hb_buffer_create();
	hb_buffer_set_direction(phbBuf,HB_DIRECTION_LTR);
	hb_buffer_set_script(phbBuf,HB_SCRIPT_LATIN);
	hb_buffer_set_language(phbBuf,hb_language_from_string("en",-1));

	pvertexBuffer = new Buffer(1024*4*sizeof(Vertex),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,ptextEngine->pcomp);
	pindexBuffer = new Buffer(1024*6*2,VK_BUFFER_USAGE_INDEX_BUFFER_BIT,ptextEngine->pcomp);
}

Text::~Text(){
	delete pvertexBuffer;
	delete pindexBuffer;

	hb_buffer_destroy(phbBuf);
}

//TODO: text max length, terminate oversized text with '...'
void Text::Set(const char *ptext, const VkCommandBuffer *pcommandBuffer){
	hb_buffer_reset(phbBuf);
	hb_buffer_add_utf8(phbBuf,ptext,-1,0,-1);
	hb_buffer_guess_segment_properties(phbBuf);
	hb_shape(ptextEngine->phbFont,phbBuf,0,0);

	hb_glyph_info_t *pglyphInfo = hb_buffer_get_glyph_infos(phbBuf,&glyphCount);
	hb_glyph_position_t *pglyphPos = hb_buffer_get_glyph_positions(phbBuf,&glyphCount);

	FontAtlas *pfontAtlas1 = ptextEngine->CreateAtlas(pglyphInfo,glyphCount,pcommandBuffer);
	if(pfontAtlas1 != pfontAtlas){
		pfontAtlas = pfontAtlas1;

		AssignPipeline(passignedSet->p);
		UpdateDescSets();
	}

	if(glyphCount == 0)
		return;

	//pfontAtlas->Update(pglyphInfo,glyphCount,pcommandBuffer);

	Vertex *pvertices = (Vertex*)pvertexBuffer->Map();

	//TODO: load all glyphs here before filling the vertex buffers. If change in texture size is required,
	//create a new texture. At the same time, see if any of the old glyphs can be purged.
	//Descriptor set for only this Text has to be updated. If texture size stays the same,
	//append to the current most recently created texture (or any other previously created texture - there might
	//even be a texture with everything readily available).

	//Text objects keep a pointer to the texture they use. If the users of some particular texture decreases to zero,
	//texture is freed. Use ReleaseTexture to decrease a reference counter, and mark the frame tag when ready to free.

	//test triangle
	/*pvertices[0].pos = glm::vec2(0.0f,-0.5f);
	pvertices[1].pos = glm::vec2(0.5f,0.5f);
	pvertices[2].pos = glm::vec2(-0.5f,0.5f);*/

	//test quad
	/*pvertices[0].pos = glm::vec2(-1.0f,-1.0f);
	pvertices[1].pos = glm::vec2(-1.0f,0.0f);
	pvertices[2].pos = glm::vec2(0.0f,-1.0f);
	pvertices[3].pos = glm::vec2(0.0f,0.0f);*/

	//glm::vec2 scale = 2.0f/glm::vec2(ptextEngine->pcomp->imageExtent.width,ptextEngine->pcomp->imageExtent.height);

	TextEngine::Glyph *plastGlyph;

	glm::vec2 position(0.0f);
	for(uint i = 0; i < glyphCount; ++i){
		glm::vec2 advance = glm::vec2(pglyphPos[i].x_advance,pglyphPos[i].y_advance)/64.0f;
		glm::vec2 offset = glm::vec2(pglyphPos[i].x_offset,pglyphPos[i].y_offset)/64.0f;

		auto m = std::find_if(pfontAtlas->glyphCollection.begin(),pfontAtlas->glyphCollection.end(),[&](auto r)->bool{
			return r.codepoint == pglyphInfo[i].codepoint;
		});
		plastGlyph = (*m).pglyph;

		glm::vec2 xy0 = position+offset+(*m).pglyph->offset; //+0.5f in Draw()
		glm::vec2 xy1 = xy0+glm::vec2((*m).pglyph->w,(*m).pglyph->h);

		pvertices[4*i+0].pos = xy0;
		pvertices[4*i+1].pos = glm::vec2(xy1.x,xy0.y);
		pvertices[4*i+2].pos = glm::vec2(xy0.x,xy1.y);
		pvertices[4*i+3].pos = glm::vec2(xy1.x,xy1.y);
		/*pvertices[4*i+0].pos = scale*xy0-1.0f;
		pvertices[4*i+1].pos = scale*glm::vec2(xy1.x,xy0.y)-1.0f;
		pvertices[4*i+2].pos = scale*glm::vec2(xy0.x,xy1.y)-1.0f;
		pvertices[4*i+3].pos = scale*glm::vec2(xy1.x,xy1.y)-1.0f;*/

		pvertices[4*i+0].texc = (*m).texc;//use posh to get sample location
		pvertices[4*i+1].texc = (*m).texc+glm::uvec2((*m).pglyph->w,0);
		pvertices[4*i+2].texc = (*m).texc+glm::uvec2(0,(*m).pglyph->h);
		pvertices[4*i+3].texc = (*m).texc+glm::uvec2((*m).pglyph->w,(*m).pglyph->h);

		position += advance;
	}
	textLength = position.x+(float)plastGlyph->w;

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

void Text::Draw(const glm::uvec2 &pos, const glm::mat2x2 &transform, const VkCommandBuffer *pcommandBuffer){
	//
	glm::vec2 imageExtent = glm::vec2(ptextEngine->pcomp->imageExtent.width,ptextEngine->pcomp->imageExtent.height);

	glm::vec2 posh = 2.0f*(glm::vec2(pos)+0.5f)/glm::vec2(pcomp->imageExtent.width,pcomp->imageExtent.height); //-1.0f in shader
	//glm::mat2x2 transform = glm::mat2x2(0,1,-1,0);

	std::vector<std::pair<ShaderModule::VARIABLE, const void *>> varAddrs = {
		{ShaderModule::VARIABLE_XY0,&posh},
		{ShaderModule::VARIABLE_TRANSFORM,&transform},
		{ShaderModule::VARIABLE_SCREEN,&imageExtent}
	};
	BindShaderResources(&varAddrs,pcommandBuffer);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(*pcommandBuffer,0,1,&pvertexBuffer->buffer,&offset);
	vkCmdBindIndexBuffer(*pcommandBuffer,pindexBuffer->buffer,0,VK_INDEX_TYPE_UINT16);
	//vkCmdDraw(*pcommandBuffer,3,1,0,0);
	vkCmdDrawIndexed(*pcommandBuffer,6*glyphCount,1,0,0,0);
	//vkCmdDrawIndexed(*pcommandBuffer,6,1,0,0,0);

	passignedSet->fenceTag = pcomp->frameTag;
}

void Text::UpdateDescSets(){
	VkDescriptorImageInfo descImageInfo = {};
	descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descImageInfo.imageView = pfontAtlas->ptexture->imageView;
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

float Text::GetTextLength() const{
	return textLength;
}

std::vector<std::pair<ShaderModule::INPUT, uint>> Text::vertexBufferLayout = {
	{ShaderModule::INPUT_POSITION_SFLOAT2,offsetof(Text::Vertex,pos)},
	{ShaderModule::INPUT_TEXCOORD_UINT2,offsetof(Text::Vertex,texc)}
};

TextEngine::TextEngine(CompositorInterface *_pcomp) : pcomp(_pcomp){
	if(FT_Init_FreeType(&library) != FT_Err_Ok)
		throw Exception("Failed to initialize Freetype2.");
	/*if(FTC_Manager_New(library,0,0,0,&FaceRequester,0,&fontCacheManager) != FT_Err_Ok)
		throw Exception("Failed to create a font cache manager.");*/

	/*static FTC_FaceID faceId = (FTC_FaceID)"DroidSans";
	if(FTC_Manager_LookupFace(fontCacheManager,faceId,&fontFace) != FT_Err_Ok)
		throw Exception("Failed to load font.");*/

	FcConfig *pfontConfig = FcInitLoadConfigAndFonts();

	FcPattern *pPattern = FcNameParse((const FcChar8*)"Droid Sans");
	FcConfigSubstitute(pfontConfig,pPattern,FcMatchPattern);
	FcDefaultSubstitute(pPattern);

	FcResult result;
	FcPattern *pPatternResult = FcFontMatch(pfontConfig,pPattern,&result);
	if(!pPatternResult || result != FcResultMatch)
		throw Exception("Could not find font.");
	FcChar8 *pfileName;
	if(FcPatternGetString(pPatternResult,FC_FILE,0,&pfileName) != FcResultMatch)
		throw Exception("Could not find font.");
	printf("Loaded font: %s\n",pfileName);
	if(FT_New_Face(library,(const char*)pfileName,0,&fontFace) != FT_Err_Ok)
		throw Exception("Failed to load font.");

	FcPatternDestroy(pPatternResult);
	FcPatternDestroy(pPattern);

	FcConfigDestroy(pfontConfig);

	glm::vec2 dpi = pcomp->GetDPI();
	FT_Set_Char_Size(fontFace,0,16<<6,(uint)dpi.x,(uint)dpi.y);
	
	phbFont = hb_ft_font_create(fontFace,0);

	glyphMap.reserve(1024);
}

TextEngine::~TextEngine(){
	for(FontAtlas *pfontAtlas : fontAtlasMap)
		delete pfontAtlas;
	
	for(auto &m : glyphMap)
		delete []m.pbuffer;
	glyphMap.clear();

	hb_font_destroy(phbFont);
	FT_Done_Face(fontFace);
	//FTC_Manager_Done(fontCacheManager);
	FT_Done_FreeType(library);
}

FontAtlas * TextEngine::CreateAtlas(hb_glyph_info_t *pglyphInfo, uint glyphCount, const VkCommandBuffer *pcommandBuffer){
	//1. check if there's an atlas if all glyphs readily available
	auto m1 = std::find_if(fontAtlasMap.begin(),fontAtlasMap.end(),[&](auto r)->bool{
		//check if pglyphInfo array is a subset of the glyphCollection of the current font atlas
		for(uint i = 0; i < glyphCount; ++i)
			if(std::find_if(r->glyphCollection.begin(),r->glyphCollection.end(),[&](auto r1)->bool{
				return pglyphInfo[i].codepoint == r1.codepoint;
			}) == r->glyphCollection.end())
				return false;
		return true;
	});
	if(m1 != fontAtlasMap.end()){
		++(*m1)->refCount;
		return (*m1);
	}
	
	//2. check if one of the atlases can be appended without resizing it
	auto m2 = std::find_if(fontAtlasMap.begin(),fontAtlasMap.end(),[&](auto r)->bool{
		uint size = (1+(fontFace->size->metrics.height>>6))*ceilf(sqrtf(r->glyphCollection.size()+glyphCount)); // /64
		size = ((size-1)/128+1)*128; //round up to nearest multiple of 128 to improve reuse of the texture
		return size <= r->size;
	});
	if(m2 != fontAtlasMap.end()){
		++(*m2)->refCount;
		(*m2)->Update(pglyphInfo,glyphCount,pcommandBuffer);
		return (*m2);
	}

	uint size = (1+(fontFace->size->metrics.height>>6))*ceilf(sqrtf(glyphCount)); // /64
	size = ((size-1)/128+1)*128; //round up to nearest multiple of 128 to improve reuse of the texture

	FontAtlas *pfontAtlas = new FontAtlas(size,this);
	pfontAtlas->Update(pglyphInfo,glyphCount,pcommandBuffer);

	fontAtlasMap.push_back(pfontAtlas);

	return pfontAtlas;
}

void TextEngine::ReleaseAtlas(FontAtlas *pfontAtlas){
	--pfontAtlas->refCount;
	pfontAtlas->releaseTag = pcomp->frameTag;
}

TextEngine::Glyph * TextEngine::LoadGlyph(uint codePoint){
	auto m = std::find_if(glyphMap.begin(),glyphMap.end(),[&](auto r)->bool{
		return r.codepoint == codePoint;
	});
	if(m != glyphMap.end())
		return &(*m);

	FT_Load_Glyph(fontFace,codePoint,FT_LOAD_RENDER);
	FT_Bitmap *pbitmap = &fontFace->glyph->bitmap;

	Glyph glyph;
	glyph.codepoint = codePoint;
	glyph.w = pbitmap->width;
	glyph.h = pbitmap->rows;
	glyph.pitch = pbitmap->pitch;
	glyph.offset = glm::vec2(fontFace->glyph->bitmap_left,-fontFace->glyph->bitmap_top);
	glyph.pbuffer = new unsigned char[pbitmap->rows*pbitmap->pitch];
	memcpy(glyph.pbuffer,pbitmap->buffer,pbitmap->rows*pbitmap->pitch);
	glyphMap.push_back(glyph);

	return &glyphMap.back();
}

float TextEngine::GetFontSize() const{
	return (float)(fontFace->size->metrics.height>>6)/(float)pcomp->imageExtent.width;
}


/*FT_Error TextEngine::FaceRequester(FTC_FaceID faceId, FT_Library library, FT_Pointer ptr, FT_Face *pfontFace){
	//
	return FT_New_Face(library,"/usr/share/fonts/droid/DroidSans.ttf",0,pfontFace);
}*/

}

