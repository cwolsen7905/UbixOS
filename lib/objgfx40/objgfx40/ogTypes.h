#ifndef OGTYPES_H
#define OGTYPES_H

typedef signed char            int8;
typedef signed short int       int16;
typedef signed long int        int32;
typedef signed long long int   int64;
typedef unsigned char          uInt8;
typedef unsigned short int     uInt16;
typedef unsigned int           uInt32;
typedef unsigned long long int uInt64;

enum ogDataState { ogNone, ogOwner, ogAliasing };

enum ogErrorCode {
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
}; // ogErrorCode

class ogRGB8 {
 public:
  uInt8 red;
  uInt8 green;
  uInt8 blue;
};

class ogRGBA8 {
 public:
  uInt8 red;
  uInt8 green;
  uInt8 blue;
  uInt8 alpha;
};

class ogRGB16 {
 public:
  uInt16 red;
  uInt16 blue;
  uInt16 green;
};

class ogRGBA16 {
 public: 
  uInt16 red;
  uInt16 green;
  uInt16 blue;
  uInt16 alpha;
};

class ogRGB32 {
 public:
  uInt32 red;
  uInt32 green;
  uInt32 blue;
};

class ogRGBA32 {
 public:
  uInt32 red;
  uInt32 green;
  uInt32 blue;
  uInt32 alpha;
};

class ogPoint2d {
 public:
  int32 x;
  int32 y;
};

class ogPoint3d {
 public:
  int32 x;
  int32 y;
  int32 z;
};

#endif
