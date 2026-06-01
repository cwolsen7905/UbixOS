// (C) 2002-2026 The UbixOS Project
#pragma once

#include "objgfx.h"

enum ogImageType
{
	NoImage,
	BMP
};

union ogImageOptions
{
};

// Raw BMP on-disk structs (packed, little-endian).  The packed attribute is
// required: the 14-byte file header has a uInt32 at offset 2, so without it the
// compiler pads the struct to 16 bytes and fd_read() of sizeof() misaligns the
// whole file (which silently broke ogImage::Load on every BMP).
struct __attribute__((packed)) Win3xBitmapHeader
{
	uInt16 ImageFileType;
	uInt32 FileSize;
	uInt16 Reserved1;
	uInt16 Reserved2;
	uInt32 ImageDataOffset;
	Win3xBitmapHeader() : ImageFileType(0), FileSize(0), Reserved1(0), Reserved2(0), ImageDataOffset(0)
	{
	}
	bool IsMatch() const
	{
		return (ImageFileType == 0x4D42 && FileSize != 0 && Reserved1 == 0 && Reserved2 == 0 &&
		        ImageDataOffset != 0);
	}
};

struct __attribute__((packed)) Win3xBitmapInfoHeader
{
	uInt32 HeaderSize;
	uInt32 ImageWidth;
	uInt32 ImageHeight;
	uInt16 NumberOfImagePlanes;
	uInt16 BitsPerPixel;
	uInt32 CompressionMethod;
	uInt32 SizeOfBitmap;
	uInt32 HorzResolution;
	uInt32 VertResolution;
	uInt32 NumColoursUsed;
	uInt32 NumSignificantColours;
	Win3xBitmapInfoHeader()
	    : HeaderSize(0), ImageWidth(0), ImageHeight(0), NumberOfImagePlanes(0), BitsPerPixel(0),
	      CompressionMethod(0), SizeOfBitmap(0), HorzResolution(0), VertResolution(0), NumColoursUsed(0),
	      NumSignificantColours(0)
	{
	}
	bool IsMatch() const
	{
		return (HeaderSize != 0 && ImageWidth != 0 && ImageHeight != 0 && CompressionMethod == 0 &&
		        (BitsPerPixel == 8 || BitsPerPixel == 24));
	}
};

struct RGBQuad
{
	uInt8 rgbBlue;
	uInt8 rgbGreen;
	uInt8 rgbRed;
	uInt8 rgbReserved;
	RGBQuad() : rgbBlue(0), rgbGreen(0), rgbRed(0), rgbReserved(0)
	{
	}
	RGBQuad(ogRGBA8 e) : rgbBlue(e.blue), rgbGreen(e.green), rgbRed(e.red), rgbReserved(0)
	{
	}
};

class ogImage
{
      public:
	ogImage()
	{
	}
	bool Load(const char *filename, ogSurface &surface);
	bool Save(const char *filename, ogSurface &surface, ogImageType imageType, ogImageOptions *options = 0);

      private:
	bool DecodeBMP(int fd, ogSurface &surface);
	bool DecodePNG(int fd, ogSurface &surface);
	bool EncodeBMP(int fd, ogSurface &surface);
};
