#ifndef COMPOSITOR_FONT_H
#define COMPOSITOR_FONT_H

//#include <ft2build.h>
//#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>
#include <vulkan/vulkan.h>

namespace Compositor{

class Text{
public:
	Text(class TextEngine *);
	~Text();
	void Set(const char *); //updates the vertex buffers
	hb_buffer_t *phbBuf;
	class TextEngine *ptextEngine;
	class Buffer *pvertexBuffer;
};

class TextEngine{
friend class Text;
public:
	TextEngine(class CompositorInterface *);
	~TextEngine();

	class CompositorInterface *pcomp;
	FT_Library library;
	FT_Face fontFace;
	hb_font_t *phbFont;
};

}

#endif

