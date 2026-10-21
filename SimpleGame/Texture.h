#pragma once

#include <string>
#include <vector>

#include "Dependencies\glew.h"

// Finds a data file the way shaders are found: the working directory differs between
// running from Visual Studio and running the built exe.
bool FindDataFile(const std::string& relative, std::string& found);

// PNG and JPEG through the Windows Imaging Component, as top-down RGBA8 rows.
bool LoadImageRGBA(const std::string& path, std::vector<unsigned char>& pixels, int& width, int& height);

// Mipmapped, repeating, read as-is: colours are display values like the rest of the palette.
GLuint LoadTexture2D(const std::string& relative);

// Every layer must share one size. Returns 0 if any layer is missing.
GLuint LoadTextureArray(const std::vector<std::string>& relatives);

// A 1x1 texture of one colour, standing in for a missing map.
GLuint SolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a);
