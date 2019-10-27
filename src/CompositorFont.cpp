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

	pvertexBuffer = new Buffer(1024,VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,ptextEngine->pcomp);
}

Text::~Text(){
	delete pvertexBuffer;

	hb_buffer_destroy(phbBuf);
}

void Text::Set(const char *ptext, const VkCommandBuffer *pcommandBuffer){
	hb_buffer_reset(phbBuf);
	hb_buffer_add_utf8(phbBuf,ptext,-1,0,-1);
	hb_buffer_guess_segment_properties(phbBuf);
	hb_shape(ptextEngine->phbFont,phbBuf,0,0);

	uint glyphCount;
	hb_glyph_info_t *pglyphInfo = hb_buffer_get_glyph_infos(phbBuf,&glyphCount);
	hb_glyph_position_t *pglyphPos = hb_buffer_get_glyph_positions(phbBuf,&glyphCount);

	//loop through codepoints, update texture atlas if necessary

	//Vertex *pdata = (Vertex*)pvertexBuffer->Map();

	//test triangle
	/*pdata[0].pos = glm::vec2(0.0f,-0.5f);
	pdata[1].pos = glm::vec2(0.5f,0.5f);
	pdata[2].pos = glm::vec2(-0.5f,0.5f);*/

	glm::vec2 pos = glm::vec2(0.0f);
	for(uint i = 0; i < glyphCount; ++i){
		glm::vec2 advance = glm::vec2(pglyphPos[i].x_advance,pglyphPos[i].y_advance)/64.0f;
		glm::vec2 offset = glm::vec2(pglyphPos[i].x_offset,pglyphPos[i].y_offset)/64.0f;

		TextEngine::Glyph *pglyph = ptextEngine->LoadGlyph(pglyphInfo[i].codepoint);
		glm::vec2 xy0 = pos+offset+pglyph->offset;
		//xy0.y = floorf(xy0.y);
		glm::vec2 xy1 = xy0+glm::vec2(pglyph->w,-(float)pglyph->h);
		//xy1.y = floorf(xy1.y);

		printf("%.3f, %.3f - %.3f, %.3f\n",xy0.x,xy0.y,xy1.x,xy1.y);
		//pdata[i].pos = 
		//pdata[i].pos.y = 

		pos += advance;
	}

	//pvertexBuffer->Unmap(pcommandBuffer);
}

void Text::Draw(const VkCommandBuffer *pcommandBuffer){
	//
	std::vector<std::pair<ShaderModule::VARIABLE, const void *>> varAddrs = {
		//{ShaderModule::VARIABLE_FLAGS,&flags},
	};
	//BindShaderResources(&varAddrs,pcommandBuffer);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(*pcommandBuffer,0,1,&pvertexBuffer->buffer,&offset);
	vkCmdDraw(*pcommandBuffer,3,1,0,0);

	passignedSet->fenceTag = pcomp->frameTag;
}

std::vector<std::pair<ShaderModule::INPUT, uint>> Text::vertexBufferLayout = {
	{ShaderModule::INPUT_POSITION_SFLOAT2,offsetof(Text::Vertex,pos)}
};

TextEngine::TextEngine(CompositorInterface *_pcomp) : pcomp(_pcomp){
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
	FT_Set_Char_Size(fontFace,0,50*64,96,96);
	
	phbFont = hb_ft_font_create(fontFace,0);

	glyphMap.reserve(1024);
}

TextEngine::~TextEngine(){
	for(auto m : glyphMap)
		delete []m.second.pbuffer;
	glyphMap.clear();

	hb_font_destroy(phbFont);
	FT_Done_Face(fontFace);
	//FTC_Manager_Done(fontCacheManager);
	FT_Done_FreeType(library);
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

	//TODO: mark the texture atlas for update
	
	return &glyphMap.back().second;
}

/*FT_Error TextEngine::FaceRequester(FTC_FaceID faceId, FT_Library library, FT_Pointer ptr, FT_Face *pfontFace){
	//
	return FT_New_Face(library,"/usr/share/fonts/droid/DroidSans.ttf",0,pfontFace);
}*/

}

