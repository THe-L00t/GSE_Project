#include "stdafx.h"

#define NOMINMAX
#include <windows.h>
#include <wincodec.h>

#include "Texture.h"

#include <fstream>
#include <iostream>

#pragma comment(lib, "windowscodecs.lib")

namespace
{
	IWICImagingFactory* Factory()
	{
		static IWICImagingFactory* factory = nullptr;
		static bool tried = false;
		if (tried) return factory;
		tried = true;

		// Already initialised in another mode still leaves COM usable on this thread.
		CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
			factory = nullptr;
		return factory;
	}

	std::wstring Widen(const std::string& s)
	{
		int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(len > 0 ? len : 1, L'\0');
		if (len > 0) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], len);
		return w;
	}

	void SetSampling(GLenum target)
	{
		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_REPEAT);
		// The quarter view looks at the ground at a slant; a little anisotropy keeps it from blurring.
		if (GLEW_EXT_texture_filter_anisotropic)
			glTexParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4.0f);
	}
}

bool FindDataFile(const std::string& relative, std::string& found)
{
	const char* prefixes[] = { "", "./", "../SimpleGame/", "./SimpleGame/", "../../SimpleGame/" };
	for (int i = 0; i < 5; ++i)
	{
		std::string path = std::string(prefixes[i]) + relative;
		std::ifstream file(path.c_str(), std::ios::binary);
		if (file.good())
		{
			found = path;
			return true;
		}
	}
	return false;
}

bool LoadImageRGBA(const std::string& path, std::vector<unsigned char>& pixels, int& width, int& height)
{
	IWICImagingFactory* factory = Factory();
	if (factory == nullptr) return false;

	IWICBitmapDecoder* decoder = nullptr;
	IWICBitmapFrameDecode* frame = nullptr;
	IWICFormatConverter* converter = nullptr;
	bool ok = false;

	std::wstring wide = Widen(path);
	if (SUCCEEDED(factory->CreateDecoderFromFilename(wide.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) &&
		SUCCEEDED(decoder->GetFrame(0, &frame)) &&
		SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
		SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom)))
	{
		UINT w = 0, h = 0;
		converter->GetSize(&w, &h);
		pixels.resize((size_t)w * h * 4);
		if (w > 0 && h > 0 && SUCCEEDED(converter->CopyPixels(nullptr, w * 4, (UINT)pixels.size(), &pixels[0])))
		{
			width = (int)w;
			height = (int)h;
			ok = true;
		}
	}

	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	return ok;
}

GLuint LoadTexture2D(const std::string& relative)
{
	std::string path;
	std::vector<unsigned char> pixels;
	int w = 0, h = 0;
	if (!FindDataFile(relative, path) || !LoadImageRGBA(path, pixels, w, h))
	{
		std::cout << "Texture " << relative << " not found.\n";
		return 0;
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
	glGenerateMipmap(GL_TEXTURE_2D);
	SetSampling(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

GLuint SolidTexture(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
	const unsigned char pixel[4] = { r, g, b, a };
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

GLuint LoadTextureArray(const std::vector<std::string>& relatives)
{
	if (relatives.empty()) return 0;

	std::vector<std::vector<unsigned char>> layers(relatives.size());
	int w = 0, h = 0;
	for (size_t i = 0; i < relatives.size(); ++i)
	{
		std::string path;
		int lw = 0, lh = 0;
		if (!FindDataFile(relatives[i], path) || !LoadImageRGBA(path, layers[i], lw, lh))
		{
			std::cout << "Texture " << relatives[i] << " not found.\n";
			return 0;
		}
		if (i == 0)
		{
			w = lw;
			h = lh;
		}
		else if (lw != w || lh != h)
		{
			std::cout << "Texture " << relatives[i] << " differs in size from its array.\n";
			return 0;
		}
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D_ARRAY, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, w, h, (GLsizei)layers.size(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
	for (size_t i = 0; i < layers.size(); ++i)
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, (GLint)i, w, h, 1, GL_RGBA, GL_UNSIGNED_BYTE, &layers[i][0]);
	glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	SetSampling(GL_TEXTURE_2D_ARRAY);
	glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
	return tex;
}
