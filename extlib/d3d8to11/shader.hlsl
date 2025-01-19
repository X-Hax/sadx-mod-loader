// Before including the shader library header, define OIT_NODE_WRITE
// so that the OIT subsystem knows that, if anything, we will be
// storing OIT nodes rather than reading them.
#define OIT_NODE_WRITE

#include "d3d8to11.hlsl"

// When UBER_DEMO_MODE is defined, uber shaders will replace the RGB
// components of the pixel shader result with red, so that uber shader
// use is visible in the scene.
//#define UBER_DEMO_MODE

VS_OUTPUT vs_main(VS_INPUT input)
{
	return fixed_func_vs(input);
}

float4 ps_main(VS_OUTPUT input) : SV_TARGET
{
	float4 diffuse;
	float4 specular;

	get_input_colors(input, diffuse, specular);

	float4 result = handle_texture_stages(input, diffuse, specular);
	result = apply_fog(result, input.fog);

	const bool standard_blending = is_standard_blending();

	do_alpha_test(result, standard_blending);
	do_oit(result, input, standard_blending);

#if UBER == 1 && defined(UBER_DEMO_MODE)
	result.rgb = float3(1, 0, 0);
#endif

	return result;
}
