#include "include.hlsli"

// When defined, only half of the screen is alpha sorted,
// and a red line is drawn down the middle.
//#define OIT_DEMO_MODE

// When defined, sorting is completely disabled.
//#define OIT_DISABLE_SORT

// When defined, composition outputs a solid color which
// is proportional to OIT_MAX_FRAGMENTS at each pixel.
//#define OIT_SHOW_FRAGMENT_OVERDRAW

// FIXME: PerSceneBuffer copy/pasted from d3d8to11.hlsl
cbuffer PerSceneBuffer : register(b1)
{
	matrix view_matrix;
	matrix projection_matrix;
	float2 screen_dimensions;
	float3 view_position;
	uint   oit_buffer_length;
}

/*
	This shader draws a full screen triangle onto
	which it composites the stored alpha fragments.
*/

struct VertexOutput
{
	float4 position : SV_POSITION;
};

VertexOutput vs_main(uint vertex_id : SV_VERTEXID)
{
	VertexOutput output;
	float2 texcoord = float2((vertex_id << 1) & 2, vertex_id & 2);
	output.position = float4(texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
	return output;
}

float4 get_blend_factor(uint mode, float4 source, float4 destination)
{
	switch (mode)
	{
		default: // error state
			return float4(1, 0, 0, 1);
		case BLEND_ZERO:
			return float4(0, 0, 0, 0);
		case BLEND_ONE:
			return float4(1, 1, 1, 1);
		case BLEND_SRCCOLOR:
			return source;
		case BLEND_INVSRCCOLOR:
			return 1.0f - source;
		case BLEND_SRCALPHA:
			return source.aaaa;
		case BLEND_INVSRCALPHA:
			return 1.0f - source.aaaa;
		case BLEND_DESTALPHA:
			return destination.aaaa;
		case BLEND_INVDESTALPHA:
			return 1.0f - destination.aaaa;
		case BLEND_DESTCOLOR:
			return destination;
		case BLEND_INVDESTCOLOR:
			return 1.0f - destination;
		case BLEND_SRCALPHASAT:
			float f = min(source.a, 1 - destination.a);
			return float4(f, f, f, 1);
	}
}

float4 blend_colors(uint blend_op, uint source_blend, uint destination_blend, float4 source_color, float4 destination_color)
{
	float4 source_factor      = get_blend_factor(source_blend, source_color, destination_color);
	float4 destination_factor = get_blend_factor(destination_blend, source_color, destination_color);

	float4 source_result      = source_color * source_factor;
	float4 destination_result = destination_color * destination_factor;

	switch (blend_op)
	{
		default:
			return float4(1, 0, 0, 1);

		case BLENDOP_ADD:
			return source_result + destination_result;
		case BLENDOP_REVSUBTRACT:
			return destination_result - source_result;
		case BLENDOP_SUBTRACT:
			return source_result - destination_result;
		case BLENDOP_MIN:
			return min(source_result, destination_result);
		case BLENDOP_MAX:
			return max(source_result, destination_result);
			/*
		case BLENDOP_DIVIDE:
			return source_result / destination_result;
		case BLENDOP_MULTIPLY:
			return source_result * destination_result;
		case BLENDOP_DODGE:
			return source_result / (1.0 - destination_result);
			*/
	}
}

float4 ps_main(VertexOutput input) : SV_TARGET
{
	int2 pos = int2(input.position.xy);

#ifdef OIT_DEMO_MODE
	const int center = screen_dimensions.x / 2;
	const bool should_sort = pos.x >= center;

	if (abs(center.x - pos.x) < 4)
	{
		return float4(1, 0, 0, 1);
	}
#endif

	float4 back_buffer_color = back_buffer[pos];
	uint index = frag_list_head[pos];

	// FIXME: LotR: RotK is bailing here!
	if (index == OIT_FRAGMENT_LIST_NULL)
	{
		return back_buffer_color;
	}

	float opaque_depth = depth_buffer[pos].r;

	uint indices[OIT_MAX_FRAGMENTS];
	uint count = 0;

	while (index != OIT_FRAGMENT_LIST_NULL && count < OIT_MAX_FRAGMENTS)
	{
		const OitNode node_i = frag_list_nodes[index];

		if (node_i.depth > opaque_depth)
		{
			index = node_i.next;
			continue;
		}

		if (!count)
		{
			indices[0] = index;
			++count;
			index = node_i.next;
			continue;
		}

		int j = count;

	#ifndef OIT_DISABLE_SORT
		OitNode node_j = frag_list_nodes[indices[j - 1]];

		uint draw_call_i = (node_i.flags >> 16) & 0xFFFF;
		uint draw_call_j = (node_j.flags >> 16) & 0xFFFF;

		while (j > 0 &&
		       #ifdef OIT_DEMO_MODE
		       ((should_sort && node_j.depth > node_i.depth) || ((!should_sort || node_j.depth == node_i.depth) && draw_call_j < draw_call_i))
		       #else
		       (node_j.depth > node_i.depth || (node_j.depth == node_i.depth && draw_call_j < draw_call_i))
		       #endif
		)
		{
			uint temp = indices[j];
			indices[j] = indices[--j];
			indices[j] = temp;
			node_j = frag_list_nodes[indices[j - 1]];
			draw_call_j = (node_j.flags >> 16) & 0xFFFF;
		}
	#endif

		indices[j] = index;
		index = node_i.next;
		++count;
	}

	if (count == 0)
	{
		return back_buffer_color;
	}

#ifdef OIT_SHOW_FRAGMENT_OVERDRAW
	return float4((float)count / OIT_MAX_FRAGMENTS, 0, 0, 0);
#endif

	float4 final = back_buffer_color;

	for (int i = count - 1; i >= 0; i--)
	{
		const OitNode fragment = frag_list_nodes[indices[i]];
		uint blend_flags = fragment.flags;

		uint blend_op          = (blend_flags >> 8) & 0xF;
		uint source_blend      = (blend_flags >> 4) & 0xF;
		uint destination_blend = (blend_flags & 0xF);

		float4 color = unorm_to_float4(fragment.color);
		final = blend_colors(blend_op, source_blend, destination_blend, color, final);
	}

	return float4(final.rgb, 1);
}
