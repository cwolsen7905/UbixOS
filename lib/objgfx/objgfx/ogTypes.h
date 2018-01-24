// Object Graphics
// Mark Iuzzolino
// mark.iuzzolino@gmail.com
// ogTypes.h

#pragma once

#include <map>

#define ogVERSION 1.0

typedef int8_t            int8;
typedef int16_t       int16;
typedef int32_t       int32;
typedef int64_t   int64;
typedef uint8_t          uInt8;
typedef uint16_t     uInt16;
typedef uint32_t           uInt32;
typedef uint64_t uInt64;

enum ogDataState { ogNone, ogOwner, ogAliasing };

enum ogErrorCode 
{
	ogOK,
	ogMemAllocFail,
	ogAlreadyOwner,
	ogNoSurface,
	ogNoPalette,
	ogBadBPP,
	ogSourceOutOfBounds,
	ogDestOutOfBounds,
	ogFileNotFound,
	ogFileReadError,
	ogFileWriteError,
	ogNoCloning,
	ogNoAliasing,
	ogNoModeSupport
}; // enum ogErrorCode

//static std::map<ogErrorCode, std::string> ogErrorCodes = SetupErrorCodes();

struct ogRGB8 
{
	uInt8 red;
	uInt8 green;
	uInt8 blue;
};

struct ogRGBA8 
{
	uInt8 red;
	uInt8 green;
	uInt8 blue;
	uInt8 alpha;
};

struct ogRGB16 
{
	uInt16 red;
	uInt16 blue;
	uInt16 green;
};

struct ogRGBA16 
{
	uInt16 red;
	uInt16 green;
	uInt16 blue;
	uInt16 alpha;
};

struct ogRGB32 
{
	uInt32 red;
	uInt32 green;
	uInt32 blue;
};

struct ogRGBA32 
{
	uInt32 red;
	uInt32 green;
	uInt32 blue;
	uInt32 alpha;
};

struct ogPoint2d 
{
	int32 x;
	int32 y;
};

struct ogPoint3d 
{
	int32 x;
	int32 y;
	int32 z;
};

struct ogAttribute {
	uInt32 transparentColor;
	uInt32 defaultAlpha;
	bool antiAlias;
	bool blending;

	ogAttribute(
		uInt32 transparentColor = 0,
		uInt32 defaultAlpha = 255,
		bool antiAlias = true,
		bool blending = false)
	{ 
		this->transparentColor = transparentColor;
		this->defaultAlpha = defaultAlpha;
		this->antiAlias = antiAlias;
		this->blending = blending;
	} // ogAttribute()

}; // struct ogAttribute

