#include "stdafx.h"

#include "SADXModLoader.h"
#include "Trampoline.h"
#include <stack>

#include "uiscale.h"

namespace uiscale
{
	FillMode bg_fill  = FillMode_Fill;
	FillMode fmv_fill = FillMode_Fit;

	float scale_min = 0.0f;
	float scale_max = 0.0f;
	float scale_h   = 0.0f;
	float scale_v   = 0.0f;
}

#pragma region Scale stack

struct ScaleEntry
{
	uiscale::Align alignment;
	NJS_POINT2 scale;
	bool is_background;
};

static std::stack<ScaleEntry, std::vector<ScaleEntry>> scale_stack;

static constexpr float THIRD_H = 640.0f / 3.0f;
static constexpr float THIRD_V = 480.0f / 3.0f;

// @formatter:int_align_eq true
// @formatter:int_align_declaration_names true
static bool  sprite_scale  = false;
static bool  do_scale      = false;
static bool  last_do_scale = false;
static float backup_h      = 0.0f;
static float backup_v      = 0.0f;
// @formatter:int_align_declaration_names restore
// @formatter:int_align_eq restore

static NJS_POINT2 region_fit  = { 0.0f, 0.0f };
static NJS_POINT2 region_fill = { 0.0f, 0.0f };

void uiscale::update_parameters()
{
	scale_h = HorizontalStretch;
	scale_v = VerticalStretch;

	scale_min = min(scale_h, scale_v);
	scale_max = max(scale_h, scale_v);

	region_fit.x  = 640.0f * scale_min;
	region_fit.y  = 480.0f * scale_min;
	region_fill.x = 640.0f * scale_max;
	region_fill.y = 480.0f * scale_max;

	hudscale::update();
}

bool uiscale::is_top_background()
{
	return !scale_stack.empty() && scale_stack.top().is_background;
}

bool uiscale::is_scale_enabled()
{
	return do_scale && (!is_top_background() || bg_fill != FillMode_Stretch);
}

void uiscale::scale_push(Uint8 align, bool is_background, float h, float v)
{
#ifdef _DEBUG
	auto pad = ControllerPointers[0];
	if (pad && pad->HeldButtons & Buttons_Z)
	{
		return;
	}
#endif

	scale_stack.push({ static_cast<Align>(align), { HorizontalStretch, VerticalStretch }, is_background });

	HorizontalStretch = h;
	VerticalStretch = v;

	do_scale = true;
}

void uiscale::scale_disable()
{
	backup_h = HorizontalStretch;
	backup_v = VerticalStretch;
	HorizontalStretch = scale_h;
	VerticalStretch = scale_v;
	last_do_scale = do_scale;
	do_scale = false;
}

void uiscale::scale_enable()
{
	do_scale = last_do_scale;

	if (do_scale)
	{
		HorizontalStretch = backup_h;
		VerticalStretch = backup_v;

		backup_h = backup_v = 0.0f;
	}
}

void uiscale::scale_pop()
{
	if (scale_stack.empty())
	{
		return;
	}

	auto point = scale_stack.top();
	HorizontalStretch = point.scale.x;
	VerticalStretch = point.scale.y;

	scale_stack.pop();
	do_scale = !scale_stack.empty();
}

static NJS_POINT2 auto_align(uiscale::Align align, const NJS_POINT2& center)
{
	using namespace uiscale;

	static constexpr Uint8 mask_horizontal = Align_Left | Align_Right | Align_Center_Horizontal;
	static constexpr Uint8 mask_vertical = Align_Top | Align_Bottom | Align_Center_Vertical;

	Uint8 actual_alignment = align;

	if (actual_alignment & Align_Automatic_Horizontal)
	{
		actual_alignment &= ~(Align_Left | Align_Right);

		if (center.x < THIRD_H)
		{
			actual_alignment |= Align_Left;
		}
		else if (center.x < THIRD_H * 2.0f)
		{
			actual_alignment |= Align_Center_Horizontal;
		}
		else
		{
			actual_alignment |= Align_Right;
		}
	}

	if (actual_alignment & Align_Automatic_Vertical)
	{
		actual_alignment &= ~(Align_Top | Align_Bottom);

		if (center.y < THIRD_V)
		{
			actual_alignment |= Align_Top;
		}
		else if (center.y < THIRD_V * 2.0f)
		{
			actual_alignment |= Align_Center_Vertical;
		}
		else
		{
			actual_alignment |= Align_Bottom;
		}
	}

	if (!(actual_alignment & mask_horizontal))
	{
		actual_alignment |= Align_Left;
	}

	if (!(actual_alignment & mask_vertical))
	{
		actual_alignment |= Align_Top;
	}

	NJS_POINT2 result = {};
	const auto width = static_cast<float>(HorizontalResolution);
	const auto height = static_cast<float>(VerticalResolution);

	switch (actual_alignment & mask_horizontal)
	{
		default:
		case Align_Left:
			break;

		case Align_Center_Horizontal:
			if (width / scale_v > 640.0f)
			{
				result.x = (width - region_fit.x) / 2.0f;
			}
			break;

		case Align_Right:
			result.x = width - region_fit.x;
			break;
	}

	switch (actual_alignment & mask_vertical)
	{
		default:
		case Align_Top:
			break;

		case Align_Center_Vertical:
			if (height / scale_h > 480.0f)
			{
				result.y = (height - region_fit.y) / 2.0f;
			}
			break;

		case Align_Bottom:
			result.y = height - region_fit.y;
			break;
	}

	return result;
}

static NJS_POINT2 auto_align(uiscale::Align align, const NJS_POINT3& center)
{
	return auto_align(align, *reinterpret_cast<const NJS_POINT2*>(&center));
}

inline NJS_POINT2 get_offset(uiscale::Align align, const NJS_POINT2& center)
{
	NJS_POINT2 offset;

	// if we're scaling a background with fill mode, manually set
	// coordinate offset so the entire image lands in the center.
	if (uiscale::is_top_background() && uiscale::bg_fill == uiscale::FillMode_Fill)
	{
		offset.x = (static_cast<float>(HorizontalResolution) - region_fill.x) / 2.0f;
		offset.y = (static_cast<float>(VerticalResolution) - region_fill.y) / 2.0f;
	}
	else
	{
		offset = auto_align(align, center);
	}

	return offset;
}

float uiscale::get_scale()
{
	return is_top_background() && bg_fill == FillMode_Fill ? scale_max : scale_min;
}

void uiscale::check_stack_balance()
{
	if (!scale_stack.empty())
	{
		// TODO: Maybe this should be a critical error?
		PrintDebug("UI SCALE STACK IS UNBALANCED! Stack should be empty at end of frame!\n");
	}
}

/**
 * \brief Scales and re-positions an array of structures containing the fields x and y.
 * \tparam T A structure type containing the fields x and y.
 * \param points Pointer to an array of T.
 * \param count Length of the array
 */
template <typename T>
static void scale_points(T* points, size_t count)
{
	if (sprite_scale || scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const uiscale::Align align = top.alignment;

	NJS_POINT2 center = {};
	const auto m = 1.0f / static_cast<float>(count);

	for (size_t i = 0; i < count; i++)
	{
		const auto& p = points[i];
		center.x += p.x * m;
		center.y += p.y * m;
	}

	const NJS_POINT2 offset = get_offset(align, center);
	const float scale = uiscale::get_scale();

	for (size_t i = 0; i < count; i++)
	{
		auto& p = points[i];
		p.x = p.x * scale + offset.x;
		p.y = p.y * scale + offset.y;
	}
}

static void scale_vector(NJS_VECTOR* point)
{
	if (sprite_scale || scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const uiscale::Align align = top.alignment;

	const NJS_POINT2 center = { point->x, point->y };

	const float scale = uiscale::get_scale();
	const NJS_POINT2 offset = get_offset(align, center);

	point->x = point->x * scale + offset.x;
	point->y = point->y * scale + offset.y;
	point->z *= 1.0f / scale;
}

static void scale_quad_ex(NJS_QUAD_TEXTURE_EX* quad)
{
	if (sprite_scale || scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const uiscale::Align align = top.alignment;

	const NJS_POINT2 center = {
		quad->x + (quad->vx1 / 2.0f),
		quad->y + (quad->vy2 / 2.0f)
	};

	const NJS_POINT2 offset = get_offset(align, center);
	const float scale = uiscale::get_scale();

	quad->x = quad->x * scale + offset.x;
	quad->y = quad->y * scale + offset.y;
	quad->vx1 *= scale;
	quad->vy1 *= scale;
	quad->vx2 *= scale;
	quad->vy2 *= scale;
}

static void scale_quad(float* x1, float* y1, float* x2, float* y2)
{
	if (sprite_scale || scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const uiscale::Align align = top.alignment;

	const NJS_POINT2 center = {
		*x1 + (*x2 / 2.0f),
		*y1 + (*y2 / 2.0f)
	};

	const float scale = uiscale::get_scale();
	const NJS_POINT2 offset = get_offset(align, center);

	*x1 = *x1 * scale + offset.x;
	*y1 = *y1 * scale + offset.y;
	*x2 = *x2 * scale + offset.x;
	*y2 = *y2 * scale + offset.y;
}

void uiscale::scale_texmemlist(NJS_TEXTURE_VTX* list, Int count)
{
	scale_points(list, count);
}

static NJS_SPRITE last_sprite = {};

static void __cdecl sprite_push(NJS_SPRITE* sp)
{
	using namespace uiscale;

	sprite_scale = true;

	if (scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const Align align = top.alignment;

	const NJS_POINT2 offset = auto_align(align, sp->p);

	last_sprite = *sp;

	sp->p.x = sp->p.x * scale_min + offset.x;
	sp->p.y = sp->p.y * scale_min + offset.y;
	sp->sx *= scale_min;
	sp->sy *= scale_min;
}

static void __cdecl sprite_pop(NJS_SPRITE* sp)
{
	sprite_scale = false;

	sp->p = last_sprite.p;
	sp->sx = last_sprite.sx;
	sp->sy = last_sprite.sy;
}

#pragma endregion

#pragma region Scale wrappers

static Trampoline* njDrawTextureMemList_t = nullptr;
static Trampoline* njDrawSprite2D_Queue_t = nullptr;
static Trampoline* Draw2DLinesMaybe_Queue_t = nullptr;
static Trampoline* njDrawTriangle2D_t = nullptr;
static Trampoline* Direct3D_DrawQuad_t = nullptr;
static Trampoline* njDrawPolygon_t = nullptr;
static Trampoline* chDrawBillboardSR_t = nullptr;
static Trampoline* njDrawCircle2D_t = nullptr;
static Trampoline* DisplayVideoFrame_t = nullptr;
static Trampoline* chCalcWorldPosFromScreenPos_t = nullptr;

static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE* sp, Int n, Float pri, NJD_SPRITE attr, QueuedModelFlagsB queue_flags)
{
	using namespace uiscale;

	if (sp == nullptr)
	{
		return;
	}

	auto original = reinterpret_cast<decltype(njDrawSprite2D_Queue_r)*>(njDrawSprite2D_Queue_t->Target());

	// Scales lens flare and sun.
	// It uses njProjectScreen so there's no position scaling required.
	if (sp == reinterpret_cast<NJS_SPRITE*>(0x009BF3B0))
	{
		sp->sx *= scale_min;
		sp->sy *= scale_min;
		original(sp, n, pri, attr, queue_flags);
	}
	else if (uiscale::is_scale_enabled())
	{
		sprite_push(sp);
		original(sp, n, pri, attr | NJD_SPRITE_SCALE, queue_flags);
		sprite_pop(sp);
	}
	else
	{
		original(sp, n, pri, attr, queue_flags);
	}
}

static void __cdecl Draw2DLinesMaybe_Queue_r(NJS_POINT2COL* points, int count, float depth, Uint32 attr, QueuedModelFlagsB flags)
{
	auto original = static_cast<decltype(Draw2DLinesMaybe_Queue_r)*>(Draw2DLinesMaybe_Queue_t->Target());

	if (uiscale::is_scale_enabled())
	{
		scale_points(points->p, count);
	}

	original(points, count, depth, attr, flags);
}

static void __cdecl njDrawTextureMemList_r(NJS_TEXTURE_VTX* polygons, Int count, Int tex, Int flag)
{
	auto original = static_cast<decltype(njDrawTextureMemList_r)*>(njDrawTextureMemList_t->Target());

	if (uiscale::is_scale_enabled())
	{
		scale_points(polygons, count);
	}

	original(polygons, count, tex, flag);
}

static void __cdecl njDrawTriangle2D_r(NJS_POINT2COL* p, Int n, Float pri, Uint32 attr)
{
	auto original = static_cast<decltype(njDrawTriangle2D_r)*>(njDrawTriangle2D_t->Target());

	if (uiscale::is_scale_enabled())
	{
		auto _n = n;

		if (attr & (NJD_DRAW_FAN | NJD_DRAW_CONNECTED))
		{
			_n += 2;
		}
		else
		{
			_n *= 3;
		}

		scale_points(p->p, _n);
	}

	original(p, n, pri, attr);
}

static void __cdecl Direct3D_DrawQuad_r(NJS_QUAD_TEXTURE_EX* quad)
{
	auto original = static_cast<decltype(Direct3D_DrawQuad_r)*>(Direct3D_DrawQuad_t->Target());

	if (uiscale::is_scale_enabled())
	{
		scale_quad_ex(quad);
	}

	original(quad);
}

static void __cdecl njDrawPolygon_r(NJS_POLYGON_VTX* polygon, Int count, Int trans)
{
	auto orig = static_cast<decltype(njDrawPolygon_r)*>(njDrawPolygon_t->Target());

	if (uiscale::is_scale_enabled())
	{
		scale_points(polygon, count);
	}

	orig(polygon, count, trans);
}

static void __cdecl chDrawBillboardSR_r(CHS_BILL_INFO* chaosprite, float x, float y, float depth, float sclx, float scly, int unused, __int16 use_sclx, __int16 use_scly)
{
	if (!uiscale::is_scale_enabled())
	{
		auto orig = static_cast<decltype(chDrawBillboardSR_r)*>(chDrawBillboardSR_t->Target());
		orig(chaosprite, x, y, depth, sclx, scly, unused, use_sclx, use_scly);
		return;
	}

	// HACK: SetPreferredFilterOption is declared in dllmain.cpp and is not exposed! please fix!
	void __cdecl SetPreferredFilterOption();
	SetPreferredFilterOption();

	Direct3D_SetTexList(chaosprite->pTexlist);
	njSetTextureNum_(chaosprite->TexNum);

	Direct3D_Device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_WRAP);
	Direct3D_Device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_WRAP);

	njAlphaMode(2);

	if (*reinterpret_cast<int*>(0x3D08584) == 1)
	{
		Direct3D_Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		Direct3D_Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
	}
	else
	{
		Direct3D_Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		Direct3D_Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	}

	float s0, s1, t0, t1;

	if (chaosprite->adjust != 0)
	{
		s0 = chaosprite->s0;
		s1 = chaosprite->s1;
		t0 = chaosprite->t0;
		t1 = chaosprite->t1;
	}
	else
	{
		s0 = chaosprite->s0 * 0.00390625f;
		s1 = chaosprite->s1 * 0.00390625f;
		t0 = chaosprite->t0 * 0.00390625f;
		t1 = chaosprite->t1 * 0.00390625f;
	}

	const float sclx_ = sclx * chaosprite->wd;
	const float scly_ = scly * chaosprite->ht;
	float x1 = x;
	float y1 = y;

	if (use_sclx == 0)
	{
		x1 = x - 0.5f * sclx_;
	}
	else if (use_sclx == 1)
	{
		x1 = x - sclx_;
	}

	if (use_scly == 0)
	{
		y1 = y - 0.5f * scly_;
	}
	else if (use_scly == 1)
	{
		y1 = y - scly_;
	}

	float x2 = x1 + sclx_;
	float y2 = y1 + scly_;

	scale_quad(&x1, &y1, &x2, &y2);

	const Uint32 color    = *reinterpret_cast<Uint32*>(0x3D08580);
	const float new_depth = DoWeirdProjectionThings(depth * -1.0f);

	FVFStruct_K* vbuff = reinterpret_cast<FVFStruct_K*>(GiantVertexBuffer_ptr);

	vbuff[0] = { { x1 + 0.5f, y1 + 0.5f, new_depth, 1.0f, color }, s0, t0 };
	vbuff[1] = { { x2 + 0.5f, y1 + 0.5f, new_depth, 1.0f, color }, s1, t0 };
	vbuff[2] = { { x1 + 0.5f, y2 + 0.5f, new_depth, 1.0f, color }, s0, t1 };
	vbuff[3] = { { x2 + 0.5f, y2 + 0.5f, new_depth, 1.0f, color }, s1, t1 };

	Direct3D_Device->SetVertexShader(D3DFVF_DIFFUSE | D3DFVF_XYZRHW | 0x100);
	Direct3D_Device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, GiantVertexBuffer_ptr, sizeof(FVFStruct_K));

	Direct3D_TextureFilterLinear();
}

static void __cdecl njDrawCircle2D_r(NJS_POINT2COL* p, Int n, Float pri, Uint32 attr)
{
	auto original = static_cast<decltype(njDrawCircle2D_r)*>(njDrawCircle2D_t->Target());

	if (uiscale::is_scale_enabled())
	{
		scale_points(p->p, n);
	}

	original(p, n, pri, attr);
}

static void __cdecl njDrawTextureMemList_init()
{
	// This has to be created dynamically to repair a function call.
	if (njDrawTextureMemList_t == nullptr)
	{
		njDrawTextureMemList_t = new Trampoline(0x0077DC70, 0x0077DC79, njDrawTextureMemList_r);
		WriteCall(reinterpret_cast<void*>(reinterpret_cast<size_t>(njDrawTextureMemList_t->Target()) + 4), njAlphaMode);
	}
}

static void __cdecl DisplayVideoFrame_r(int width, int height)
{
	using namespace uiscale;

	auto orig = bg_fill;
	bg_fill = fmv_fill;
	scale_trampoline(Align_Center, true, DisplayVideoFrame_r, DisplayVideoFrame_t, width, height);
	bg_fill = orig;
}

static void __cdecl chCalcWorldPosFromScreenPos_r(NJS_VECTOR* pos, NJS_VECTOR* ret)
{
	auto orig = static_cast<decltype(chCalcWorldPosFromScreenPos_r)*>(chCalcWorldPosFromScreenPos_t->Target());

	if (uiscale::is_scale_enabled())
	{
		scale_vector(pos);
	}

	orig(pos, ret);
}

#pragma endregion

void uiscale::initialize_common()
{
	static bool initialized = false;
	if (initialized == false)
	{
		update_parameters();
		njDrawTextureMemList_init();

		Direct3D_DrawQuad_t           = new Trampoline(0x0077DE10, 0x0077DE18, Direct3D_DrawQuad_r);
		njDrawPolygon_t               = new Trampoline(0x0077DBC0, 0x0077DBC5, njDrawPolygon_r);
		njDrawSprite2D_Queue_t        = new Trampoline(0x00404660, 0x00404666, njDrawSprite2D_Queue_r);
		Draw2DLinesMaybe_Queue_t      = new Trampoline(0x00404490, 0x00404496, Draw2DLinesMaybe_Queue_r);
		njDrawTriangle2D_t            = new Trampoline(0x0077E9F0, 0x0077E9F8, njDrawTriangle2D_r);
		njDrawCircle2D_t              = new Trampoline(0x0077DFC0, 0x0077DFC7, njDrawCircle2D_r);
		chCalcWorldPosFromScreenPos_t = new Trampoline(0x00720B10, 0x00720B17, chCalcWorldPosFromScreenPos_r);

		chDrawBillboardSR_t = new Trampoline(0x0078B070, 0x0078B079, chDrawBillboardSR_r);
		WriteCall(reinterpret_cast<void*>(reinterpret_cast<size_t>(chDrawBillboardSR_t->Target()) + 4), Direct3D_TextureFilterPoint);

		// njDrawCircle2D call in njDrawTriangle2D_t is already scaled, use original function instead.
		WriteCall(reinterpret_cast<void*>(0x77EA10), njDrawCircle2D_t->Target());

		// The backgound rectangle should not be scaled
		WriteCall(reinterpret_cast<void*>(0x78B922), njDrawCircle2D_t->Target());

		initialized = true;
	}
}

void uiscale::initialize()
{
	initialize_common();
}

void uiscale::setup_background_scale()
{
	initialize_common();
}

void uiscale::setup_fmv_scale()
{
	update_parameters();
	njDrawTextureMemList_init();

	if (Direct3D_DrawQuad_t == nullptr)
	{
		Direct3D_DrawQuad_t = new Trampoline(0x0077DE10, 0x0077DE18, Direct3D_DrawQuad_r);
	}

	DisplayVideoFrame_t = new Trampoline(0x005139F0, 0x005139F5, DisplayVideoFrame_r);
}
