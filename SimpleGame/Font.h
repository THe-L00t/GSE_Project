#pragma once

#include <unordered_map>
#include <vector>

#include "Dependencies\glew.h"
#include "Dependencies\stb_truetype.h"

struct Glyph
{
	float u0 = 0.0f;
	float v0 = 0.0f;
	float u1 = 0.0f;
	float v1 = 0.0f;
	int   x0 = 0;          // bitmap offset from the pen on the baseline
	int   y0 = 0;
	int   w = 0;
	int   h = 0;
	float advance = 0.0f;
};

// Glyphs are rasterised into the atlas the first time they are asked for,
// so the eleven thousand Hangul syllables never need a pre-baked table.
class Font
{
public:
	bool Load(const char* filename, float pixelHeight);
	bool IsLoaded() const { return texture != 0; }

	const Glyph& Get(unsigned int codepoint);
	GLuint Texture() const { return texture; }

private:
	bool Rasterize(unsigned int codepoint, Glyph& glyph);

	static const int kAtlasSize = 1024;

	std::vector<unsigned char> data;
	stbtt_fontinfo info = {};
	float  scale = 1.0f;
	GLuint texture = 0;
	int    penX = 1;
	int    penY = 1;
	int    rowHeight = 0;
	bool   full = false;

	std::unordered_map<unsigned int, Glyph> glyphs;
};

// Decodes one UTF-8 sequence and advances text past it. Malformed bytes decode as '?'.
unsigned int NextCodepoint(const char*& text);
