#ifndef INCLUDE_HLSLI
#define INCLUDE_HLSLI

// Maximum number of fragments to be sorted per pixel for OIT.
#ifndef OIT_MAX_FRAGMENTS
	#define OIT_MAX_FRAGMENTS 32
#endif

// The maximum number of texture stages supported.
#ifndef TEXTURE_STAGE_MAX
	#define TEXTURE_STAGE_MAX 8
#endif

// The active number of texture stages.
#ifndef TEXTURE_STAGE_COUNT
	#define TEXTURE_STAGE_COUNT TEXTURE_STAGE_MAX
#elif TEXTURE_STAGE_COUNT > TEXTURE_STAGE_MAX
	#error Active texture stage count exceeds maximum supported!
#endif

#define M_PI 3.14159265358979323846

// D3DCMPFUNC enum.
#define CMP_NEVER        1
#define CMP_LESS         2
#define CMP_EQUAL        3
#define CMP_LESSEQUAL    4
#define CMP_GREATER      5
#define CMP_NOTEQUAL     6
#define CMP_GREATEREQUAL 7
#define CMP_ALWAYS       8

// D3DBLEND enum.
#define BLEND_ZERO            1
#define BLEND_ONE             2
#define BLEND_SRCCOLOR        3
#define BLEND_INVSRCCOLOR     4
#define BLEND_SRCALPHA        5
#define BLEND_INVSRCALPHA     6
#define BLEND_DESTALPHA       7
#define BLEND_INVDESTALPHA    8
#define BLEND_DESTCOLOR       9
#define BLEND_INVDESTCOLOR    10
#define BLEND_SRCALPHASAT     11

// D3DBLENDOP enum.
#define BLENDOP_ADD         1
#define BLENDOP_SUBTRACT    2
#define BLENDOP_REVSUBTRACT 3
#define BLENDOP_MIN         4
#define BLENDOP_MAX         5

// D3DMCS enum (CS = Color Source).
#define CS_MATERIAL 0 // Color from material is used
#define CS_COLOR1   1 // Diffuse vertex color is used
#define CS_COLOR2   2 // Specular vertex color is used

// From FixedFuncEMU.fx
// Copyright (c) 2005 Microsoft Corporation. All rights reserved.
#define FOGMODE_NONE   0
#define FOGMODE_EXP    1
#define FOGMODE_EXP2   2
#define FOGMODE_LINEAR 3
#define E              2.71828
// End FixedFuncEMU.fx

// D3DLIGHTTYPE enum.
#define LIGHT_POINT       1
#define LIGHT_SPOT        2
#define LIGHT_DIRECTIONAL 3

// D3DTA_ preprocessor definitions (TA = Texture Argument)
#define TA_SELECTMASK        0x0000000f  // mask for arg selector
#define TA_DIFFUSE           0x00000000  // select diffuse color (read only)
#define TA_CURRENT           0x00000001  // select stage destination register (read/write)
#define TA_TEXTURE           0x00000002  // select texture color (read only)
#define TA_TFACTOR           0x00000003  // select D3DRS_TEXTUREFACTOR (read only)
#define TA_SPECULAR          0x00000004  // select specular color (read only)
#define TA_TEMP              0x00000005  // select temporary register color (read/write)
#define TA_COMPLEMENT        0x00000010  // take 1.0 - x (read modifier)
#define TA_ALPHAREPLICATE    0x00000020  // replicate alpha to color components (read modifier)

// D3DTEXTUREOP enum (TOP = Texture OP[eration]).
#define TOP_DISABLE                    1
#define TOP_SELECTARG1                 2
#define TOP_SELECTARG2                 3
#define TOP_MODULATE                   4
#define TOP_MODULATE2X                 5
#define TOP_MODULATE4X                 6
#define TOP_ADD                        7
#define TOP_ADDSIGNED                  8
#define TOP_ADDSIGNED2X                9
#define TOP_SUBTRACT                  10
#define TOP_ADDSMOOTH                 11
#define TOP_BLENDDIFFUSEALPHA         12
#define TOP_BLENDTEXTUREALPHA         13
#define TOP_BLENDFACTORALPHA          14
#define TOP_BLENDTEXTUREALPHAPM       15
#define TOP_BLENDCURRENTALPHA         16
#define TOP_PREMODULATE               17
#define TOP_MODULATEALPHA_ADDCOLOR    18
#define TOP_MODULATECOLOR_ADDALPHA    19
#define TOP_MODULATEINVALPHA_ADDCOLOR 20
#define TOP_MODULATEINVCOLOR_ADDALPHA 21
#define TOP_BUMPENVMAP                22
#define TOP_BUMPENVMAPLUMINANCE       23
#define TOP_DOTPRODUCT3               24
#define TOP_MULTIPLYADD               25
#define TOP_LERP                      26

// D3DTSS_ preprocessor definitions (TSS = Texture Stage State).
#define TSS_TCI_SELECT_MASK                          0x000F0000
#define TSS_TCI_COORD_MASK                           0x0000FFFF
#define TSS_TCI_PASSTHRU                             0x00000000
#define TSS_TCI_CAMERASPACENORMAL                    0x00010000
#define TSS_TCI_CAMERASPACEPOSITION                  0x00020000
#define TSS_TCI_CAMERASPACEREFLECTIONVECTOR          0x00030000

// Magic number to consider a null-entry in the OIT buffer.
static const uint OIT_FRAGMENT_LIST_NULL = 0xFFFFFFFF;

// OIT fragment linked list node.
struct OitNode
{
	float depth; // fragment depth
	uint  color; // 32-bit packed fragment color
	uint  flags; // 16 bit draw call number, 4 bit blend op, 4 bit source blend, 4 bit destination blend
	uint  next;  // index of the next entry, or OIT_FRAGMENT_LIST_NULL
};

// TODO: Optional per-pixel link count (prevents over-saturating one pixel!)

#ifdef OIT_NODE_WRITE

// Read/write mode.

globallycoherent RWTexture2D<uint>           frag_list_head  : register(u1);
globallycoherent RWTexture2D<uint>           frag_list_count : register(u2);
globallycoherent RWStructuredBuffer<OitNode> frag_list_nodes : register(u3);

#else

// Read-only mode.

Texture2D<uint>           frag_list_head  : register(t0);
Texture2D<uint>           frag_list_count : register(t1);
StructuredBuffer<OitNode> frag_list_nodes : register(t2);
Texture2D                 back_buffer     : register(t3);
Texture2D                 depth_buffer    : register(t4);

#endif

// From D3DX_DXGIFormatConvert.inl

// Originally D3DX_FLOAT_to_UINT
uint float_to_uint(float f, float scale)
{
	return (uint)floor(f * scale + 0.5f);
}

// Originally D3DX_R8G8B8A8_UNORM_to_FLOAT4
float4 unorm_to_float4(uint packed)
{
	precise float4 unpacked;
	unpacked.x = (float)(packed & 0x000000ff) / 255;
	unpacked.y = (float)(((packed >> 8) & 0x000000ff)) / 255;
	unpacked.z = (float)(((packed >> 16) & 0x000000ff)) / 255;
	unpacked.w = (float)(packed >> 24) / 255;
	return unpacked;
}

// Originally D3DX_FLOAT4_to_R8G8B8A8_UNORM
uint float4_to_unorm(precise float4 unpacked)
{
	const uint packed = ((float_to_uint(saturate(unpacked.x), 255)) |
	                     (float_to_uint(saturate(unpacked.y), 255) << 8) |
	                     (float_to_uint(saturate(unpacked.z), 255) << 16) |
	                     (float_to_uint(saturate(unpacked.w), 255) << 24));
	return packed;
}

// End D3DX_DXGIFormatConvert.inl

#endif
