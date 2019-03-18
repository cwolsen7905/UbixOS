#include <string>
#include <fstream>
#include <istream>
#include <sstream>
#include <iostream>
#include <assert.h>

#include "ogImage.h"
#include "objgfx.h"

bool Win3xBitmapHeader::Deserialize(std::istream& stream)
{
	if (!stream) return false;

	// Read in each field, and check for any failure. Return false on failure
	if (!stream.read(reinterpret_cast<char *>(&ImageFileType), sizeof(ImageFileType))) return false;
	if (!stream.read(reinterpret_cast<char *>(&FileSize), sizeof(FileSize))) return false;
	if (!stream.read(reinterpret_cast<char *>(&Reserved1), sizeof(Reserved1))) return false;
	if (!stream.read(reinterpret_cast<char *>(&Reserved2), sizeof(Reserved2))) return false;
	if (!stream.read(reinterpret_cast<char *>(&ImageDataOffset), sizeof(ImageDataOffset))) return false;

	return true;
} // bool Win3xBitmapHeader::Deserialize()

std::string Win3xBitmapHeader::ToString() 
{
	return 
		"ImageFileType = " + std::to_string(ImageFileType) + "\r\n" +
		"FileSize = " + std::to_string(FileSize) + "\r\n" +
		"Reserved1 = " + std::to_string(Reserved1) + "\r\n" +
		"Reserved2 = " + std::to_string(Reserved2) + "\r\n" +
		"ImageDataOffset = " + std::to_string(ImageDataOffset) + "\r\n";
} // string Win3xBitmapHeader::ToString()

bool Win3xBitmapHeader::IsMatch() 
{
	return 
		(ImageFileType == 0x4D42 && 
		FileSize != 0 && 
		Reserved1 == 0 &&
		Reserved2 == 0 &&
		ImageDataOffset != 0);
} // bool Win3xBitmapHeader::IsMatch()

bool Win3xBitmapHeader::Serialize(std::ostream& stream)
{
	if (!stream) return false;

	// Read in each field, and check for any failure. Return false on failure
	if (!stream.write(reinterpret_cast<char *>(&ImageFileType), sizeof(ImageFileType))) return false;
	if (!stream.write(reinterpret_cast<char *>(&FileSize), sizeof(FileSize))) return false;
	if (!stream.write(reinterpret_cast<char *>(&Reserved1), sizeof(Reserved1))) return false;
	if (!stream.write(reinterpret_cast<char *>(&Reserved2), sizeof(Reserved2)))	return false;
	if (!stream.write(reinterpret_cast<char *>(&ImageDataOffset), sizeof(ImageDataOffset))) return false;

	return true;
} // bool Win3xBitmapHeader::Serialize()

size_t Win3xBitmapHeader::Size() 
{
	return
		sizeof(ImageFileType) +
		sizeof(FileSize) + 
		sizeof(Reserved1) + 
		sizeof(Reserved2) + 
		sizeof(ImageDataOffset);
} // size_t Win3xBitmapHeader::Size() 

bool Win3xBitmapInfoHeader::Deserialize(std::istream& stream)
{
	if (!stream) return false;

	// Read in each field, and check for any failure. Return false on failure
	if (!stream.read(reinterpret_cast<char *>(&HeaderSize), sizeof(HeaderSize))) return false;
	if (!stream.read(reinterpret_cast<char *>(&ImageWidth), sizeof(ImageWidth))) return false;
	if (!stream.read(reinterpret_cast<char *>(&ImageHeight), sizeof(ImageHeight))) return false;
	if (!stream.read(reinterpret_cast<char *>(&NumberOfImagePlanes), sizeof(NumberOfImagePlanes))) return false;
	if (!stream.read(reinterpret_cast<char *>(&BitsPerPixel), sizeof(BitsPerPixel))) return false;
	if (!stream.read(reinterpret_cast<char *>(&CompressionMethod), sizeof(CompressionMethod))) return false;
	if (!stream.read(reinterpret_cast<char *>(&SizeOfBitmap), sizeof(SizeOfBitmap))) return false;
	if (!stream.read(reinterpret_cast<char *>(&HorzResolution), sizeof(HorzResolution))) return false;
	if (!stream.read(reinterpret_cast<char *>(&VertResolution), sizeof(VertResolution))) return false;
	if (!stream.read(reinterpret_cast<char *>(&NumColoursUsed), sizeof(NumColoursUsed))) return false;
	if (!stream.read(reinterpret_cast<char *>(&NumSignificantColours), sizeof(NumSignificantColours))) return false;

	return true;
} // bool Win3xBitmapInfoHeader::Deserialize()

std::string Win3xBitmapInfoHeader::ToString() 
{
	return
		"HeaderSize = " + std::to_string(HeaderSize) + "\r\n" +
		"ImageWidth = " + std::to_string(ImageWidth) + "\r\n" +
		"ImageHeight = " + std::to_string(ImageHeight) + "\r\n" +
		"NumberOfImagePlanes = " + std::to_string(NumberOfImagePlanes) + "\r\n" +
		"BitsPerPixel = "+ std::to_string(BitsPerPixel) + "\r\n" +
		"CompressionMethod = " + std::to_string(CompressionMethod) + "\r\n" +
		"SizeOfBitmap = " + std::to_string(SizeOfBitmap) + "\r\n" +
		"HorzResolution = " + std::to_string(HorzResolution) + "\r\n" +
		"VertResolution = " + std::to_string(VertResolution) + "\r\n" +
		"NumColoursUsed = " + std::to_string(NumColoursUsed) + "\r\n" +
		"NumSignificantColours = " + std::to_string(NumSignificantColours) + "\r\n";
} // string Win3xBitmapInfoHeader::ToString() 

bool Win3xBitmapInfoHeader::IsMatch()
{
	return 
		(HeaderSize != 0 &&
		ImageWidth != 0 &&
		ImageHeight != 0 &&
		CompressionMethod == 0 &&
		(BitsPerPixel == 8 || BitsPerPixel == 24));
}
bool Win3xBitmapInfoHeader::Serialize(std::ostream& stream)
{
	if (!stream) return false;

	// Read in each field, and return false on failure
	if (!stream.write(reinterpret_cast<char *>(&HeaderSize), sizeof(HeaderSize))) return false;
	if (!stream.write(reinterpret_cast<char *>(&ImageWidth), sizeof(ImageWidth))) return false;
	if (!stream.write(reinterpret_cast<char *>(&ImageHeight), sizeof(ImageHeight))) return false;
	if (!stream.write(reinterpret_cast<char *>(&NumberOfImagePlanes), sizeof(NumberOfImagePlanes))) return false;
	if (!stream.write(reinterpret_cast<char *>(&BitsPerPixel), sizeof(BitsPerPixel))) return false;
	if (!stream.write(reinterpret_cast<char *>(&CompressionMethod), sizeof(CompressionMethod))) return false;
	if (!stream.write(reinterpret_cast<char *>(&SizeOfBitmap), sizeof(SizeOfBitmap))) return false;
	if (!stream.write(reinterpret_cast<char *>(&HorzResolution), sizeof(HorzResolution))) return false;
	if (!stream.write(reinterpret_cast<char *>(&VertResolution), sizeof(VertResolution))) return false;
	if (!stream.write(reinterpret_cast<char *>(&NumColoursUsed), sizeof(NumColoursUsed))) return false;
	if (!stream.write(reinterpret_cast<char *>(&NumSignificantColours), sizeof(NumSignificantColours))) return false;

	return true;
} // bool Win3xBitmapInfoHeader::Serialize()

size_t Win3xBitmapInfoHeader::Size() 
{
	return
		sizeof(HeaderSize) +
		sizeof(ImageWidth) +
		sizeof(ImageHeight) + 
		sizeof(NumberOfImagePlanes) + 
		sizeof(BitsPerPixel) +
		sizeof(CompressionMethod) + 
		sizeof(SizeOfBitmap) +
		sizeof(HorzResolution) +
		sizeof(VertResolution) +
		sizeof(NumColoursUsed) +
		sizeof(NumSignificantColours);
} // size_t Win3xBitmapInfoHeader::Size() 

bool RGBQuad::Deserialize(std::istream& stream)
{
	if (!stream) return false;

	// Read in each field, and return false on failure
	if (!stream.read(reinterpret_cast<char *>(&rgbBlue), sizeof(rgbBlue))) return false;
	if (!stream.read(reinterpret_cast<char *>(&rgbGreen), sizeof(rgbGreen))) return false;
	if (!stream.read(reinterpret_cast<char *>(&rgbRed), sizeof(rgbRed))) return false;
	if (!stream.read(reinterpret_cast<char *>(&rgbReserved), sizeof(rgbReserved))) return false;

	return true;
} // bool RGBQuad::Deserialize()

std::string RGBQuad::ToString()
{
	return "rgbRed = " + to_string(rgbRed) + "\r\n"
		+ "rgbGreen = " + to_string(rgbGreen) + "\r\n"
		+ "rgbBlue = " + to_string(rgbBlue) + "\r\n"
		+ "rgbReserved = " + to_string(rgbReserved) + "\r\n";
} // string RGBQuad::ToString()

bool RGBQuad::Serialize(std::ostream& stream)
{
	if (!stream) return false;

	// Read in each field, and return false on failure
	if (!stream.write(reinterpret_cast<char *>(&rgbBlue), sizeof(rgbBlue))) return false;
	if (!stream.write(reinterpret_cast<char *>(&rgbGreen), sizeof(rgbGreen))) return false;
	if (!stream.write(reinterpret_cast<char *>(&rgbRed), sizeof(rgbRed))) return false;
	if (!stream.write(reinterpret_cast<char *>(&rgbReserved), sizeof(rgbReserved))) return false;

	return true;
} // bool RGBQuad::Serialize()
size_t RGBQuad::Size()
{
	return 
		sizeof(rgbBlue) +
		sizeof(rgbGreen) + 
		sizeof(rgbRed) +
		sizeof(rgbReserved);
} // size_t RGBQuad::Size()

bool IsBMP(std::istream& stream)
{

	// Check for BMP
	Win3xBitmapHeader bmpHeader;
	bmpHeader.Deserialize(stream);
	Win3xBitmapInfoHeader bmpInfoHeader;
	bmpInfoHeader.Deserialize(stream);

	if (bmpHeader.IsMatch() && bmpInfoHeader.IsMatch()) 
	{
		std::cout << bmpHeader.ToString();
		std::cout << bmpInfoHeader.ToString();
		return true;
	}
	return false;
} // bool IsBMP()

/**********************************************
 *			ogImage 
 **********************************************/
static const std::map<ogImageType, std::function<bool(std::istream&)>>& CreateIsImageMap()
{
	static std::map<ogImageType, std::function<bool(std::istream&)>> IsImage;

	//IsImage[NoImage] = ([&](std::istream&) -> bool { return false; });
	IsImage[BMP] = &IsBMP;
	return IsImage;
} // map<> CreateIsImageMap()

std::map<ogImageType, std::function<bool(std::istream&)>> ogImage::IsImage = CreateIsImageMap();

ogImage::ogImage() :
	surface(nullptr),
	options(nullptr),
	output(nullptr),
	input(nullptr)
{
	Decode[NoImage] = &ogImage::NoOp;
	Decode[BMP] = &ogImage::DecodeBMP;

	Encode[NoImage] = &ogImage::NoOp;
	Encode[BMP] = &ogImage::EncodeBMP;

} // ogImage::ogImage()

bool ogImage::DecodeBMP()
{
	// ogImage::ImageType() has determined we're a BMP, so we only need to do
	// minimal sanity checks on the header information.

	Win3xBitmapHeader bmpHeader;
	Win3xBitmapInfoHeader bmpInfoHeader;
	if (!bmpHeader.Deserialize(*input)) {
 std::cout << "!bmpHeader" << std::endl;
        return false;
}
	if (!bmpInfoHeader.Deserialize(*input)) {
 std::cout << "!bmpInfoHeader" << std::endl;
return false;
}

	size_t lineSize;
	size_t paddington;
	char linePadding[4];

std::cout <<"DecodeBMP" << std::endl;
	
	if (bmpInfoHeader.BitsPerPixel == 8)
	{
		if (!surface->ogCreate(bmpInfoHeader.ImageWidth, bmpInfoHeader.ImageHeight, OG_PIXFMT_8BPP)) return false;

		lineSize = ((bmpInfoHeader.ImageWidth+3) >> 2) << 2;	// round up to the nearest 4 bytes
		paddington = lineSize - bmpInfoHeader.ImageWidth;		// see if we have a remainder
		
		if (paddington > 4) return false;						// this would be odd

		RGBQuad quad;
		for (unsigned colour = 0; colour < 256; colour++)
		{
			if (!quad.Deserialize(*input)) return false;
			surface->ogSetPalette(colour, quad.rgbRed, quad.rgbGreen, quad.rgbBlue, quad.rgbReserved);
		} // for colour

		for (unsigned y = surface->ogGetMaxY()+1; y --> 0 ;) // y goes to 0
		{
			char * ptr = reinterpret_cast<char *>(surface->ogGetPtr(0, y));
			if (ptr == nullptr) return false;	// this doesn't have to be a complete failure, fix later
			if (!input->read(ptr, bmpInfoHeader.ImageWidth)) return false;
			if (paddington != 0)
			{
				if (!input->read(linePadding, paddington)) return false;	// double check this
			}
		} // for y
	} 
	else if (bmpInfoHeader.BitsPerPixel == 24)
	{
		ogPixelFmt BMPPixelFmt(24, 16,8,0,0,  8,8,8,0);
		if (!surface->ogCreate(bmpInfoHeader.ImageWidth, bmpInfoHeader.ImageHeight, OG_PIXFMT_24BPP)) return false;	
		size_t widthInBytes = bmpInfoHeader.ImageWidth*3;	// 3 represents how many bytes per pixel
		lineSize = ((widthInBytes+3) >> 2) << 2;			// round up to the nearest 4 bytes
		paddington = lineSize - widthInBytes;		// see if we have a remainder

		if (paddington > 4) return false;

		for (unsigned y = surface->ogGetMaxY()+1; y --> 0 ;)
		{
			char * ptr = reinterpret_cast<char *>(surface->ogGetPtr(0, y));
			if (ptr == nullptr) return false;	// this doesn't have to be a complete failure, fix later
			if (!input->read(ptr, widthInBytes)) return false;
			if (paddington != 0)
			{
				if (!input->read(linePadding, paddington)) return false;
			} 
		} // for y
	}

	return true;
} // bool ogImage::DecodeBMP()

bool ogImage::EncodeBMP()
{
	Win3xBitmapHeader bmpHeader;
	Win3xBitmapInfoHeader bmpInfoHeader;
	size_t paddington;
	size_t lineSize;
	const char linePadding[4] = {0,0,0,0};
	ogRGBA8 ogPalette[256]; // used to get the palette from the surface

	bmpHeader.ImageFileType = 0x4D42;
	bmpHeader.FileSize = 0; // fill in later
	//bmpHeader.Reserved1 = 0;	// set by constructor
	//bmpHeader.Reserved2 = 0;	// set by constructor
	bmpHeader.ImageDataOffset = bmpHeader.Size() + bmpInfoHeader.Size();

	bmpInfoHeader.HeaderSize = bmpInfoHeader.Size();
	bmpInfoHeader.ImageWidth = surface->ogGetMaxX()+1;
	bmpInfoHeader.ImageHeight = surface->ogGetMaxY()+1;
	bmpInfoHeader.NumberOfImagePlanes = 1;
	// bmpInfoHeader.BitsPerPixel is set below
	bmpInfoHeader.CompressionMethod = 0;
	bmpInfoHeader.SizeOfBitmap = 0;
	bmpInfoHeader.HorzResolution = 0;	// option
	bmpInfoHeader.VertResolution = 0;	// option

	switch (surface->ogGetBPP())
	{
	case 8:
		bmpInfoHeader.BitsPerPixel = 8;	// 8bpp, 256 colours
		bmpInfoHeader.NumColoursUsed = 1 << surface->ogGetBPP();
		bmpInfoHeader.NumSignificantColours = 0; // option

		// This is hard to calculate dynamically. Sod it.
		bmpHeader.ImageDataOffset += 1024;// adjust by palette size

		lineSize = ((bmpInfoHeader.ImageWidth+3) >> 2) << 2;	// multiple of 4
		bmpInfoHeader.SizeOfBitmap = lineSize*bmpInfoHeader.ImageHeight;
		bmpHeader.FileSize = bmpHeader.ImageDataOffset+bmpInfoHeader.SizeOfBitmap;

		// Write the headers
		if (!bmpHeader.Serialize(*output)) return false;
		if (!bmpInfoHeader.Serialize(*output)) return false;

		// all rows are aligned to the nearest 4 bytes. Figure out if we need to pad.
		paddington = lineSize-bmpInfoHeader.ImageWidth;

		surface->ogGetPalette(ogPalette);

		for (size_t index = 0; index < sizeof(ogPalette) / sizeof(ogPalette[0]); index++)
		{
			RGBQuad quad(ogPalette[index]);
			if (!quad.Serialize(*output)) return false;
		} // for index

		for (unsigned y = surface->ogGetMaxY()+1; y --> 0 ;) // y goes to 0
		{
			char * ptr = reinterpret_cast<char *>(surface->ogGetPtr(0, y));
			if (ptr != nullptr)
			{
				if (!output->write(ptr, bmpInfoHeader.ImageWidth)) return false;
			}

			// Is there any padding to add to the end of the line?
			if (paddington != 0) 
			{
				if (!output->write(linePadding, paddington)) return false;
			}
		} // for y
		break;
	case 32:
	case 24:
	case 16:
	case 15:
		// 15, 16, and 32 bpp are all treated as 24bpp
		bmpInfoHeader.BitsPerPixel = 24;
		bmpInfoHeader.NumColoursUsed = 0;
		bmpInfoHeader.NumSignificantColours = 0;

		lineSize = ((bmpInfoHeader.ImageWidth*3+3) >> 2) << 2;
		bmpInfoHeader.SizeOfBitmap = lineSize*bmpInfoHeader.ImageHeight;
		bmpHeader.FileSize = bmpHeader.ImageDataOffset+bmpInfoHeader.SizeOfBitmap;

		// Write the headers
		if (!bmpHeader.Serialize(*output)) return false;
		if (!bmpInfoHeader.Serialize(*output)) return false;

		// all rows are aligned to the nearest 4 bytes. Figure out if we need to pad.
		paddington = lineSize-bmpInfoHeader.ImageWidth*3;	// 3 represents how many bytes per pixel
		uInt8 red, green, blue;

		for (unsigned y = surface->ogGetMaxY()+1; y --> 0 ;) // y goes to 0
		{
			for (unsigned x = 0; x <= surface->ogGetMaxX(); x++)
			{
				// Unpack the pixel and write it out.
				surface->ogUnpack(surface->ogGetPixel(x, y), red, green, blue);
				if (!output->write(reinterpret_cast<char *>(&blue), sizeof(blue))) return false;
				if (!output->write(reinterpret_cast<char *>(&green), sizeof(green))) return false;
				if (!output->write(reinterpret_cast<char *>(&red), sizeof(red))) return false;
			} // for x

			// Is there any padding to add to the end of the line?
			if (paddington != 0) 
			{
				if (!output->write(linePadding, paddington)) return false;
			}
		} // for y
		break;
	default:
		return false; // we can't encode anything else (I'm not sure there *is* anything else)
	} // switch


	return true;
}  // bool ogImage::EncodeBMP()

ogImageType ogImage::ImageType(std::istream& input)
{
	ogImageType imageType = NoImage;
	// space for the header
	char header[128]; // This really should be as large as the largest header we know of

	for (size_t index = 0; index < std::extent<decltype(header)>::value; index++)
	{
		header[index] = 0;	// clear the header
	} // for index

	size_t size = sizeof(header);

	if (!input.read(header, size)) return NoImage;	// This isn't necessarily true. Might just be a small image.
	// Figure out how many bytes we read in. 
	streamsize bytesRead = input.gcount();

	if (bytesRead != 0) 
	{
		
		// Try to determine what it really is
		for (auto iType : IsImage)
		{
			mstream mb(header, bytesRead);
			// This creates a new istream from a memory stream,
			// which is wrapped adound the header.
			//istream stream(&mstream(header, bytesRead));
            istream stream(&mb);
			if (iType.second(stream))	// Did we find it?
			{
				imageType = iType.first;	// Found it
				break;
			} // if
		} // for iType
std::cout << "bRead: " << bytesRead << std::endl;
		// Now seek backwards in the stream.
		input.seekg(-bytesRead, std::ios_base::cur);
	}
	
	return imageType;
} // ogImageType ogImage::ImageType()

ogImageType ogImage::ImageType(const std::string& filename)
{
	std::ifstream input(filename, ios::in | std::ios_base::binary);
	ogImageType imageType = NoImage;

	// Sanity check
	if (input)
	{
		// Use the stream version
		imageType = ogImage::ImageType(input);

		// close the file
		input.close();
	} // if

	return imageType;
} // ogImageType ogImage::ImageType()

bool ogImage::Load(const std::string& filename, ogSurface & surface)
{
    std::cout << "Loading " << filename << std::endl;
	bool success = false;	// assume failure

	ifstream stream(filename, ios_base::in | ios_base::binary);

std::cout << "Loaded" << std::endl;

	if (stream)
	{
		success = Load(stream, surface);
		stream.close();
	} // if stream

	return success;
} // ogSurface * ogImage::Load()

bool ogImage::Load(std::istream& stream, ogSurface& surface)
{
	// First we need to find out what type of graphics file it is:
std::cout << "eIT" << std::endl;
	ogImageType imageType = ogImage::ImageType(stream);
std::cout << "rIT" << std::endl;

	if (imageType == NoImage) return false;

	this->surface = &surface;
	this->input = &stream;
	this->options = nullptr;	// shouldn't be necessary
	this->output = nullptr;		// shouldn't be necessary

	assert(Decode.count(imageType) == 1);	// make sure we can handle it

        std::cout << "Load-istream2" << std::endl;

	return (this->*Decode[imageType])();	// Decode
} // bool ogImage::Load()

bool ogImage::Save(const std::string& filename, 
				   ogSurface &surface, 
				   ogImageType imageType, 
				   ogImageOptions * options)
{
	bool success = false;	// assume failure

	ofstream stream(filename, ios_base::out | ios_base::binary);	// try to open the file

	if (stream) 
	{
		// Use the stream version
		success = Save(stream, surface, imageType, options);

		// close the file
		stream.close();	
	} // if

	return success;
} // bool ogImage::Save()

bool ogImage::Save(std::ostream& stream,
				   ogSurface &surface, 
				   ogImageType imageType, 
				   ogImageOptions * options)
{
	bool success = false;	// assume failure

	if (stream && surface.ogAvail())
	{
		// Since surface is a reference, it cannot [normally] be null,
		// so no checking needs to be done in any of the encoder functions.
		this->surface = &surface;
		this->options = options;
		this->output = &stream;

		assert(Encode.count(imageType) == 1);	// Sanity check

		// Call the specific encoder
		success = (this->*Encode[imageType])();
	} // if

	return success;
} // bool ogImage::Save()

#if 0
class membuf : public basic_streambuf<char>
{
public:
	membuf(char* p, size_t n) {
		setg(p, p, p + n);
		setp(p, p + n);
	}
}
Usage:

char *mybuffer;
size_t length;
// ... allocate "mybuffer", put data into it, set "length"

membuf mb(mybuffer, length);
istream reader(&mb);
// use "reader"
#endif
