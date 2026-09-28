#include "stdafx.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "Font.h"

#include <fstream>
#include <iostream>
#include <string>

bool Font::Load(const char* filename, float pixelHeight)
{
	// Same search as shader loading: the working directory differs between Visual Studio and the built exe.
	const char* prefixes[] = { "", "./", "../SimpleGame/", "./SimpleGame/", "../../SimpleGame/" };
	for (int i = 0; i < 5 && data.empty(); ++i)
	{
		std::ifstream file((std::string(prefixes[i]) + filename).c_str(), std::ios::binary);
		if (!file) continue;
		data.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
	}

	if (data.empty() || !stbtt_InitFont(&info, &data[0], stbtt_GetFontOffsetForIndex(&data[0], 0)))
	{
		std::cout << "Font " << filename << " could not be loaded.\n";
		data.clear();
		return false;
	}

	scale = stbtt_ScaleForPixelHeight(&info, pixelHeight);

	std::vector<unsigned char> blank(kAtlasSize * kAtlasSize, 0);
	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, kAtlasSize, kAtlasSize, 0, GL_RED, GL_UNSIGNED_BYTE, &blank[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	std::cout << "Font " << filename << " loaded at " << pixelHeight << "px.\n";
	return true;
}

const Glyph& Font::Get(unsigned int codepoint)
{
	std::unordered_map<unsigned int, Glyph>::iterator found = glyphs.find(codepoint);
	if (found != glyphs.end()) return found->second;

	Glyph& glyph = glyphs[codepoint];
	if (!Rasterize(codepoint, glyph) && codepoint != '?')
		glyph = Get('?');
	return glyph;
}

bool Font::Rasterize(unsigned int codepoint, Glyph& glyph)
{
	if (texture == 0) return false;

	int index = stbtt_FindGlyphIndex(&info, (int)codepoint);
	if (index == 0 && codepoint != ' ') return false;

	int advance, bearing;
	stbtt_GetGlyphHMetrics(&info, index, &advance, &bearing);
	glyph.advance = (float)advance * scale;

	int x0, y0, x1, y1;
	stbtt_GetGlyphBitmapBox(&info, index, scale, scale, &x0, &y0, &x1, &y1);
	int w = x1 - x0;
	int h = y1 - y0;
	if (w <= 0 || h <= 0) return true;

	// Shelf packing with a pixel of air so linear filtering never bleeds a neighbour in.
	if (penX + w + 1 > kAtlasSize)
	{
		penX = 1;
		penY += rowHeight + 1;
		rowHeight = 0;
	}
	if (penY + h + 1 > kAtlasSize)
	{
		if (!full) std::cout << "Font atlas is full; new glyphs are skipped.\n";
		full = true;
		return true;
	}

	std::vector<unsigned char> bitmap(w * h);
	stbtt_MakeGlyphBitmap(&info, &bitmap[0], w, h, w, scale, scale, index);

	glBindTexture(GL_TEXTURE_2D, texture);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexSubImage2D(GL_TEXTURE_2D, 0, penX, penY, w, h, GL_RED, GL_UNSIGNED_BYTE, &bitmap[0]);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glBindTexture(GL_TEXTURE_2D, 0);

	const float texel = 1.0f / (float)kAtlasSize;
	glyph.u0 = (float)penX * texel;
	glyph.v0 = (float)penY * texel;
	glyph.u1 = (float)(penX + w) * texel;
	glyph.v1 = (float)(penY + h) * texel;
	glyph.x0 = x0;
	glyph.y0 = y0;
	glyph.w = w;
	glyph.h = h;

	penX += w + 1;
	if (h > rowHeight) rowHeight = h;
	return true;
}

unsigned int NextCodepoint(const char*& text)
{
	const unsigned char* s = (const unsigned char*)text;
	unsigned int c = s[0];
	int extra = 0;

	if ((c & 0xE0) == 0xC0) extra = 1;
	else if ((c & 0xF0) == 0xE0) extra = 2;
	else if ((c & 0xF8) == 0xF0) extra = 3;
	else if (c >= 0x80)
	{
		++text;
		return '?';
	}
	c &= 0x7F >> extra;

	for (int i = 1; i <= extra; ++i)
	{
		if ((s[i] & 0xC0) != 0x80)
		{
			text += i;
			return '?';
		}
		c = (c << 6) | (s[i] & 0x3F);
	}

	text += extra + 1;
	return c;
}
