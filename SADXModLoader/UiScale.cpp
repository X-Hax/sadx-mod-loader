#include "stdafx.h"

#include "SADXModLoader.h"
#include "Trampoline.h"
#include <stack>

#include "UiScale.h"

using std::stack;
using std::vector;

namespace uiscale
{
	FillMode bg_fill  = FillMode::fill;
	FillMode fmv_fill = FillMode::fit;
}

using namespace uiscale;

#pragma region Scale stack

enum Align : Uint8
{
	automatic,
	horizontal_center = 1 << 0,
	vertical_center   = 1 << 1,
	center            = horizontal_center | vertical_center,
	left              = 1 << 2,
	top               = 1 << 3,
	right             = 1 << 4,
	bottom            = 1 << 5
};

struct ScaleEntry
{
	Uint8 alignment;
	NJS_POINT2 scale;
	bool is_background;
};

static stack<ScaleEntry, vector<ScaleEntry>> scale_stack;

static const float patch_dummy = 1.0f;

static constexpr float THIRD_H = 640.0f / 3.0f;
static constexpr float THIRD_V = 480.0f / 3.0f;

static bool  sprite_scale = false;
static bool  do_scale     = false;
static bool  _do_scale    = false;
static float scale_min    = 0.0f;
static float scale_max    = 0.0f;
static float scale_h      = 0.0f;
static float scale_v      = 0.0f;
static float backup_h     = 0.0f;
static float backup_v     = 0.0f;

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
}

inline bool is_top_background()
{
	return !scale_stack.empty() && scale_stack.top().is_background;
}

inline bool is_scale_enabled()
{
	return do_scale && (!is_top_background() || bg_fill != FillMode::stretch);
}

static void scale_push(Uint8 align, bool is_background, float h = 1.0f, float v = 1.0f)
{
#ifdef _DEBUG
	auto pad = ControllerPointers[0];
	if (pad && pad->HeldButtons & Buttons_Z)
	{
		return;
	}
#endif

	scale_stack.push({ align, { HorizontalStretch, VerticalStretch }, is_background });

	HorizontalStretch = h;
	VerticalStretch = v;

	do_scale = true;
}

static void scale_disable()
{
	backup_h = HorizontalStretch;
	backup_v = VerticalStretch;
	HorizontalStretch = scale_h;
	VerticalStretch = scale_v;
	_do_scale = do_scale;
	do_scale = false;
}

static void scale_enable()
{
	do_scale = _do_scale;

	if (do_scale)
	{
		HorizontalStretch = backup_h;
		VerticalStretch = backup_v;

		backup_h = backup_v = 0.0f;
	}
}

static void scale_pop()
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

static Trampoline* DisplayAllObjects_t;

static void __cdecl DisplayAllObjects_r()
{
	if (do_scale)
	{
		scale_push(Align::automatic, false, scale_h, scale_v);
	}

	auto original = static_cast<decltype(DisplayAllObjects_r)*>(DisplayAllObjects_t->Target());
	original();

	if (do_scale)
	{
		scale_pop();
	}
}

static NJS_POINT2 auto_align(Uint8 align, const NJS_POINT2& center)
{
	if (align == Align::automatic)
	{
		if (center.x < THIRD_H)
		{
			align |= Align::left;
		}
		else if (center.x < THIRD_H * 2.0f)
		{
			align |= Align::horizontal_center;
		}
		else
		{
			align |= Align::right;
		}

		if (center.y < THIRD_V)
		{
			align |= Align::top;
		}
		else if (center.y < THIRD_V * 2.0f)
		{
			align |= Align::vertical_center;
		}
		else
		{
			align |= Align::bottom;
		}
	}

	NJS_POINT2 result = {};
	const auto h = static_cast<float>(HorizontalResolution);
	const auto v = static_cast<float>(VerticalResolution);

	if (align & Align::horizontal_center)
	{
		if (h / scale_v > 640.0f)
		{
			result.x = (h - region_fit.x) / 2.0f;
		}
	}
	else if (align & Align::right)
	{
		result.x = h - region_fit.x;
	}

	if (align & Align::vertical_center)
	{
		if (v / scale_h > 480.0f)
		{
			result.y = (v - region_fit.y) / 2.0f;
		}
	}
	else if (align & Align::bottom)
	{
		result.y = v - region_fit.y;
	}

	return result;
}

static NJS_POINT2 auto_align(Uint8 align, const NJS_POINT3& center)
{
	return auto_align(align, *reinterpret_cast<const NJS_POINT2*>(&center));
}

inline NJS_POINT2 get_offset(Uint8 align, const NJS_POINT2& center)
{
	NJS_POINT2 offset;

	// if we're scaling a background with fill mode, manually set
	// coordinate offset so the entire image lands in the center.
	if (is_top_background() && bg_fill == FillMode::fill)
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

inline float get_scale()
{
	return is_top_background() && bg_fill == FillMode::fill ? scale_max : scale_min;
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
	const Uint8 align = top.alignment;

	NJS_POINT2 center = {};
	const auto m = 1.0f / static_cast<float>(count);

	for (size_t i = 0; i < count; i++)
	{
		const auto& p = points[i];
		center.x += p.x * m;
		center.y += p.y * m;
	}

	const NJS_POINT2 offset = get_offset(align, center);
	const float scale = get_scale();

	for (size_t i = 0; i < count; i++)
	{
		auto& p = points[i];
		p.x = p.x * scale + offset.x;
		p.y = p.y * scale + offset.y;
	}
}

static void scale_quad_ex(NJS_QUAD_TEXTURE_EX* quad)
{
	if (sprite_scale || scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const Uint8 align = top.alignment;

	const NJS_POINT2 center = {
		quad->x + (quad->vx1 / 2.0f),
		quad->y + (quad->vy2 / 2.0f)
	};

	const NJS_POINT2 offset = get_offset(align, center);
	const float scale = get_scale();

	quad->x = quad->x * scale + offset.x;
	quad->y = quad->y * scale + offset.y;
	quad->vx1 *= scale;
	quad->vy1 *= scale;
	quad->vx2 *= scale;
	quad->vy2 *= scale;
}

static NJS_SPRITE last_sprite = {};
static void __cdecl sprite_push(NJS_SPRITE* sp)
{
	sprite_scale = true;

	if (scale_stack.empty())
	{
		return;
	}

	const auto& top = scale_stack.top();
	const Uint8 align = top.alignment;

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

static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE *sp, Int n, Float pri, NJD_SPRITE attr, QueuedModelFlagsB queue_flags);
static void __cdecl Draw2DLinesMaybe_Queue_r(NJS_POINT2COL *points, int count, float depth, Uint32 attr, QueuedModelFlagsB flags);
static void __cdecl njDrawTriangle2D_r(NJS_POINT2COL *p, Int n, Float pri, Uint32 attr);
static void __cdecl Direct3D_DrawQuad_r(NJS_QUAD_TEXTURE_EX* quad);
static void __cdecl njDrawPolygon_r(NJS_POLYGON_VTX *polygon, Int count, Int trans);

// Must be initialized dynamically to fix a call instruction.
static Trampoline* njDrawTextureMemList_t = nullptr;
static Trampoline njDrawSprite2D_Queue_t(0x00404660, 0x00404666, njDrawSprite2D_Queue_r);
static Trampoline Draw2DLinesMaybe_Queue_t(0x00404490, 0x00404496, Draw2DLinesMaybe_Queue_r);
static Trampoline njDrawTriangle2D_t(0x0077E9F0, 0x0077E9F8, njDrawTriangle2D_r);
static Trampoline Direct3D_DrawQuad_t(0x0077DE10, 0x0077DE18, Direct3D_DrawQuad_r);
static Trampoline njDrawPolygon_t(0x0077DBC0, 0x0077DBC5, njDrawPolygon_r);

static void __cdecl njDrawSprite2D_Queue_r(NJS_SPRITE *sp, Int n, Float pri, NJD_SPRITE attr, QueuedModelFlagsB queue_flags)
{
	if (sp == nullptr)
	{
		return;
	}

	NonStaticFunctionPointer(void, original, (NJS_SPRITE*, Int, Float, NJD_SPRITE, QueuedModelFlagsB), njDrawSprite2D_Queue_t.Target());

	// Scales lens flare and sun.
	// It uses njProjectScreen so there's no position scaling required.
	if (sp == reinterpret_cast<NJS_SPRITE*>(0x009BF3B0))
	{
		sp->sx *= scale_min;
		sp->sy *= scale_min;
		original(sp, n, pri, attr, queue_flags);
	}
	else if (is_scale_enabled())
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

static void __cdecl Draw2DLinesMaybe_Queue_r(NJS_POINT2COL *points, int count, float depth, Uint32 attr, QueuedModelFlagsB flags)
{
	auto original = static_cast<decltype(Draw2DLinesMaybe_Queue_r)*>(Draw2DLinesMaybe_Queue_t.Target());

	if (is_scale_enabled())
	{
		scale_points(points->p, count);
	}

	original(points, count, depth, attr, flags);
}

static void __cdecl njDrawTextureMemList_r(NJS_TEXTURE_VTX *polygon, Int count, Int tex, Int flag)
{
	auto original = static_cast<decltype(njDrawTextureMemList_r)*>(njDrawTextureMemList_t->Target());

	if (is_scale_enabled())
	{
		scale_points(polygon, count);
	}

	original(polygon, count, tex, flag);
}

static void __cdecl njDrawTriangle2D_r(NJS_POINT2COL *p, Int n, Float pri, Uint32 attr)
{
	auto original = static_cast<decltype(njDrawTriangle2D_r)*>(njDrawTriangle2D_t.Target());

	if (is_scale_enabled())
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
	auto original = static_cast<decltype(Direct3D_DrawQuad_r)*>(Direct3D_DrawQuad_t.Target());

	if (is_scale_enabled())
	{
		scale_quad_ex(quad);
	}

	original(quad);
}

static void __cdecl njDrawPolygon_r(NJS_POLYGON_VTX* polygon, Int count, Int trans)
{
	auto orig = static_cast<decltype(njDrawPolygon_r)*>(njDrawPolygon_t.Target());

	if (is_scale_enabled())
	{
		scale_points(polygon, count);
	}

	orig(polygon, count, trans);
}

static void njDrawTextureMemList_init()
{
	// This has to be created dynamically to repair a function call.
	if (njDrawTextureMemList_t == nullptr)
	{
		njDrawTextureMemList_t = new Trampoline(0x0077DC70, 0x0077DC79, njDrawTextureMemList_r);
		WriteCall(reinterpret_cast<void*>(reinterpret_cast<size_t>(njDrawTextureMemList_t->Target()) + 4), njAlphaMode);
	}
}

#pragma endregion

#pragma region Convenience

/**
 * \brief Calls a trampoline function.
 * \tparam T Function type
 * \tparam Args
 * \param align Alignment mode
 * \param is_background Enables background scaling mode.
 * \param pfn The function to call.
 * \param args Option arguments for function.
 */
template<typename T, typename... Args>
void scale_function(Uint8 align, bool is_background, T *const pfn, Args... args)
{
	if (is_background && bg_fill == FillMode::stretch)
	{
		scale_disable();
		pfn(args...);
		scale_enable();
		return;
	}

	scale_push(align, is_background);
	pfn(args...);
	scale_pop();
}

/**
 * \brief Calls a trampoline function with a return type.
 * \tparam R Return type
 * \tparam T Function type
 * \tparam Args 
 * \param align Alignment mode
 * \param is_background Enables background scaling mode.
 * \param pfn The function to call.
 * \param args Optional arguments for function.
 * \return Return value of function.
 */
template<typename R, typename T, typename... Args>
R scale_function(Uint8 align, bool is_background, T *const pfn, Args... args)
{
	if (is_background && bg_fill == FillMode::stretch)
	{
		scale_disable();
		R result = pfn(args...);
		scale_enable();
		return result;
	}

	scale_push(align, is_background);
	R result = pfn(args...);
	scale_pop();
	return result;
}

/**
 * \brief Calls a trampoline function.
 * \tparam T Function type
 * \tparam Args
 * \param align Alignment mode
 * \param is_background Enables background scaling mode.
 * \param t Trampoline
 * \param args Option arguments for function
 */
template<typename T, typename... Args>
void scale_trampoline(Uint8 align, bool is_background, const T&, const Trampoline* t, Args... args)
{
	T *const pfn = reinterpret_cast<T*>(t->Target());
	scale_function(align, is_background, pfn, args...);
}

/**
 * \brief Calls a trampoline function with a return type.
 * \tparam R Return type
 * \tparam T Function type
 * \tparam Args 
 * \param align Alignment mode
 * \param is_background Enables background scaling mode.
 * \param t Trampoline
 * \param args Optional arguments for function
 * \return Return value of trampoline function
 */
template<typename R, typename T, typename... Args>
R scale_trampoline(Uint8 align, bool is_background, const T&, const Trampoline* t, Args... args)
{
	T *const pfn = reinterpret_cast<T*>(t->Target());
	return scale_function<R>(align, is_background, pfn, args...);
}

#pragma endregion

#pragma region HUD scaling

#pragma region Trampolines

static Trampoline* HudDisplayRingTimeLife_Check_t;
static Trampoline* HudDisplayScoreOrTimer_t;
static Trampoline* DrawStageMissionImage_t;
static Trampoline* DisplayPauseMenu_t;
static Trampoline* LifeGauge_Main_t;
static Trampoline* scaleScoreA;
static Trampoline* scaleTornadoHP;
static Trampoline* scaleTwinkleCircuitHUD;
static Trampoline* FishingHud_DrawHIT_t;
static Trampoline* FishingHud_DrawReel_t;
static Trampoline* FishingHud_DrawRod_t;
static Trampoline* BigHud_DrawWeightAndLife_t;
static Trampoline* FishingHud_DrawMeters_t;
static Trampoline* scaleAnimalPickup;
static Trampoline* scaleItemBoxSprite;
static Trampoline* scaleBalls;
static Trampoline* scaleCheckpointTime;
static Trampoline* scaleEmeraldRadarA;
static Trampoline* scaleEmeraldRadar_Grab;
static Trampoline* scaleEmeraldRadarB;
static Trampoline* scaleSandHillMultiplier;
static Trampoline* scaleIceCapMultiplier;
static Trampoline* scaleGammaTimeAddHud;
static Trampoline* scaleGammaTimeRemaining;
static Trampoline* scaleEmblemScreen;
static Trampoline* scaleBossName;
static Trampoline* scaleNightsCards;
static Trampoline* scaleNightsJackpot;
static Trampoline* scaleMissionStartClear;
static Trampoline* scaleMissionTimer;
static Trampoline* scaleMissionCounter;
static Trampoline* scaleTailsWinLose;
static Trampoline* scaleTailsRaceBar;
static Trampoline* scaleDemoPressStart;
static Trampoline* ChaoDX_Message_PlayerAction_Load_t;
static Trampoline* ChaoDX_Message_PlayerAction_Display_t;
static Trampoline* MissionCompleteScreen_Draw_t;
static Trampoline* CharSelBg_Display_t;
static Trampoline* TrialLevelList_Display_t;
static Trampoline* SubGameLevelList_Display_t;
static Trampoline* EmblemResultMenu_Display_t;
static Trampoline* FileSelect_Display_t;
static Trampoline* MenuObj_Display_t;
static Trampoline* OptionsMenu_Display_t;
static Trampoline* SoundTest_Display_t;
static Trampoline* GreenMenuRect_Draw_t;
static Trampoline* TutorialInstructionOverlay_Display_t;
static Trampoline* DisplayTitleCard_t;
static Trampoline* Credits_Main_t;
static Trampoline* PauseMenu_Map_Display_t;
static Trampoline* DrawSubtitles_t;
static Trampoline* EmblemCollected_Init_t;
static Trampoline* EmblemCollected_Main_t;
static Trampoline* DrawTitleScreen_t;

#pragma endregion

static void __cdecl scale_result_screen(ObjectMaster* _this)
{
	scale_push(Align::center, false);
	ScoreDisplay_Main(_this);
	scale_pop();
}

static void __cdecl HudDisplayRingTimeLife_Check_r()
{
	scale_trampoline(Align::automatic, false, HudDisplayRingTimeLife_Check_r, HudDisplayRingTimeLife_Check_t);
}

static void __cdecl HudDisplayScoreOrTimer_r()
{
	scale_trampoline(Align::left, false, HudDisplayScoreOrTimer_r, HudDisplayScoreOrTimer_t);
}

static void __cdecl DrawStageMissionImage_r(ObjectMaster* _this)
{
	scale_trampoline(Align::center, false, DrawStageMissionImage_r, DrawStageMissionImage_t, _this);
}

static short __cdecl DisplayPauseMenu_r()
{
	return scale_trampoline<short>(Align::center, false, DisplayPauseMenu_r, DisplayPauseMenu_t);
}

static void __cdecl LifeGauge_Main_r(ObjectMaster* a1)
{
	scale_trampoline(Align::right, false, LifeGauge_Main_r, LifeGauge_Main_t, a1);
}

static void __cdecl ScaleScoreA()
{
	scale_trampoline(Align::left, false, ScaleScoreA, scaleScoreA);
}

static void __cdecl ScaleTornadoHP(ObjectMaster* a1)
{
	scale_trampoline(Align::left | Align::bottom, false, ScaleTornadoHP, scaleTornadoHP, a1);
}

static void __cdecl ScaleTwinkleCircuitHUD(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, ScaleTwinkleCircuitHUD, scaleTwinkleCircuitHUD, a1);
}

static void __cdecl FishingHud_DrawHIT_r(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, FishingHud_DrawHIT_r, FishingHud_DrawHIT_t, a1);
}

static void __cdecl FishingHud_DrawReel_r()
{
	scale_trampoline(Align::right | Align::bottom, false, FishingHud_DrawReel_r, FishingHud_DrawReel_t);
}
static void __cdecl FishingHud_DrawRod_r()
{
	scale_trampoline(Align::right | Align::bottom, false, FishingHud_DrawRod_r, FishingHud_DrawRod_t);
}

static void __cdecl BigHud_DrawWeightAndLife_r(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, BigHud_DrawWeightAndLife_r, BigHud_DrawWeightAndLife_t, a1);
}
static void __cdecl FishingHud_DrawMeters_r(float length)
{
	scale_trampoline(Align::right | Align::bottom, false, FishingHud_DrawMeters_r, FishingHud_DrawMeters_t, length);
}

static void __cdecl ScaleAnimalPickup(ObjectMaster* a1)
{
	scale_trampoline(Align::right | Align::bottom, false, ScaleAnimalPickup, scaleAnimalPickup, a1);
}

static void __cdecl ScaleItemBoxSprite(ObjectMaster* a1)
{
	scale_trampoline(Align::bottom | Align::horizontal_center, false, ScaleItemBoxSprite, scaleItemBoxSprite, a1);
}

static void __cdecl ScaleBalls(ObjectMaster* a1)
{
	scale_trampoline(Align::right, false, ScaleBalls, scaleBalls, a1);
}

static void __cdecl ScaleCheckpointTime(int a1, int a2, int a3)
{
	scale_trampoline(Align::right | Align::bottom, false, ScaleCheckpointTime, scaleCheckpointTime, a1, a2, a3);
}

static void __cdecl ScaleEmeraldRadarA(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleEmeraldRadarA, scaleEmeraldRadarA, a1);
}

static void __cdecl ScaleEmeraldRadar_Grab(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleEmeraldRadar_Grab, scaleEmeraldRadar_Grab, a1);
}

static void __cdecl ScaleEmeraldRadarB(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleEmeraldRadarB, scaleEmeraldRadarB, a1);
}

static void __cdecl ScaleSandHillMultiplier(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleSandHillMultiplier, scaleSandHillMultiplier, a1);
}

static void __cdecl ScaleIceCapMultiplier(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleIceCapMultiplier, scaleIceCapMultiplier, a1);
}

static void __cdecl ScaleGammaTimeAddHud(ObjectMaster* a1)
{
	scale_trampoline(Align::right, false, ScaleGammaTimeAddHud, scaleGammaTimeAddHud, a1);
}
static void __cdecl ScaleGammaTimeRemaining(ObjectMaster* a1)
{
	scale_trampoline(Align::bottom | Align::horizontal_center, false, ScaleGammaTimeRemaining, scaleGammaTimeRemaining, a1);
}

static void __cdecl ScaleEmblemScreen(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, ScaleEmblemScreen, scaleEmblemScreen, a1);
}

static void __cdecl ScaleBossName(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, ScaleBossName, scaleBossName, a1);
}

static void __cdecl ScaleNightsCards(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleNightsCards, scaleNightsCards, a1);
}
static void __cdecl ScaleNightsJackpot(ObjectMaster* a1)
{
	scale_trampoline(Align::automatic, false, ScaleNightsJackpot, scaleNightsJackpot, a1);
}

static void __cdecl ScaleMissionStartClear(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, ScaleMissionStartClear, scaleMissionStartClear, a1);
}
static void __cdecl ScaleMissionTimer()
{

	scale_trampoline(Align::center, false, ScaleMissionTimer, scaleMissionTimer);
}
static void __cdecl ScaleMissionCounter()
{
	scale_trampoline(Align::center, false, ScaleMissionCounter, scaleMissionCounter);
}

static void __cdecl ScaleTailsWinLose(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, ScaleTailsWinLose, scaleTailsWinLose, a1);
}
static void __cdecl ScaleTailsRaceBar(ObjectMaster* a1)
{
	scale_trampoline(Align::horizontal_center | Align::bottom, false, ScaleTailsRaceBar, scaleTailsRaceBar, a1);
}

static void __cdecl ScaleDemoPressStart(ObjectMaster* a1)
{
	scale_trampoline(Align::right, false, ScaleDemoPressStart, scaleDemoPressStart, a1);
}

static void __cdecl ChaoDX_Message_PlayerAction_Load_r()
{
	scale_trampoline(Align::automatic, false, ChaoDX_Message_PlayerAction_Load_r, ChaoDX_Message_PlayerAction_Load_t);
}

static void __cdecl ChaoDX_Message_PlayerAction_Display_r(ObjectMaster* a1)
{
	scale_trampoline(Align::top | Align::right, false, ChaoDX_Message_PlayerAction_Display_r, ChaoDX_Message_PlayerAction_Display_t, a1);
}

void __cdecl MissionCompleteScreen_Draw_r()
{
	scale_trampoline(Align::center, false, MissionCompleteScreen_Draw_r, MissionCompleteScreen_Draw_t);
}

static void __cdecl CharSelBg_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, CharSelBg_Display_r, CharSelBg_Display_t, a1);
}

static void __cdecl TrialLevelList_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, TrialLevelList_Display_r, TrialLevelList_Display_t, a1);
}

static void __cdecl SubGameLevelList_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, SubGameLevelList_Display_r, SubGameLevelList_Display_t, a1);
}

static void __cdecl EmblemResultMenu_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, EmblemResultMenu_Display_r, EmblemResultMenu_Display_t, a1);
}

static void __cdecl FileSelect_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, FileSelect_Display_r, FileSelect_Display_t, a1);
}

static void __cdecl MenuObj_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, MenuObj_Display_r, MenuObj_Display_t, a1);
}

static void __cdecl OptionsMenu_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, OptionsMenu_Display_r, OptionsMenu_Display_t, a1);
}

static void __cdecl SoundTest_Display_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, false, SoundTest_Display_r, SoundTest_Display_t, a1);
}

static void __cdecl GreenMenuRect_Draw_r(float x, float y, float z, float width, float height)
{
	scale_trampoline(Align::center, false, GreenMenuRect_Draw_r, GreenMenuRect_Draw_t, x, y, z, width, height);
}

static void __cdecl TutorialInstructionOverlay_Display_r(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, TutorialInstructionOverlay_Display_r, TutorialInstructionOverlay_Display_t, a1);
}

static Sint32 __cdecl DisplayTitleCard_r()
{
	return scale_trampoline<Sint32>(Align::center, false, DisplayTitleCard_r, DisplayTitleCard_t);
}

static void __cdecl Credits_Main_r(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, Credits_Main_r, Credits_Main_t, a1);
}

static void __cdecl PauseMenu_Map_Display_r()
{
	scale_trampoline(Align::center, false, PauseMenu_Map_Display_r, PauseMenu_Map_Display_t);
}

static void __cdecl DrawSubtitles_r()
{
	scale_trampoline(Align::center, false, DrawSubtitles_r, DrawSubtitles_t);
}

static void EmblemCollected_Init_r(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, EmblemCollected_Init_r, EmblemCollected_Init_t, a1);
}

static void EmblemCollected_Main_r(ObjectMaster* a1)
{
	scale_trampoline(Align::center, false, EmblemCollected_Main_r, EmblemCollected_Main_t, a1);
}

#pragma pack(push, 1)
struct TitleScreenData
{
  int f0;
  void *field_4;
  void *field_8;
  float field_C;
  char gap_10[4];
  int field_14;
  int field_18;
  char gap_1c[8];
  _DWORD dword24;
  _DWORD dword28;
  uint8_t f2C[4];
  _DWORD dword30;
  float float34;
  uint8_t byte38;
  char _padding[3];
  char field_3C[36];
};
#pragma pack(pop)


static void __cdecl DrawTitleScreen_o(void* a1)
{
	auto orig = DrawTitleScreen_t->Target();

	__asm
	{
		mov esi, a1
		call orig
	}
}

DataPointer(int, dword_3B11180, 0x3B11180);

template<class T> int8_t __SETS__(T x)
{
	if (sizeof(T) == 1)
		return int8_t(x) < 0;
	if (sizeof(T) == 2)
		return int16_t(x) < 0;
	if (sizeof(T) == 4)
		return int32_t(x) < 0;
	return int64_t(x) < 0;
}

template <class T, class U>
int8_t __OFSUB__(T x, U y)
{
	if (sizeof(T) < sizeof(U))
	{
		U x2 = x;
		int8_t sx = __SETS__(x2);
		return (sx ^ __SETS__(y)) & (sx ^ __SETS__(x2 - y));
	}
	else
	{
		T y2 = y;
		int8_t sx = __SETS__(x);
		return (sx ^ __SETS__(y2)) & (sx ^ __SETS__(x - y2));
	}
}

static void __cdecl DrawTitleScreen_r(TitleScreenData* a1)
{
	auto v1 = a1->dword30;

	if (v1 >= 0x3C)
	{
		SetVtxColorA(0xFFFFFFFF);
	}
	else
	{
		SetVtxColorA(((char)(255 * v1 / 0x3C) << 24) + 0xFFFFFF);
	}

	auto original_fill_mode = bg_fill;

	SetVtxColorB(0xFFFFFFFF);
	njSetTexture(&ava_title_cmn_TEXLIST);
	auto v17  = 0;
	auto v279 = 0;

	bg_fill = FillMode::fill;
	scale_push(Align::center, true);

	// draw the scrolling background
	do
	{
		auto v18 = v17 + a1->byte38;

		if (v18 >= 10)
		{
			v18 -= 10;
		}

		int v19 = v18;
		float v20 = HorizontalStretch * 0.5f;
		float v21 = (float)(signed int)v279 * 256.0f - 64.0f;
		float scaleY = VerticalStretch * 0.46874997f;
		float v23 = (v21 - a1->float34) * v20;
		DrawBG(v18 + 27, v23, -16.0f, 2.0f, v20, scaleY);
		float v24 = VerticalStretch * 0.46874997f;
		float v25 = HorizontalStretch * 0.5f;
		float v26 = v24 * 256.0f - 16.0f;
		float v27 = (v21 - a1->float34) * v25;
		DrawBG(v19 + 37, v27, v26, 2.0f, v25, v24);
		auto v28 = VerticalStretch * 0.46874997f;
		auto v29 = HorizontalStretch * 0.5f;
		auto v30 = v28 * 512.0f - 16.0f;
		auto v31 = (v21 - a1->float34) * v29;
		DrawBG(v19 + 47, v31, v30, 2.0f, v29, v28);
		auto v32 = VerticalStretch * 0.46874997f;
		auto v33 = HorizontalStretch * 0.5f;
		auto v34 = v32 * 768.0f - 16.0f;
		auto v35 = (v21 - a1->float34) * v33;
		DrawBG(v19 + 57, v35, v34, 2.0, v33, v32);
		v279 = ++v17;
	} while (v17 < 9);

	scale_pop();

	auto v36 = (float)(dword_3B11180 + 1) * 4.21f + a1->float34;
	a1->float34 = v36;
	if (v36 >= 256.0f)
	{
		auto v37 = a1->byte38 + 1;
		auto v228 = (char)(a1->byte38 - 9) < 0;
		a1->float34 = 0.0f;
		a1->byte38 = v37;
		if (!((unsigned __int8)v228 ^ __OFSUB__(v37, 10)))
		{
			a1->byte38 = 0;
		}
	}
	
	auto v38 = 0;
	auto v280 = 0;

	// draw the top bar
	do
	{
		auto v39 = VerticalStretch * 0.46874997f;
		auto v40 = HorizontalStretch * 0.5f;
		auto v41 = HorizontalStretch * 128.0f * (float)(signed int)v280;
		DrawBG(v38++, v41, 0.0f, 1.3f, v40, v39);
		v280 = v38;
	} while (v38 < 5);

	auto v42 = 0;
	auto v281 = 0;

	// draw upper part of white circle thing
	do
	{
		auto v43 = VerticalStretch * 0.46874997f;
		auto v44 = HorizontalStretch * 0.5f;
		auto v45 = VerticalStretch * 120.0f;
		auto v46 = (float)(signed int)v281 * (HorizontalStretch * 128.0f)
		           + HorizontalStretch * 128.0f
		           + HorizontalStretch * 128.0f;
		DrawBG(v42++ + 5, v46, v45, 1.3f, v44, v43);
		v281 = v42;
	} while (v42 < 2);

	auto v47 = 0;
	auto v282 = 0;

	float v276 = 0.0f;
	float v275 = 0.0f;
	float scaleX = 0.0f;
	float v126 = 0.0f;

	// draw uh... second... upper part of white circle thing
	do
	{
		auto v48 = VerticalStretch * 0.46874997f;
		auto v49 = HorizontalStretch * 0.5f;
		auto v50 = VerticalStretch * 240.0f;
		auto v51 = (float)(signed int)v282 * (HorizontalStretch * 128.0f)
		           + HorizontalStretch * 128.0f
		           + HorizontalStretch * 128.0f;
		DrawBG(v47++ + 7, v51, v50, 1.3f, v49, v48);
		v282 = v47;
	} while (v47 < 2);

	auto v52 = 0;
	auto v283 = 0;

	// draw bottom bar
	do
	{
		auto v53 = VerticalStretch * 0.46874997f;
		auto v54 = HorizontalStretch * 0.5f;
		auto v55 = VerticalStretch * 360.0f;
		auto v56 = HorizontalStretch * 128.0f * (float)(signed int)v283;
		DrawBG(v52++ + 9, v56, v55, 1.3f, v54, v53);
		v283 = v52;
	} while (v52 < 5);

	auto v57 = a1->dword30;

	const auto w = static_cast<float>(HorizontalResolution);
	const auto h = static_cast<float>(VerticalResolution);

	bg_fill = (w / h < (4.0f / 3.0f)) ? FillMode::fill : FillMode::fit;
	scale_push(Align::center, true);

	if (v57 >= 0x50)
	{
		SetVtxColorB(0xFFFFFFFF);
		njSetTexture(&ava_title_cmn_TEXLIST);

		// draw static sonic
	#if true
		float v74 = VerticalStretch * 0.46874997f * 1.025f;
		float v75 = HorizontalStretch * 0.5f * 0.935f;
		float v76 = 0.0f * VerticalStretch;
		float v77 = -110.0f * HorizontalStretch;
		DrawBG(14, v77, v76, 1.2f, v75, v74);
		float v78 = VerticalStretch * 0.46874997f * 1.025f;
		float v79 = HorizontalStretch * 0.5f * 0.935f;
		float v80 = 0.0f * VerticalStretch;
		float v81 = -110.0f * HorizontalStretch + HorizontalStretch * 119.68f;
		DrawBG(15, v81, v80, 1.2f, v79, v78);
		float v82 = VerticalStretch * 0.46874997f * 1.025f;
		float v83 = HorizontalStretch * 0.5f * 0.935f;
		float v84 = 0.0f * VerticalStretch;
		float v85 = -110.0f * HorizontalStretch + HorizontalStretch * 239.36f;
		DrawBG(16, v85, v84, 1.2f, v83, v82);
		float v86 = VerticalStretch * 0.46874997f * 1.025f;
		float v87 = HorizontalStretch * 0.5f * 0.935f;
		float v88 = 0.0f * VerticalStretch + VerticalStretch * 123.0f;
		float v89 = -110.0f * HorizontalStretch + HorizontalStretch * 119.68f;
		DrawBG(17, v89, v88, 1.2f, v87, v86);
		float v90 = VerticalStretch * 0.46874997f * 1.025f;
		float v91 = HorizontalStretch * 0.5f * 0.935f;
		float v92 = 0.0f * VerticalStretch + VerticalStretch * 123.0f;
		float v93 = -110.0f * HorizontalStretch + HorizontalStretch * 239.36f;
		DrawBG(18, v93, v92, 1.2f, v91, v90);
		float v94 = VerticalStretch * 0.46874997f * 1.025f;
		float v95 = HorizontalStretch * 0.5f * 0.935f;
		float v96 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v97 = -110.0f * HorizontalStretch;
		DrawBG(19, v97, v96, 1.2f, v95, v94);
		float v98 = VerticalStretch * 0.46874997f * 1.025f;
		float v99 = HorizontalStretch * 0.5f * 0.935f;
		float v100 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v101 = -110.0f * HorizontalStretch + HorizontalStretch * 119.68f;
		DrawBG(20, v101, v100, 1.2f, v99, v98);
		float v102 = VerticalStretch * 0.46874997f * 1.025f;
		float v103 = HorizontalStretch * 0.5f * 0.935f;
		float v104 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v105 = -110.0f * HorizontalStretch + HorizontalStretch * 239.36f;
		DrawBG(21, v105, v104, 1.2f, v103, v102);
		float v106 = VerticalStretch * 0.46874997f * 1.025f;
		float v107 = HorizontalStretch * 0.5f * 0.935f;
		float v108 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v109 = -110.0f * HorizontalStretch + HorizontalStretch * 359.04001f;
		DrawBG(22, v109, v108, 1.2f, v107, v106);
		float v110 = VerticalStretch * 0.46874997f * 1.025f;
		float v111 = HorizontalStretch * 0.5f * 0.935f;
		float v112 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v113 = -110.0f * HorizontalStretch;
		DrawBG(23, v113, v112, 1.2f, v111, v110);
		float v114 = VerticalStretch * 0.46874997f * 1.025f;
		float v115 = HorizontalStretch * 0.5f * 0.935f;
		float v116 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v117 = -110.0f * HorizontalStretch + HorizontalStretch * 119.68f;
		DrawBG(24, v117, v116, 1.2f, v115, v114);
		float v118 = VerticalStretch * 0.46874997f * 1.025f;
		float v119 = HorizontalStretch * 0.5f * 0.935f;
		float v120 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v121 = -110.0f * HorizontalStretch + HorizontalStretch * 239.36f;
		DrawBG(25, v121, v120, 1.2f, v119, v118);
		float v122 = VerticalStretch * 0.46874997f * 1.025f;
		float v123 = HorizontalStretch * 0.5f * 0.935f;
		float v124 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v125 = -110.0f * HorizontalStretch + HorizontalStretch * 359.04001f;
		DrawBG(26, v125, v124, 1.2f, v123, v122);
	#endif

		// draw logo
		njSetTexture(&ava_gtitle0_e_TEXLIST);
		float v127 = VerticalStretch * 0.46874997f;
		float v128 = HorizontalStretch * 0.5f;
		float v129 = 107.0f * VerticalStretch;
		float v130 = 149.0f * HorizontalStretch;
		DrawBG(0, v130, v129, 1.1f, v128, v127);
		float v131 = VerticalStretch * 0.46874997f;
		float v132 = HorizontalStretch * 0.5f;
		float v133 = 107.0f * VerticalStretch;
		float v134 = 149.0f * HorizontalStretch + HorizontalStretch * 128.0f;
		DrawBG(1, v134, v133, 1.1f, v132, v131);
		float v135 = VerticalStretch * 0.46874997f;
		float v136 = HorizontalStretch * 0.5f;
		float v137 = 107.0f * VerticalStretch;
		float v138 = 149.0f * HorizontalStretch + HorizontalStretch * 256.0f;
		DrawBG(2, v138, v137, 1.1f, v136, v135);
		float v139 = VerticalStretch * 0.46874997f;
		float v140 = HorizontalStretch * 0.5f;
		float v141 = 107.0f * VerticalStretch;
		float v142 = 149.0f * HorizontalStretch + HorizontalStretch * 384.0f;
		DrawBG(3, v142, v141, 1.1f, v140, v139);
		float v143 = VerticalStretch * 0.46874997f;
		float v144 = HorizontalStretch * 0.5f;
		float v145 = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
		float v146 = 149.0f * HorizontalStretch;
		DrawBG(4, v146, v145, 1.1f, v144, v143);
		float v147 = VerticalStretch * 0.46874997f;
		float v148 = HorizontalStretch * 0.5f;
		float v149 = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
		float v150 = 149.0f * HorizontalStretch + HorizontalStretch * 128.0f;
		DrawBG(5, v150, v149, 1.1f, v148, v147);
		float v151 = VerticalStretch * 0.46874997f;
		float v152 = HorizontalStretch * 0.5f;
		float v153 = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
		float v154 = 149.0f * HorizontalStretch + HorizontalStretch * 256.0f;
		DrawBG(6, v154, v153, 1.1f, v152, v151);
		v276 = VerticalStretch * 0.46874997f;
		v275 = HorizontalStretch * 0.5f;
		scaleX = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
		v126 = 149.0f * HorizontalStretch;
	LABEL_56:
		float v259 = v126 + HorizontalStretch * 384.0f;
		DrawBG(7, v259, scaleX, 1.1f, v275, v276);
		goto LABEL_57;
	}

	// slide into dms
	if (v57 >= 0x32)
	{
		float v284;

		if (v57 >= 0x37)
		{
			v284 = /*HorizontalStretch <= 1.0f ? 160.0f :*/ -110.0;
		}
		else
		{
			auto v157 = v57 - 50;
			auto v158 = /*HorizontalStretch <= 1.0f ? 160.0f - 863.0f :*/ -110.0f - 863.0f;
			v284 = v158 * (float)(unsigned int)v157 * 0.2f + 863.0f;
		}

		njSetTexture(&ava_title_cmn_TEXLIST);

		// draw sonic
	#if true
		float v175 = VerticalStretch * 0.46874997f * 1.025f;
		float v176 = HorizontalStretch * 0.5f * 0.935f;
		float v177 = 0.0f * VerticalStretch;
		float v178 = HorizontalStretch * v284;
		DrawBG(14, v178, v177, 1.2f, v176, v175);
		float v179 = VerticalStretch * 0.46874997f * 1.025f;
		float v180 = HorizontalStretch * 0.5f * 0.935f;
		float v181 = 0.0f * VerticalStretch;
		float v182 = HorizontalStretch * v284 + HorizontalStretch * 119.68f;
		DrawBG(15, v182, v181, 1.2f, v180, v179);
		float v183 = VerticalStretch * 0.46874997f * 1.025f;
		float v184 = HorizontalStretch * 0.5f * 0.935f;
		float v185 = 0.0f * VerticalStretch;
		float v186 = HorizontalStretch * v284 + HorizontalStretch * 239.36f;
		DrawBG(16, v186, v185, 1.2f, v184, v183);
		float v187 = VerticalStretch * 0.46874997f * 1.025f;
		float v188 = HorizontalStretch * 0.5f * 0.935f;
		float v189 = 0.0f * VerticalStretch + VerticalStretch * 123.0f;
		float v190 = HorizontalStretch * v284 + HorizontalStretch * 119.68f;
		DrawBG(17, v190, v189, 1.2f, v188, v187);
		float v191 = VerticalStretch * 0.46874997f * 1.025f;
		float v192 = HorizontalStretch * 0.5f * 0.935f;
		float v193 = 0.0f * VerticalStretch + VerticalStretch * 123.0f;
		float v194 = HorizontalStretch * v284 + HorizontalStretch * 239.36f;
		DrawBG(18, v194, v193, 1.2f, v192, v191);
		float v195 = VerticalStretch * 0.46874997f * 1.025f;
		float v196 = HorizontalStretch * 0.5f * 0.935f;
		float v197 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v198 = HorizontalStretch * v284;
		DrawBG(19, v198, v197, 1.2f, v196, v195);
		float v199 = VerticalStretch * 0.46874997f * 1.025f;
		float v200 = HorizontalStretch * 0.5f * 0.935f;
		float v201 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v202 = HorizontalStretch * v284 + HorizontalStretch * 119.68f;
		DrawBG(20, v202, v201, 1.2f, v200, v199);
		float v203 = VerticalStretch * 0.46874997f * 1.025f;
		float v204 = HorizontalStretch * 0.5f * 0.935f;
		float v205 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v206 = HorizontalStretch * v284 + HorizontalStretch * 239.36f;
		DrawBG(21, v206, v205, 1.2f, v204, v203);
		float v207 = VerticalStretch * 0.46874997f * 1.025f;
		float v208 = HorizontalStretch * 0.5f * 0.935f;
		float v209 = 0.0f * VerticalStretch + VerticalStretch * 246.0f;
		float v210 = HorizontalStretch * v284 + HorizontalStretch * 359.04001f;
		DrawBG(22, v210, v209, 1.2f, v208, v207);
		float v211 = VerticalStretch * 0.46874997f * 1.025f;
		float v212 = HorizontalStretch * 0.5f * 0.935f;
		float v213 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v214 = HorizontalStretch * v284;
		DrawBG(23, v214, v213, 1.2f, v212, v211);
		float v215 = VerticalStretch * 0.46874997f * 1.025f;
		float v216 = HorizontalStretch * 0.5f * 0.935f;
		float v217 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v218 = HorizontalStretch * v284 + HorizontalStretch * 119.68f;
		DrawBG(24, v218, v217, 1.2f, v216, v215);
		float v219 = VerticalStretch * 0.46874997f * 1.025f;
		float v220 = HorizontalStretch * 0.5f * 0.935f;
		float v221 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v222 = HorizontalStretch * v284 + HorizontalStretch * 239.36f;
		DrawBG(25, v222, v221, 1.2f, v220, v219);
		float v223 = VerticalStretch * 0.46874997f * 1.025f;
		float v224 = HorizontalStretch * 0.5f * 0.935f;
		float v225 = 0.0f * VerticalStretch + VerticalStretch * 369.0f;
		float v226 = HorizontalStretch * v284 + HorizontalStretch * 359.04001f;
		DrawBG(26, v226, v225, 1.2f, v224, v223);
	#endif

		// logo
		auto v227 = a1->dword30;
		if (v227 >= 0x46)
		{
			float v229;
			int v228;

			if (v227 > 0x4D)
			{
				v229 = (float)(signed int)(84 - v227);
				v228 = (signed int)(84 - v227) < 0;
			}
			else
			{
				v229 = (float)(signed int)(v227 - 70);
				v228 = (signed int)(v227 - 70) < 0;
			}
			if (v228)
			{
				v229 = (float)(v229 + 4294967300.0);
			}
			float v230 = v229 * 120.0f + -416.0f;
			njSetTexture(&ava_gtitle0_e_TEXLIST);
			float v231 = VerticalStretch * 0.46874997f;
			float v232 = HorizontalStretch * 0.5f;
			float v233 = 107.0f * VerticalStretch;
			float v234 = HorizontalStretch * v230;
			DrawBG(0, v234, v233, 1.1f, v232, v231);
			float v235 = VerticalStretch * 0.46874997f;
			float v236 = HorizontalStretch * 0.5f;
			float v237 = 107.0f * VerticalStretch;
			float v238 = HorizontalStretch * v230 + HorizontalStretch * 128.0f;
			DrawBG(1, v238, v237, 1.1f, v236, v235);
			float v239 = VerticalStretch * 0.46874997f;
			float v240 = HorizontalStretch * 0.5f;
			float v241 = 107.0f * VerticalStretch;
			float v242 = HorizontalStretch * v230 + HorizontalStretch * 256.0f;
			DrawBG(2, v242, v241, 1.1f, v240, v239);
			float v243 = VerticalStretch * 0.46874997f;
			float v244 = HorizontalStretch * 0.5f;
			float v245 = 107.0f * VerticalStretch;
			float v246 = HorizontalStretch * v230 + HorizontalStretch * 384.0f;
			DrawBG(3, v246, v245, 1.1f, v244, v243);
			float v247 = VerticalStretch * 0.46874997f;
			float v248 = HorizontalStretch * 0.5f;
			float v249 = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
			float v250 = HorizontalStretch * v230;
			DrawBG(4, v250, v249, 1.1f, v248, v247);
			float v251 = VerticalStretch * 0.46874997f;
			float v252 = HorizontalStretch * 0.5f;
			float v253 = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
			float v254 = HorizontalStretch * v230 + HorizontalStretch * 128.0f;
			DrawBG(5, v254, v253, 1.1f, v252, v251);
			float v255 = VerticalStretch * 0.46874997f;
			float v256 = HorizontalStretch * 0.5f;
			float v257 = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
			float v258 = HorizontalStretch * v230 + HorizontalStretch * 256.0f;
			DrawBG(6, v258, v257, 1.1f, v256, v255);
			v276 = VerticalStretch * 0.46874997f;
			v275 = HorizontalStretch * 0.5f;
			scaleX = 107.0f * VerticalStretch + VerticalStretch * 120.0f;
			v126 = HorizontalStretch * v230;
			goto LABEL_56;
		}
	}

	scale_pop();

LABEL_57:
	if (a1->dword24 < 150)
	{
		a1->dword30 += MissedFrames_B;
	}
	else
	{
		if (TextLanguage)
		{
			njSetTexture(&ava_gtitle0_e_TEXLIST);
		}
		else
		{
			njSetTexture(&TexList_Ava_Gtitle0);
		}

		float v285;
		float v287;

		if (TextLanguage)
		{
			v285 = 278.0f;
			v287 = 342.0f;
		}
		else
		{
			v285 = 260.0f;
			v287 = 333.0f;
		}
		auto v261 = a1->dword28;
		auto v260 = a1->dword28;
		if ((a1->dword28 & 0x1FFu) >= 0x100)
		{
			v260 = -1 - v261;
		}
		SetVtxColorB((v260 << 24) + 0xFFFFFF);
		Direct3D_SetZFunc(7u);
		auto v262 = VerticalStretch * 0.46874997f;
		auto v263 = HorizontalStretch * 0.5f;
		auto v264 = VerticalStretch * v287;
		auto v265 = HorizontalStretch * v285;
		// draw PRESS ENTER
		DrawBG(8, v265, v264, 1.1f, v263, v262);
		auto v266 = VerticalStretch * 0.46874997f;
		auto v267 = HorizontalStretch * 0.5f;
		auto v268 = VerticalStretch * v287;
		auto v269 = HorizontalStretch * v285 + HorizontalStretch * 128.0f;
		DrawBG(9, v269, v268, 1.1f, v267, v266);
		if (!TextLanguage)
		{
			auto v270 = VerticalStretch * 0.46874997f;
			auto v271 = HorizontalStretch * 0.5f;
			auto v272 = VerticalStretch * v287;
			auto v273 = HorizontalStretch * v285 + HorizontalStretch * 256.0f;
			DrawBG(10, v273, v272, 1.1f, v271, v270);
		}
		Direct3D_SetZFunc(1u);
		a1->dword30 += MissedFrames_B;
	}

	bg_fill = original_fill_mode;
}

static void __declspec(naked) DrawTitleScreen_asm()
{
	__asm
	{
		push esi
		call DrawTitleScreen_r
		pop esi
		ret
	}
}

void uiscale::initialize()
{
	update_parameters();
	njDrawTextureMemList_init();

	MissionCompleteScreen_Draw_t         = new Trampoline(0x00590690, 0x00590695, MissionCompleteScreen_Draw_r);
	CharSelBg_Display_t                  = new Trampoline(0x00512450, 0x00512455, CharSelBg_Display_r);
	TrialLevelList_Display_t             = new Trampoline(0x0050B410, 0x0050B415, TrialLevelList_Display_r);
	SubGameLevelList_Display_t           = new Trampoline(0x0050A640, 0x0050A645, SubGameLevelList_Display_r);
	EmblemResultMenu_Display_t           = new Trampoline(0x0050DFD0, 0x0050DFD5, EmblemResultMenu_Display_r);
	FileSelect_Display_t                 = new Trampoline(0x00505550, 0x00505555, FileSelect_Display_r);
	MenuObj_Display_t                    = new Trampoline(0x00432480, 0x00432487, MenuObj_Display_r);
	OptionsMenu_Display_t                = new Trampoline(0x00509810, 0x00509815, OptionsMenu_Display_r);
	SoundTest_Display_t                  = new Trampoline(0x00511390, 0x00511395, SoundTest_Display_r);
	GreenMenuRect_Draw_t                 = new Trampoline(0x004334F0, 0x004334F5, GreenMenuRect_Draw_r);
	TutorialInstructionOverlay_Display_t = new Trampoline(0x006430F0, 0x006430F7, TutorialInstructionOverlay_Display_r);
	DisplayTitleCard_t                   = new Trampoline(0x0047E170, 0x0047E175, DisplayTitleCard_r);
	Credits_Main_t                       = new Trampoline(0x006411A0, 0x006411A5, Credits_Main_r);
	PauseMenu_Map_Display_t              = new Trampoline(0x00458B00, 0x00458B06, PauseMenu_Map_Display_r);
	EmblemCollected_Init_t               = new Trampoline(0x004B4860, 0x004B4867, EmblemCollected_Init_r);
	EmblemCollected_Main_t               = new Trampoline(0x004B46A0, 0x004B46A6, EmblemCollected_Main_r);
	DrawTitleScreen_t                    = new Trampoline(0x0050E470, 0x0050E476, DrawTitleScreen_asm);

	DrawSubtitles_t = new Trampoline(0x0040D4D0, 0x0040D4D9, DrawSubtitles_r);
	WriteCall(reinterpret_cast<void*>(reinterpret_cast<size_t>(DrawSubtitles_t->Target()) + 4), reinterpret_cast<void*>(0x00402F00));

	// Fixes character scale on character select screen.
	WriteData(reinterpret_cast<const float**>(0x0051285E), &patch_dummy);

	WriteJump(reinterpret_cast<void*>(0x0042BEE0), scale_result_screen);

	DisplayAllObjects_t = new Trampoline(0x0040B540, 0x0040B546, DisplayAllObjects_r);
	WriteCall(reinterpret_cast<void*>(reinterpret_cast<size_t>(DisplayAllObjects_t->Target()) + 1), reinterpret_cast<void*>(0x004128F0));

	HudDisplayRingTimeLife_Check_t = new Trampoline(0x00425F90, 0x00425F95, HudDisplayRingTimeLife_Check_r);
	HudDisplayScoreOrTimer_t       = new Trampoline(0x00427F50, 0x00427F55, HudDisplayScoreOrTimer_r);
	DrawStageMissionImage_t        = new Trampoline(0x00457120, 0x00457126, DrawStageMissionImage_r);

	DisplayPauseMenu_t = new Trampoline(0x00415420, 0x00415425, DisplayPauseMenu_r);

	LifeGauge_Main_t = new Trampoline(0x004B3830, 0x004B3837, LifeGauge_Main_r);

	scaleScoreA = new Trampoline(0x00628330, 0x00628335, ScaleScoreA);

	WriteData(reinterpret_cast<const float**>(0x006288C2), &patch_dummy);
	scaleTornadoHP = new Trampoline(0x00628490, 0x00628496, ScaleTornadoHP);

	// TODO: Consider tracking down the individual functions so that they can be individually aligned.
	scaleTwinkleCircuitHUD = new Trampoline(0x004DB5E0, 0x004DB5E5, ScaleTwinkleCircuitHUD);

	// Rod scaling
	FishingHud_DrawReel_t      = new Trampoline(0x0046C9F0, 0x0046C9F5, FishingHud_DrawReel_r);
	FishingHud_DrawRod_t       = new Trampoline(0x0046CAB0, 0x0046CAB9, FishingHud_DrawRod_r);
	FishingHud_DrawMeters_t    = new Trampoline(0x0046CC70, 0x0046CC75, FishingHud_DrawMeters_r);
	FishingHud_DrawHIT_t       = new Trampoline(0x0046C920, 0x0046C926, FishingHud_DrawHIT_r);
	BigHud_DrawWeightAndLife_t = new Trampoline(0x0046FB00, 0x0046FB05, BigHud_DrawWeightAndLife_r);

	scaleAnimalPickup = new Trampoline(0x0046B330, 0x0046B335, ScaleAnimalPickup);

	scaleItemBoxSprite = new Trampoline(0x004C0790, 0x004C0795, ScaleItemBoxSprite);

	scaleBalls = new Trampoline(0x005C0B70, 0x005C0B75, ScaleBalls);

	scaleCheckpointTime = new Trampoline(0x004BABE0, 0x004BABE5, ScaleCheckpointTime);
	WriteData(reinterpret_cast<const float**>(0x0044F2E1), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x0044F30B), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x00476742), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x0047676A), &patch_dummy);

	// EmeraldRadarHud_Load
	WriteData(reinterpret_cast<const float**>(0x00475BE3), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x00475C00), &patch_dummy);
	// Emerald get
	WriteData(reinterpret_cast<const float**>(0x00477E8E), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x00477EC0), &patch_dummy);

	scaleEmeraldRadarA = new Trampoline(0x00475A70, 0x00475A75, ScaleEmeraldRadarA);
	scaleEmeraldRadarB = new Trampoline(0x00475E50, 0x00475E55, ScaleEmeraldRadarB);
	scaleEmeraldRadar_Grab = new Trampoline(0x00475D50, 0x00475D55, ScaleEmeraldRadar_Grab);

	scaleSandHillMultiplier = new Trampoline(0x005991A0, 0x005991A6, ScaleSandHillMultiplier);
	scaleIceCapMultiplier = new Trampoline(0x004EC120, 0x004EC125, ScaleIceCapMultiplier);

	WriteData(reinterpret_cast<const float**>(0x0049FF70), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x004A005B), &patch_dummy);
	WriteData(reinterpret_cast<const float**>(0x004A0067), &patch_dummy);
	scaleGammaTimeAddHud = new Trampoline(0x0049FDA0, 0x0049FDA5, ScaleGammaTimeAddHud);
	scaleGammaTimeRemaining = new Trampoline(0x004C51D0, 0x004C51D7, ScaleGammaTimeRemaining);

	WriteData(reinterpret_cast<float**>(0x004B4470), &scale_h);
	WriteData(reinterpret_cast<float**>(0x004B444E), &scale_v);
	scaleEmblemScreen = new Trampoline(0x004B4200, 0x004B4205, ScaleEmblemScreen);

	scaleBossName = new Trampoline(0x004B33D0, 0x004B33D5, ScaleBossName);

	scaleNightsCards = new Trampoline(0x005D73F0, 0x005D73F5, ScaleNightsCards);
	WriteData(reinterpret_cast<float**>(0x005D701B), &scale_h);
	scaleNightsJackpot = new Trampoline(0x005D6E60, 0x005D6E67, ScaleNightsJackpot);

	scaleMissionStartClear = new Trampoline(0x00591260, 0x00591268, ScaleMissionStartClear);

	scaleMissionTimer = new Trampoline(0x00592D50, 0x00592D59, ScaleMissionTimer);
	scaleMissionCounter = new Trampoline(0x00592A60, 0x00592A68, ScaleMissionCounter);

	scaleTailsWinLose = new Trampoline(0x0047C480, 0x0047C485, ScaleTailsWinLose);
	scaleTailsRaceBar = new Trampoline(0x0047C260, 0x0047C267, ScaleTailsRaceBar);

	scaleDemoPressStart = new Trampoline(0x00457D30, 0x00457D36, ScaleDemoPressStart);

#if 0
	ChaoDX_Message_PlayerAction_Load_t = new Trampoline(0x0071B3B0, 0x0071B3B7, ChaoDX_Message_PlayerAction_Load_r);
	ChaoDX_Message_PlayerAction_Display_t = new Trampoline(0x0071B210, 0x0071B215, ChaoDX_Message_PlayerAction_Display_r);
#endif
}

#pragma endregion

#pragma region Background scaling

static Trampoline* TutorialBackground_Display_t;
static Trampoline* RecapBackground_Main_t;
static Trampoline* EndBG_Display_t;
static Trampoline* DrawTiledBG_AVA_BACK_t;
static Trampoline* DrawAVA_TITLE_BACK_t;
static Trampoline* DisplayLogoScreen_t;

static void __cdecl DrawAVA_TITLE_BACK_r(float depth)
{
	scale_trampoline(Align::center, true, DrawAVA_TITLE_BACK_r, DrawAVA_TITLE_BACK_t, depth);
}

static void __cdecl DrawTiledBG_AVA_BACK_r(float depth)
{
	scale_trampoline(Align::center, true, DrawTiledBG_AVA_BACK_r, DrawTiledBG_AVA_BACK_t, depth);
}

static void __cdecl RecapBackground_Main_r(ObjectMaster *a1)
{
	scale_trampoline(Align::center, true, RecapBackground_Main_r, RecapBackground_Main_t, a1);
}

static void __cdecl DisplayLogoScreen_r(Uint8 index)
{
	scale_trampoline(Align::center, true, DisplayLogoScreen_r, DisplayLogoScreen_t, index);
}

static void __cdecl TutorialBackground_Display_r(ObjectMaster* a1)
{
	auto orig = bg_fill;
	bg_fill = FillMode::fit;
	scale_trampoline(Align::center, true, TutorialBackground_Display_r, TutorialBackground_Display_t, a1);
	bg_fill = orig;
}

static void __cdecl EndBG_Display_r(ObjectMaster* a1)
{
	auto orig = bg_fill;
	bg_fill = FillMode::fit;
	scale_trampoline(Align::center, true, EndBG_Display_r, EndBG_Display_t, a1);
	bg_fill = orig;
}

void uiscale::setup_background_scale()
{
	update_parameters();
	njDrawTextureMemList_init();

	TutorialBackground_Display_t = new Trampoline(0x006436B0, 0x006436B7, TutorialBackground_Display_r);
	RecapBackground_Main_t       = new Trampoline(0x00643C90, 0x00643C95, RecapBackground_Main_r);
	EndBG_Display_t              = new Trampoline(0x006414A0, 0x006414A7, EndBG_Display_r);
	DrawTiledBG_AVA_BACK_t       = new Trampoline(0x00507BB0, 0x00507BB5, DrawTiledBG_AVA_BACK_r);
	DrawAVA_TITLE_BACK_t         = new Trampoline(0x0050BA90, 0x0050BA96, DrawAVA_TITLE_BACK_r);
	DisplayLogoScreen_t          = new Trampoline(0x0042CB20, 0x0042CB28, DisplayLogoScreen_r);
}

#pragma endregion

#pragma region FMV scaling

static Trampoline* DisplayVideoFrame_t = nullptr;

static void __cdecl DisplayVideoFrame_r(int width, int height)
{
	auto orig = bg_fill;
	bg_fill = fmv_fill;
	scale_trampoline(Align::center, true, DisplayVideoFrame_r, DisplayVideoFrame_t, width, height);
	bg_fill = orig;
}

void uiscale::setup_fmv_scale()
{
	update_parameters();
	njDrawTextureMemList_init();

	DisplayVideoFrame_t = new Trampoline(0x005139F0, 0x005139F5, DisplayVideoFrame_r);
}

#pragma endregion
