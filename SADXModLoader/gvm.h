#pragma once

// GVR data formats
#define GJD_TEXFMT_ARGB_I4            (0x00)
#define GJD_TEXFMT_ARGB_I8            (0x01)
#define GJD_TEXFMT_ARGB_IA4           (0x02)
#define GJD_TEXFMT_ARGB_IA8           (0x03)
#define GJD_TEXFMT_RGB_565            (0x04)
#define GJD_TEXFMT_ARGB_5A3           (0x05)
#define GJD_TEXFMT_ARGB_8888		  (0x06)

#define GJD_TEXFMT_PALETTIZE4		  (0x08)
#define GJD_TEXFMT_PALETTIZE8		  (0x09)
#define GJD_TEXFMT_DXT1				  (0x0E)

// GVM flags
#define GVMH_NAMES					  (0x1)
#define GVMH_FORMATS				  (0x2)
#define GVMH_DIMENSIONS				  (0x4)
#define GVMH_GBIX					  (0x8)

// GVR surface flags
#define GJD_SURFACEFLAGS_MIPMAPED     (0x80000000)
#define GJD_SURFACEFLAGS_VQ           (0x40000000)
#define GJD_SURFACEFLAGS_NOTWIDDLED   (0x04000000)
#define GJD_SURFACEFLAGS_TWIDDLED     (0x00000000)
#define GJD_SURFACEFLAGS_STRIDE       (0x02000000)
#define GJD_SURFACEFLAGS_PALETTIZED   (0x00008000)

// GVR data flags
#define GVR_FLAG_MIPMAP               (0x1)
#define GVR_FLAG_PALETTE              (0x2)
#define GVR_FLAG_PALETTE_INT          (0x8)
