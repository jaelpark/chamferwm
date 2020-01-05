#ifndef COMPOSITOR_FONT_H
#define COMPOSITOR_FONT_H

#include <hb.h>
#include <hb-ft.h>
//#include <freetype/ftcache.h>
#include <vulkan/vulkan.h>

namespace Compositor{

struct FontAtlas{ //TODO: own class?
	TextureStaged *ptexture;
	std::vector<std::pair<uint, glm::uvec2>> glyphCollection;
	glm::uvec2 fontAtlasCursor; //this is useless since Map() for the full region erases everything
	uint size;
	uint refCount;
	uint64 releaseTag;
};

class Text : public Drawable{
public:
	Text(const char *[Pipeline::SHADER_MODULE_COUNT], class TextEngine *);
	~Text();
	void Set(const char *, const VkCommandBuffer *); //updates the vertex buffers
	void Draw(const glm::uvec2 &, const VkCommandBuffer *);
	void UpdateDescSets();
protected:
	hb_buffer_t *phbBuf;
	uint glyphCount;
	class TextEngine *ptextEngine;
	class Buffer *pvertexBuffer;
	class Buffer *pindexBuffer;
	FontAtlas *pfontAtlas;
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
		uint codepoint;
		uint w;
		uint h;
		uint pitch;
		glm::vec2 offset;
		unsigned char *pbuffer;
		//texture atlas parameters
		//glm::uvec2 texc;
	};
	//FontAtlas * CreateAtlas(uint);
	FontAtlas * CreateAtlas(hb_glyph_info_t *, uint);
	void ReleaseAtlas(FontAtlas *);
	//bool UpdateAtlas(const VkCommandBuffer *);
	//FontAtlas * LoadAtlas
	Glyph * LoadGlyph(uint);
private:
	//static FT_Error FaceRequester(FTC_FaceID, FT_Library, FT_Pointer, FT_Face *);
	class CompositorInterface *pcomp;
	std::vector<Glyph> glyphMap;
	std::vector<FontAtlas> fontAtlasMap;
	//std::vector<std::pair<uint, Glyph>> glyphMap;
	//std::vector<Text *> updateQueue; //keep track in order to update descriptor sets when needed
	FT_Library library;
	//FTC_Manager fontCacheManager;
	FT_Face fontFace;
	hb_font_t *phbFont;
	//TextureStaged *pfontAtlas;
	//glm::uvec2 fontAtlasCursor; //remove
	//uint fontAtlasSize; //remove
};

}

#endif

