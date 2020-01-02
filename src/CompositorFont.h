#ifndef COMPOSITOR_FONT_H
#define COMPOSITOR_FONT_H

#include <hb.h>
#include <hb-ft.h>
//#include <freetype/ftcache.h>
#include <vulkan/vulkan.h>

namespace Compositor{

class Text : public Drawable{
public:
	Text(const char *[Pipeline::SHADER_MODULE_COUNT], class TextEngine *);
	~Text();
	void Set(const char *, const VkCommandBuffer *); //updates the vertex buffers
	void Draw(const VkCommandBuffer *);
	void UpdateDescSets();
protected:
	hb_buffer_t *phbBuf;
	uint glyphCount;
	class TextEngine *ptextEngine;
	class Buffer *pvertexBuffer;
	class Buffer *pindexBuffer;
	struct Vertex{
		glm::vec2 pos;
		glm::uvec2 texc;
	};// alignas(16);
	static std::vector<std::pair<ShaderModule::INPUT, uint>> vertexBufferLayout;
};

class TextEngine{
friend class Text;
public:
	TextEngine(class CompositorInterface *);
	~TextEngine();
	struct Glyph{
		uint w;
		uint h;
		uint pitch;
		glm::vec2 offset;
		unsigned char *pbuffer;
		//texture atlas parameters
		glm::uvec2 texc;
	};
	bool UpdateAtlas(const VkCommandBuffer *);
	Glyph * LoadGlyph(uint);
private:
	//static FT_Error FaceRequester(FTC_FaceID, FT_Library, FT_Pointer, FT_Face *);
	class CompositorInterface *pcomp;
	std::vector<std::pair<uint, Glyph>> glyphMap;
	FT_Library library;
	//FTC_Manager fontCacheManager;
	FT_Face fontFace;
	hb_font_t *phbFont;
	TextureStaged *pfontAtlas;
	bool updateAtlas;
};

}

#endif

