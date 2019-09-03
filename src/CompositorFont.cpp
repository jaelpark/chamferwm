#include "main.h"
#include "container.h"
#include "backend.h"
#include "CompositorFont.h"
#include "CompositorResource.h"
#include "compositor.h"

namespace Compositor{

Text::Text(class TextEngine *_ptextEngine) : ptextEngine(_ptextEngine){
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

void Text::Set(const char *ptext){
	hb_buffer_reset(phbBuf);
	hb_buffer_add_utf8(phbBuf,ptext,-1,0,-1);
	hb_shape(ptextEngine->phbFont,phbBuf,0,0);
}

TextEngine::TextEngine(CompositorInterface *_pcomp) : pcomp(_pcomp){
	if(FT_Init_FreeType(&library) != FT_Err_Ok)
		throw Exception("Failed to initialize Freetype2.");
	if(FT_New_Face(library,"/usr/share/fonts/droid/DroidSans.ttf",0,&fontFace) != FT_Err_Ok)
		throw Exception("Failed to load font.");
	phbFont = hb_ft_font_create(fontFace,0);
}

TextEngine::~TextEngine(){
	hb_font_destroy(phbFont);
	FT_Done_Face(fontFace);
	FT_Done_FreeType(library);
}

}

