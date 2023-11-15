//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include <pddi/gl/gltex.hpp>
#include <pddi/gl/glcon.hpp>

#ifdef RAD_WIN32
    #include <pddi/gl/display_win32/gldisplay.hpp>
    #include <pddi/gl/display_win32/gl.hpp>
#endif

#ifdef RAD_LINUX
    #include <pddi/gl/display_linux/gldisplay.hpp>
    #include <pddi/gl/display_linux/gl.hpp>
#endif

#include <math.h>
#include <pddi/base/debug.hpp>
#include <radmemory.hpp>

static inline GLenum PickPixelFormat(pddiPixelFormat format)
{
    switch (format)
    {
    case PDDI_PIXEL_RGB555:
#ifndef RAD_VITA
    case PDDI_PIXEL_RGB565: return GL_RGB5;
    case PDDI_PIXEL_ARGB1555: return GL_RGB5_A1;
    case PDDI_PIXEL_ARGB4444: return GL_RGBA4;
#endif
    case PDDI_PIXEL_RGB888: return GL_RGB8;
    case PDDI_PIXEL_ARGB8888: return GL_RGBA8;
    case PDDI_PIXEL_PAL8: return GL_COLOR_INDEX8_EXT;
#ifndef RAD_VITA
    case PDDI_PIXEL_PAL4: return GL_COLOR_INDEX4_EXT;
    case PDDI_PIXEL_LUM8: return GL_LUMINANCE8;
    case PDDI_PIXEL_DUDV88: return GL_LUMINANCE8_ALPHA8;
#endif
    case PDDI_PIXEL_DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case PDDI_PIXEL_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case PDDI_PIXEL_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    }
    PDDIASSERT(false);
    return GL_INVALID_ENUM;
};

static inline pddiPixelFormat PickPixelFormat(pddiTextureType type, int bitDepth, int alphaDepth)
{
    switch (type)
    {
    case PDDI_TEXTYPE_RGB:
        switch (alphaDepth)
        {
        case 0:
            return (bitDepth <= 16) ? PDDI_PIXEL_RGB565 : PDDI_PIXEL_RGB888;
        case 1:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB1555 : PDDI_PIXEL_ARGB8888;
        default:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB4444 : PDDI_PIXEL_ARGB8888;
        }
        break;

    case PDDI_TEXTYPE_PALETTIZED:
        return PDDI_PIXEL_PAL8;

    case PDDI_TEXTYPE_LUMINANCE:
        return PDDI_PIXEL_LUM8;

    case PDDI_TEXTYPE_BUMPMAP:
        return PDDI_PIXEL_DUDV88;

    case PDDI_TEXTYPE_DXT1:
        return PDDI_PIXEL_DXT1;

    case PDDI_TEXTYPE_DXT2:
        return PDDI_PIXEL_DXT2;

    case PDDI_TEXTYPE_DXT3:
        return PDDI_PIXEL_DXT3;

    case PDDI_TEXTYPE_DXT4:
        return PDDI_PIXEL_DXT4;

    case PDDI_TEXTYPE_DXT5:
        return PDDI_PIXEL_DXT5;
    }
    PDDIASSERT(false);
    return PDDI_PIXEL_UNKNOWN;
};

void pglTexture::SetGLState(void)
{
    if(context->contextID != contextID)
    {
        contextID = context->contextID;
        gltexture = 0;
    }

    glEnable(GL_TEXTURE_2D);
    if(!valid)
    {
        glDeleteTextures(1, &gltexture);
        glGenTextures(1,&gltexture);
        glBindTexture(GL_TEXTURE_2D, gltexture);

//      if(nMipMap == 0)
        if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
        {
            unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
            GLenum internalFormat = lock.format == PDDI_PIXEL_DXT5 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT :
                lock.format == PDDI_PIXEL_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, xSize,
                ySize, 0, ceil(xSize/4.0)*ceil(ySize/4.0)*blocksize, (GLvoid*)bits[0]);
        }
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, PickPixelFormat(lock.format), xSize,
                ySize, 0, lock.native ? GL_BGRA : GL_RGBA, GL_UNSIGNED_BYTE,
                (GLvoid *)bits[0]);
        }
        /*
        else
        {

            int tmpMipMap = nMipMap;
            tmpMipMap--;
            if(tmpMipMap < 0)
                tmpMipMap = 0;

            int i = 0;
            int width = xSize;
            int height = ySize;
            bool bottomed = false;
            bool done = false;

            while(!done)
            {
                char* data = bits[i];
                if(i > tmpMipMap)
                    data = bits[tmpMipMap];

                glTexImage2D(GL_TEXTURE_2D, i, GL_RGBA8, xSize >> i,
                    ySize >> i, 0, lock.native ? GL_BGRA_EXT : GL_RGBA, GL_UNSIGNED_BYTE,
                    (GLvoid *)data);

                done = (width == 1) || (height == 1);

                width >>= 1;
                if(width < 1) 
                {
                    width = 1;
                    bottomed = true;
                }

                height >>= 1;
                if(height < 1) 
                {
                    height = 1;
                    bottomed = true;
                }

                i++;
            }
        }
        */

        valid = true;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, gltexture);
    }

#ifndef RAD_VITA
    float fpriority = float(priority) / 31.0f;
    glPrioritizeTextures(1, &gltexture, &fpriority);
#endif
}

int fastlog2(int x)
{
    int r = 0;
    int tmp = x;
    while(tmp > 1)
    {
        r++;
        tmp = tmp >> 1;

        if((tmp << r) != x)
            // not power of 2
            return -1;
    }
    return r;
}

bool pglTexture::Create(int x, int y, int bpp, int alphaDepth, int nMip, pddiTextureType textureType, pddiTextureUsageHint usageHint)
{
    xSize = x;
    ySize = y;
    nMipMap = nMip;
    type = textureType;

    log2X = fastlog2(xSize);
    log2Y = fastlog2(ySize);

    if((log2X == -1) || (log2Y == -1))
    {
        lastError = PDDI_TEX_NOT_POW_2;
        return false;
    }

    ((pddiExtGLContext*)context->GetExtension(PDDI_EXT_GL_CONTEXT))->BeginContext();

    if ((xSize > context->GetMaxTextureDimension()) ||
        (ySize > context->GetMaxTextureDimension()))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    // TODO palletized
    if (textureType == PDDI_TEXTYPE_PALETTIZED)
    {
        textureType = PDDI_TEXTYPE_RGB;
        bpp = 32;
    }

    bits = new char* [nMipMap + 1];
    if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
    {
        unsigned int blocksize = type == PDDI_TEXTYPE_DXT1 ? 8 : 16;
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(RADMEMORY_ALLOC_TEMP, size_t(ceil(double(xSize>>i)/4)*ceil(double(ySize>>i)/4)*blocksize), 16);
    }
    else
    {
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(RADMEMORY_ALLOC_TEMP, (xSize>>i)*(ySize>>i)*4, 16);
    }

    lock.depth = bpp;
    lock.format = PickPixelFormat(textureType, bpp, alphaDepth);

    if(context->GetDisplay()->ExtBGRA())
    {
        lock.native = true;
        lock.rgbaLShift[0] = lock.rgbaRShift[0] =
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaLShift[2] = lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x00ff0000;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x000000ff;
        lock.rgbaMask[3] = 0xff000000;
    }
    else
    {
        lock.native = false;
        lock.rgbaRShift[0] = 16;
        lock.rgbaLShift[2] = 16;

        lock.rgbaLShift[0] = 
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x000000ff;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x00ff0000;
        lock.rgbaMask[3] = 0xff000000;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, (float)((xSize * ySize * 4) / 1024));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, 1);

    ((pddiExtGLContext*)context->GetExtension(PDDI_EXT_GL_CONTEXT))->EndContext();

    return true;
}

pglTexture::pglTexture(pglContext* c)
{
    context = c;
    contextID = c->contextID;
    bits = NULL;
    gltexture = 0;
    priority = 15;
    valid = false;
}

pglTexture::~pglTexture()
{
    for(int i = 0; i < nMipMap+1; i++)
        radMemoryFreeAligned(bits[i]);

    if(bits) delete [] bits;

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, -(float)((xSize * ySize * 4) / 1024));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, -1);
}

pddiPixelFormat pglTexture::GetPixelFormat()
{
    return PDDI_PIXEL_ARGB8888;
}

int   pglTexture::GetWidth()
{
    return xSize;
}

int   pglTexture::GetHeight()
{
    return ySize;
}

int   pglTexture::GetDepth()
{
    return 32;
}

int   pglTexture::GetNumMipMaps()
{
    return nMipMap;
}

int pglTexture::GetAlphaDepth()
{
    return 8;
}

pddiLockInfo* pglTexture::Lock(int mipMap, pddiRect* rect)
{
    PDDIASSERT(mipMap <= nMipMap);

    lock.width = 1 << (log2X-mipMap);
    lock.height = 1 << (log2Y-mipMap);
    if (lock.format != PDDI_PIXEL_DXT1 && lock.format != PDDI_PIXEL_DXT3 && lock.format != PDDI_PIXEL_DXT5)
    {
        lock.pitch = -lock.width * 4;
        lock.bits = bits[mipMap] + (lock.width * (lock.height - 1) * 4);
    }
    else
    {
        unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
        lock.pitch = ceil(double(xSize>>mipMap)/4)*blocksize;
        lock.bits = bits[mipMap];
    }

    return &lock;
}

void pglTexture::Unlock(int mipLevel)
{
    valid = false;
}

void pglTexture::SetPriority(int p)
{
    priority = p;
}

int pglTexture::GetPriority(void)
{
    return priority;
}

// paging control
void pglTexture::Prefetch(void)
{
}

void pglTexture::Discard(void)
{
}

// palette managment
int pglTexture::GetNumPaletteEntries(void)
{
    return 0;
}

void pglTexture::SetPalette(int nEntries, pddiColour* palette)
{
}

int pglTexture::GetPalette(pddiColour* palette)
{
    return 0;
}


