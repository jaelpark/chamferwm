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
	void Draw(const glm::uvec2 &, const glm::mat2x2 &, const VkCommandBuffer *);
	void UpdateDescSets();
	float GetTextLength() const;
	float GetBaseline() const;
protected:
	hb_buffer_t *phbBuf;
	uint glyphCount;
	float textLength;
	class TextEngine *ptextEngine;
	class Buffer *pvertexBuffer;
	class Buffer *pindexBuffer;
	class FontAtlas *pfontAtlas;
	struct Vertex{
		glm::vec2 pos;
		glm::uvec2 texc;
	};// alignas(16);
	static std::vector<std::pair<ShaderModule::INPUT, uint>> vertexBufferLayout;
};

class TextEngine{
friend class FontAtlas;
friend class Text;
friend class CompositorInterface;
public:
	TextEngine(const char *, uint, class CompositorInterface *);
	~TextEngine();
	struct Glyph{
		uint codepoint;
		uint w;
		uint h;
		uint pitch;
		glm::vec2 offset;
		unsigned char *pbuffer;
	};
	FontAtlas * CreateAtlas(hb_glyph_info_t *, uint, const VkCommandBuffer *);
	void ReleaseAtlas(FontAtlas *);
	Glyph * LoadGlyph(uint);
	float GetFontSize() const;
private:
	class CompositorInterface *pcomp;
	std::vector<Glyph> glyphMap;
	std::vector<class FontAtlas *> fontAtlasMap;
	FT_Library library;
	FT_Face fontFace;
	hb_font_t *phbFont;
};

class FontAtlas{
public:
	FontAtlas(uint, class TextEngine *);
	~FontAtlas();
	void Update(hb_glyph_info_t *, uint, const VkCommandBuffer *);
	TextEngine *ptextEngine;
	TextureStaged *ptexture;
	struct GlyphEntry{
		uint codepoint;
		glm::uvec2 texc;
		TextEngine::Glyph *pglyph;
	};
	//std::vector<std::pair<uint, glm::uvec2>> glyphCollection;
	std::vector<GlyphEntry> glyphCollection;
	glm::uvec2 fontAtlasCursor; //this is useless since Map() for the full region erases everything
	uint size;
	uint refCount;
	uint64 releaseTag;
};

}

#endif

