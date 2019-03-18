// ogImage.h

#pragma once
#include "objgfx40.h"


class mstream : public std::basic_streambuf<char>
{
public:
	mstream(char* p, std::streamsize n) {
		setg(p, p, p + n);
		setp(p, p + n);
	} // mStream::mStream()
};	// class mstream

class ogHeaderAbstract
{
public:
	virtual bool Deserialize(std::istream&) = 0;
	virtual bool Serialize(std::ostream&) = 0;
	virtual bool IsMatch() { return false; }
	virtual std::string ToString() { return ""; }
	virtual size_t Size() = 0;
	virtual ~ogHeaderAbstract() {};
}; // class ogHeader

class Win3xBitmapHeader : public ogHeaderAbstract
{
public:
	uInt16	ImageFileType;		// Image file type, always 4D42h ("BM")
	uInt32	FileSize;			// Physical size in bytes
	uInt16	Reserved1;			// Always 0
	uInt16	Reserved2;			// Always 0
	uInt32	ImageDataOffset;	// Start of image data offset in bytes
	Win3xBitmapHeader() : 
		ImageFileType(0), 
		FileSize(0), 
		Reserved1(0), 
		Reserved2(0), 
		ImageDataOffset(0) {}
	bool Deserialize(std::istream&);
	std::string ToString();

	bool IsMatch();
	bool Serialize(std::ostream&);
	size_t Size();
}; // class Win3xBitmapHeader

class RGBQuad : public ogHeaderAbstract
{
public:
	uInt8 rgbBlue;
	uInt8 rgbGreen;
	uInt8 rgbRed;
	uInt8 rgbReserved;
	RGBQuad() :
		rgbBlue(0),
		rgbGreen(0),
		rgbRed(0),
		rgbReserved(0) {};
	RGBQuad(ogRGBA8 paletteEntry) :
		rgbBlue(paletteEntry.blue),
		rgbGreen(paletteEntry.green),
		rgbRed(paletteEntry.red),
		rgbReserved(0) {};
	bool Deserialize(std::istream&);
	bool Serialize(std::ostream&);
	size_t Size();
	std::string ToString();
}; // class RGBQuad

class Win3xBitmapInfoHeader : public ogHeaderAbstract
{
public:
	uInt32	HeaderSize;		// Size of this header
	uInt32	ImageWidth;		// Image width in pixels
	uInt32	ImageHeight;	// Image height in pixels
	uInt16	NumberOfImagePlanes; // Number of planes (always 1)
	uInt16	BitsPerPixel;	// Bits per pixel (1, 4, 8, or 24)
	uInt32	CompressionMethod;	// Compression method used (0, 1, or 2)
	uInt32	SizeOfBitmap;	// Size of the bitmap in bytes
	uInt32	HorzResolution;	// Horizontal resolution in pixels per meter 
	uInt32  VertResolution; // Vertical resolution in pixels per meter
	uInt32  NumColoursUsed;	// Number of colours in the image
	uInt32	NumSignificantColours; // Number of important colours in palette
	Win3xBitmapInfoHeader() : 
		HeaderSize(0),
		ImageWidth(0),
		ImageHeight(0),
		NumberOfImagePlanes(0),
		BitsPerPixel(0),
		CompressionMethod(0),
		SizeOfBitmap(0),
		HorzResolution(0),
		VertResolution(0),
		NumColoursUsed(0),
		NumSignificantColours(0) { };
	bool Deserialize(std::istream&);
	std::string ToString();
	bool IsMatch();
	bool Serialize(std::ostream&);
	size_t Size();
}; // class Win3xBitmapInfoHeader

union ogImageOptions
{
};

enum ogImageType { NoImage, BMP };

class ogImage 
{
private:
	static std::map<ogImageType, std::function<bool(std::istream&)> > IsImage;	
protected:
	ogSurface * surface;
	ogImageOptions * options;
	std::ostream * output;
	std::istream * input;

	std::map<ogImageType, bool (ogImage::*)(void)> Decode;
	std::map<ogImageType, bool (ogImage::*)(void)> Encode;

	bool NoOp() { return false; }

	bool DecodeBMP();

	bool EncodeBMP();

public:
	static ogImageType ImageType(std::istream&);
	static ogImageType ImageType(const std::string& filename);
	ogImage();
	bool Load(const std::string& filename, ogSurface & surface);
	bool Load(std::istream&, ogSurface&);
	bool Save(const std::string& filename, ogSurface& surface, ogImageType, ogImageOptions * options = NULL);
	bool Save(std::ostream&, ogSurface&, ogImageType, ogImageOptions * options = NULL);
}; // class ogImage


