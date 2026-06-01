// (C) 2002-2026 The UbixOS Project

extern "C"
{
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

	/* From og_stbimpl.c (stb_image, PNG only). Returns a malloc'd RGBA buffer. */
	unsigned char *stbi_load_from_memory(
	    const unsigned char *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
	void stbi_image_free(void *retval_from_stbi_load);
}

#include <objgfx/ogImage.h>
#include <objgfx/objgfx.h>

static bool fd_read(int fd, void *buf, size_t n)
{
	return (size_t)read(fd, buf, n) == n;
}

static bool fd_write(int fd, const void *buf, size_t n)
{
	return (size_t)write(fd, buf, n) == n;
}

bool ogImage::DecodeBMP(int fd, ogSurface &surface)
{
	Win3xBitmapHeader hdr;
	Win3xBitmapInfoHeader info;

	if (!fd_read(fd, &hdr, sizeof(hdr)))
		return false;
	if (!fd_read(fd, &info, sizeof(info)))
		return false;
	if (!hdr.IsMatch() || !info.IsMatch())
		return false;

	// seek to pixel data
	lseek(fd, hdr.ImageDataOffset, SEEK_SET);

	size_t lineSize, paddington;
	char linePadding[4];

	if (info.BitsPerPixel == 8)
	{
		if (!surface.ogCreate(info.ImageWidth, info.ImageHeight, OG_PIXFMT_8BPP))
			return false;

		// re-read from after the two headers to get the palette
		lseek(fd, sizeof(hdr) + sizeof(info), SEEK_SET);

		lineSize = ((info.ImageWidth + 3) >> 2) << 2;
		paddington = lineSize - info.ImageWidth;
		if (paddington > 4)
			return false;

		RGBQuad quad;
		for (unsigned c = 0; c < 256; c++)
		{
			if (!fd_read(fd, &quad, sizeof(quad)))
				return false;
			surface.ogSetPalette(c, quad.rgbRed, quad.rgbGreen, quad.rgbBlue, quad.rgbReserved);
		}

		for (unsigned y = surface.ogGetMaxY() + 1; y-- > 0;)
		{
			char *ptr = (char *)surface.ogGetPtr(0, y);
			if (!ptr)
				return false;
			if (!fd_read(fd, ptr, info.ImageWidth))
				return false;
			if (paddington && !fd_read(fd, linePadding, paddington))
				return false;
		}
	}
	else if (info.BitsPerPixel == 24)
	{
		if (!surface.ogCreate(info.ImageWidth, info.ImageHeight, OG_PIXFMT_24BPP))
			return false;

		size_t widthBytes = info.ImageWidth * 3;
		lineSize = ((widthBytes + 3) >> 2) << 2;
		paddington = lineSize - widthBytes;
		if (paddington > 4)
			return false;

		for (unsigned y = surface.ogGetMaxY() + 1; y-- > 0;)
		{
			char *ptr = (char *)surface.ogGetPtr(0, y);
			if (!ptr)
				return false;
			if (!fd_read(fd, ptr, widthBytes))
				return false;
			if (paddington && !fd_read(fd, linePadding, paddington))
				return false;
		}
	}
	else
	{
		return false;
	}

	return true;
}

/**
 * Decode a PNG (any bit depth/colour type) into a 24bpp surface via stb_image.
 * The whole file is slurped into memory, decoded to RGBA, then packed into the
 * surface as B,G,R bytes (the order the compositor and DecodeBMP both expect).
 */
bool ogImage::DecodePNG(int fd, ogSurface &surface)
{
	struct stat st;

	if (fstat(fd, &st) != 0 || st.st_size <= 0)
		return false;

	size_t sz = (size_t)st.st_size;
	unsigned char *file = (unsigned char *)malloc(sz);
	if (file == nullptr)
		return false;

	lseek(fd, 0, SEEK_SET);
	size_t got = 0;
	while (got < sz)
	{
		ssize_t r = read(fd, file + got, sz - got);
		if (r <= 0)
			break;
		got += (size_t)r;
	}
	if (got != sz)
	{
		free(file);
		return false;
	}

	int w = 0, h = 0, ch = 0;
	unsigned char *px = stbi_load_from_memory(file, (int)sz, &w, &h, &ch, 4);
	free(file);
	if (px == nullptr || w <= 0 || h <= 0)
	{
		if (px)
			stbi_image_free(px);
		return false;
	}

	if (!surface.ogCreate((uInt32)w, (uInt32)h, OG_PIXFMT_24BPP))
	{
		stbi_image_free(px);
		return false;
	}

	for (int y = 0; y < h; y++)
	{
		unsigned char *dst = (unsigned char *)surface.ogGetPtr(0, (uInt32)y);
		if (dst == nullptr)
		{
			stbi_image_free(px);
			return false;
		}
		const unsigned char *src = px + (size_t)y * (size_t)w * 4;
		for (int x = 0; x < w; x++)
		{
			dst[x * 3 + 0] = src[x * 4 + 2]; /* B */
			dst[x * 3 + 1] = src[x * 4 + 1]; /* G */
			dst[x * 3 + 2] = src[x * 4 + 0]; /* R */
		}
	}

	stbi_image_free(px);
	return true;
}

bool ogImage::EncodeBMP(int fd, ogSurface &surface)
{
	Win3xBitmapHeader hdr;
	Win3xBitmapInfoHeader info;
	const char zeroPad[4] = {0, 0, 0, 0};
	ogRGBA8 ogPal[256];
	size_t lineSize, paddington;

	info.ImageWidth = surface.ogGetMaxX() + 1;
	info.ImageHeight = surface.ogGetMaxY() + 1;
	info.NumberOfImagePlanes = 1;
	info.CompressionMethod = 0;

	hdr.ImageFileType = 0x4D42;
	hdr.Reserved1 = hdr.Reserved2 = 0;
	hdr.ImageDataOffset = sizeof(hdr) + sizeof(info);

	switch (surface.ogGetBPP())
	{
		case 8:
			info.BitsPerPixel = 8;
			info.NumColoursUsed = 256;
			hdr.ImageDataOffset += 256 * sizeof(RGBQuad);
			lineSize = ((info.ImageWidth + 3) >> 2) << 2;
			info.SizeOfBitmap = lineSize * info.ImageHeight;
			hdr.FileSize = hdr.ImageDataOffset + info.SizeOfBitmap;
			paddington = lineSize - info.ImageWidth;

			if (!fd_write(fd, &hdr, sizeof(hdr)))
				return false;
			if (!fd_write(fd, &info, sizeof(info)))
				return false;

			surface.ogGetPalette(ogPal);
			for (size_t i = 0; i < 256; i++)
			{
				RGBQuad q(ogPal[i]);
				if (!fd_write(fd, &q, sizeof(q)))
					return false;
			}

			for (unsigned y = surface.ogGetMaxY() + 1; y-- > 0;)
			{
				char *ptr = (char *)surface.ogGetPtr(0, y);
				if (ptr && !fd_write(fd, ptr, info.ImageWidth))
					return false;
				if (paddington && !fd_write(fd, zeroPad, paddington))
					return false;
			}
			break;

		case 24:
		case 32:
		case 16:
		case 15:
			info.BitsPerPixel = 24;
			info.NumColoursUsed = 0;
			lineSize = ((info.ImageWidth * 3 + 3) >> 2) << 2;
			info.SizeOfBitmap = lineSize * info.ImageHeight;
			hdr.FileSize = hdr.ImageDataOffset + info.SizeOfBitmap;
			paddington = lineSize - info.ImageWidth * 3;

			if (!fd_write(fd, &hdr, sizeof(hdr)))
				return false;
			if (!fd_write(fd, &info, sizeof(info)))
				return false;

			for (unsigned y = surface.ogGetMaxY() + 1; y-- > 0;)
			{
				for (unsigned x = 0; x <= surface.ogGetMaxX(); x++)
				{
					uInt8 r, g, b;
					surface.ogUnpack(surface.ogGetPixel(x, y), r, g, b);
					if (!fd_write(fd, &b, 1))
						return false;
					if (!fd_write(fd, &g, 1))
						return false;
					if (!fd_write(fd, &r, 1))
						return false;
				}
				if (paddington && !fd_write(fd, zeroPad, paddington))
					return false;
			}
			break;

		default:
			return false;
	}

	return true;
}

bool ogImage::Load(const char *filename, ogSurface &surface)
{
	static const unsigned char png_sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
	unsigned char magic[8];

	int fd = open(filename, O_RDONLY);
	if (fd < 0)
		return false;

	/* Sniff the magic to pick a decoder; both rewind before decoding. */
	bool ok = false;
	if (read(fd, magic, sizeof(magic)) == (ssize_t)sizeof(magic))
	{
		lseek(fd, 0, SEEK_SET);
		if (memcmp(magic, png_sig, sizeof(png_sig)) == 0)
			ok = DecodePNG(fd, surface);
		else
			ok = DecodeBMP(fd, surface);
	}
	close(fd);
	return ok;
}

bool ogImage::Save(const char *filename, ogSurface &surface, ogImageType imageType, ogImageOptions * /*options*/)
{
	if (imageType != BMP)
		return false;
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0)
		return false;
	bool ok = EncodeBMP(fd, surface);
	close(fd);
	return ok;
}
