#include "stdafx.h"

#include <FunctionHook.h>

#include "polybuff.h"

namespace polybuff
{
	int alignment_probably = 0;
	int count = 0;
	void* ptr = nullptr;
}

namespace
{
	FunctionHook<void, int, int, void*> InitPolyBuffers_t(0x0078E720);

	void __cdecl InitPolyBuffers_r(int alignment_probably, int count, void* ptr)
	{
		polybuff::alignment_probably = alignment_probably;
		polybuff::count = count;
		polybuff::ptr = ptr;

		InitPolyBuffers_t.Original(alignment_probably, count, ptr);
	}
}

void polybuff::init()
{
	InitPolyBuffers_t.Hook(InitPolyBuffers_r);
}

/*
 * Despite having designated polybuff drawing functions
 * specifically for meshes with vertex color, vanilla SADX
 * deliberately ignores the mesh-provided vertex color,
 * and instead uses a global vertex color.
 *
 * It uses that global vertex color EVEN AFTER IT CHECKS
 * WHETHER OR NOT IT SHOULD. As in, the code in the "if"
 * and the "else" ARE 100% IDENTICAL IN FUNCTIONALITY.
 *
 * The code below re-implements the functions to handle the
 * secondary case CORRECTLY and use the mesh-provided vcolors.
 */

DataPointer(PolyBuff, stru_3D0FEB4, 0x3D0FEB4);
DataPointer(PolyBuff, stru_3D0FED8, 0x3D0FED8);
DataPointer(PolyBuff, stru_3D0FF20, 0x3D0FF20);

void __cdecl polybuff_vcolor_strip_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points)
{
	// Count vertices
	Int vertex_count = 0;
	Sint16* _meshes = meshset->meshes;
	Uint16 i = meshset->nbMesh;
	do
	{
		--i;
		Sint32 n = *_meshes & 0x3FFF;
		vertex_count += n + 2;
		_meshes += n + 1;
	} while (i);

	int mesh_index = 0;

	PolyBuff_SetCurrent(&stru_3D0FED8);
	auto buffer = (FVFStruct_F*)PolyBuff_LockTriangleStrip(&stru_3D0FED8, vertex_count, Direct3D_CurrentCullMode);

	if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
	{
		for(int i = 0; i < meshset->nbMesh; i++)
		{
			Sint16 n = (meshset->meshes[mesh_index++] & 0x3FFF); // retrieve vertex count

			Sint16 vert_index = meshset->meshes[mesh_index];
			buffer->position = points[vert_index];
			buffer->diffuse = PolyBuffVertexColor.color;
			++buffer;

			for(int j = 0; j < n; j++)
			{
				vert_index = meshset->meshes[mesh_index++];
				buffer->position = points[vert_index];
				buffer->diffuse = PolyBuffVertexColor.color;
				++buffer;
			}

			vert_index = meshset->meshes[mesh_index - 1];
			buffer->position = points[vert_index];
			buffer->diffuse = PolyBuffVertexColor.color;
			++buffer;
		}
	}
	else if (meshset->nbMesh && meshset->vertcolor)
	{
		int tex_index = 0;
		for(int i = 0; i < meshset->nbMesh; i++)
		{
			Sint16 n = (meshset->meshes[mesh_index++] & 0x3FFF); // retrieve vertex count

			Sint16 vert_index = meshset->meshes[mesh_index];
			NJS_COLOR* vertcolor = &meshset->vertcolor[tex_index];
			buffer->position = points[vert_index];
			buffer->diffuse = vertcolor->color;
			++buffer;

			for(int j = 0; j < n; j++)
			{
				vert_index = meshset->meshes[mesh_index++];
				vertcolor = &meshset->vertcolor[tex_index++];
				buffer->position = points[vert_index];
				buffer->diffuse = vertcolor->color;
				++buffer;
			}

			vert_index = meshset->meshes[mesh_index - 1];
			vertcolor = &meshset->vertcolor[tex_index - 1];
			buffer->position = points[vert_index];
			buffer->diffuse = vertcolor->color;
			++buffer;
		}
	}

	PolyBuff_Unlock(&stru_3D0FED8);
	PolyBuff_DrawTriangleStrip(&stru_3D0FED8);
}

void __cdecl polybuff_vcolor_tri_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points)
{
	Sint16* meshes = meshset->meshes;
	Uint32 vertex_count = 3 * meshset->nbMesh;

	PolyBuff_SetCurrent(&stru_3D0FEB4);
	auto buffer = (FVFStruct_F*)PolyBuff_LockTriangleList(&stru_3D0FEB4, vertex_count, Direct3D_CurrentCullMode);

	if (buffer)
	{
		if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
		{
			for(int i = 0; i < vertex_count; ++i)
			{
				Sint16 pt = *meshes;
				buffer->position = points[pt];
				buffer->diffuse = PolyBuffVertexColor.color;
				++buffer;
				++meshes;
			}
		}
		else if (meshset->vertcolor)
		{
			NJS_COLOR* vertcolor = meshset->vertcolor;
			for(int i = 0; i < vertex_count; ++i)
			{
				Sint16 pt = *meshes;
				buffer->position = points[pt];
				buffer->diffuse = vertcolor->color;
				++buffer;
				++meshes;
				++vertcolor;
			}
		}
	}

	PolyBuff_Unlock(&stru_3D0FEB4);
	PolyBuff_DrawTriangleList(&stru_3D0FEB4);
}

void __cdecl polybuff_normal_vcolor_strip_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	// Count vertices
	Int vertex_count = 0;
	Sint16* _meshes = meshset->meshes;
	Uint16 i = meshset->nbMesh;
	do
	{
		--i;
		Sint32 n = *_meshes & 0x3FFF;
		vertex_count += n + 2;
		_meshes += n + 1;
	} while (i);

	int mesh_index = 0;

	PolyBuff_SetCurrent(&stru_3D0FED8);
	auto buffer = (FVFStruct_G*)PolyBuff_LockTriangleStrip(&stru_3D0FED8, vertex_count, Direct3D_CurrentCullMode);

	if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
	{
		for(int i = 0; i < meshset->nbMesh; i++)
		{
			Sint16 n = (meshset->meshes[mesh_index++] & 0x3FFF); // retrieve vertex count

			Sint16 vert_index = meshset->meshes[mesh_index];
			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->diffuse = PolyBuffVertexColor.color;
			++buffer;

			for(int j = 0; j < n; j++)
			{
				vert_index = meshset->meshes[mesh_index++];
				buffer->position = points[vert_index];
				buffer->normal = normals[vert_index];
				buffer->diffuse = PolyBuffVertexColor.color;
				++buffer;
			}

			vert_index = meshset->meshes[mesh_index - 1];
			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->diffuse = PolyBuffVertexColor.color;
			++buffer;
		}
	}
	else if (meshset->nbMesh && meshset->vertcolor)
	{
		int tex_index = 0;
		for(int i = 0; i < meshset->nbMesh; i++)
		{
			Sint16 n = (meshset->meshes[mesh_index++] & 0x3FFF); // retrieve vertex count
			
			Sint16 vert_index = meshset->meshes[mesh_index];
			NJS_COLOR* vertcolor = &meshset->vertcolor[tex_index];
			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->diffuse = vertcolor->color;
			++buffer;

			for(int j = 0; j < n; j++)
			{
				vert_index = meshset->meshes[mesh_index++];
				vertcolor = &meshset->vertcolor[tex_index++];
				buffer->position = points[vert_index];
				buffer->normal = normals[vert_index];
				buffer->diffuse = vertcolor->color;
				++buffer;
			}

			vert_index = meshset->meshes[mesh_index - 1];
			vertcolor = &meshset->vertcolor[tex_index - 1];
			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->diffuse = vertcolor->color;
			++buffer;
		}
	}

	PolyBuff_Unlock(&stru_3D0FED8);
	PolyBuff_DrawTriangleStrip(&stru_3D0FED8);
}

void __cdecl polybuff_normal_vcolor_tri_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	Sint16* meshes = meshset->meshes;
	Uint32 vertex_count = 3 * meshset->nbMesh;

	PolyBuff_SetCurrent(&stru_3D0FED8);
	auto buffer = (FVFStruct_G*)PolyBuff_LockTriangleList(&stru_3D0FED8, vertex_count, Direct3D_CurrentCullMode);

	if (buffer)
	{
		if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
		{
			for(int i = 0; i < vertex_count; ++i)
			{
				Sint16 pt = *meshes;
				buffer->position = points[pt];
				buffer->normal = normals[pt];
				buffer->diffuse = PolyBuffVertexColor.color;
				++buffer;
				++meshes;
			}
		}
		else if (meshset->vertcolor)
		{
			NJS_COLOR* vertcolor = meshset->vertcolor;
			for(int i = 0; i < vertex_count; ++i)
			{
				Sint16 pt = *meshes;
				buffer->position = points[pt];
				buffer->normal = normals[pt];
				buffer->diffuse = vertcolor->color;
				++buffer;
				++meshes;
				++vertcolor;
			}
		}
	}

	PolyBuff_Unlock(&stru_3D0FED8);
	PolyBuff_DrawTriangleList(&stru_3D0FED8);
}

void __cdecl polybuff_normal_vcolor_uv_strip_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	// Count vertices
	Int vertex_count = 0;
	Sint16* _meshes = meshset->meshes;
	Uint16 i = meshset->nbMesh;
	do
	{
		--i;
		Int n = *_meshes & 0x3FFF;
		vertex_count += n + 2;
		_meshes += n + 1;
	} while (i);

	int mesh_index = 0;
	int tex_index = 0;

	PolyBuff_SetCurrent(&stru_3D0FF20);
	auto buffer = (FVFStruct_I*)PolyBuff_LockTriangleStrip(&stru_3D0FF20, vertex_count, Direct3D_CurrentCullMode);

	if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
	{
		for(int i = 0; i < meshset->nbMesh; i++)
		{
			Sint16 n = (meshset->meshes[mesh_index++] & 0x3FFF); // retrieve vertex count

			Sint16 vert_index = meshset->meshes[mesh_index];
			NJS_TEX* vertuv = &meshset->vertuv[tex_index];

			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->u = (Float)vertuv->u / 255.0f;
			buffer->v = (Float)vertuv->v / 255.0f;
			buffer->diffuse = PolyBuffVertexColor.color;
			++buffer;

			for(int j = 0; j < n; j++)
			{
				vert_index = meshset->meshes[mesh_index++];
				vertuv = &meshset->vertuv[tex_index++];

				buffer->position = points[vert_index];
				buffer->normal = normals[vert_index];
				buffer->u = (Float)vertuv->u / 255.0f;
				buffer->v = (Float)vertuv->v / 255.0f;
				buffer->diffuse = PolyBuffVertexColor.color;
				++buffer;
			}

			vert_index = meshset->meshes[mesh_index - 1];
			vertuv = &meshset->vertuv[tex_index - 1];

			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->u = (Float)vertuv->u / 255.0f;
			buffer->v = (Float)vertuv->v / 255.0f;
			buffer->diffuse = PolyBuffVertexColor.color;
			++buffer;
		}
	}
	else if (meshset->nbMesh && meshset->vertcolor)
	{
		for(int i = 0; i < meshset->nbMesh; i++)
		{
			Sint16 n = (meshset->meshes[mesh_index++] & 0x3FFF); // retrieve vertex count

			Sint16 vert_index = meshset->meshes[mesh_index];
			NJS_TEX* vertuv = &meshset->vertuv[tex_index];
			NJS_COLOR* vertcolor = &meshset->vertcolor[tex_index];

			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->u = (Float)vertuv->u / 255.0f;
			buffer->v = (Float)vertuv->v / 255.0f;
			buffer->diffuse = vertcolor->color;
			++buffer;

			for(int j = 0; j < n; j++)
			{
				vert_index = meshset->meshes[mesh_index++];
				vertuv = &meshset->vertuv[tex_index];
				vertcolor = &meshset->vertcolor[tex_index++];

				buffer->position = points[vert_index];
				buffer->normal = normals[vert_index];
				buffer->u = (Float)vertuv->u / 255.0f;
				buffer->v = (Float)vertuv->v / 255.0f;
				buffer->diffuse = vertcolor->color;
				++buffer;
			}

			vert_index = meshset->meshes[mesh_index - 1];
			vertuv = &meshset->vertuv[tex_index - 1];
			vertcolor = &meshset->vertcolor[tex_index - 1];

			buffer->position = points[vert_index];
			buffer->normal = normals[vert_index];
			buffer->u = (Float)vertuv->u / 255.0f;
			buffer->v = (Float)vertuv->v / 255.0f;
			buffer->diffuse = vertcolor->color;
			++buffer;
		}
	}

	PolyBuff_Unlock(&stru_3D0FF20);
	PolyBuff_DrawTriangleStrip(&stru_3D0FF20);
}

void __cdecl polybuff_normal_vcolor_uv_tri_r(NJS_MESHSET* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	Sint16* meshes = meshset->meshes;
	NJS_TEX* vertuv = meshset->vertuv;
	Uint32 vertex_count = 3 * meshset->nbMesh;

	PolyBuff_SetCurrent(&stru_3D0FF20);
	auto buffer = (FVFStruct_I*)PolyBuff_LockTriangleList(&stru_3D0FF20, vertex_count, Direct3D_CurrentCullMode);

	if (buffer)
	{
		if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
		{
			for(int i = 0; i < vertex_count; ++i)
			{
				Sint16 pt = *meshes;
				buffer->position = points[pt];
				buffer->normal = normals[pt];
				buffer->u = ((Float)vertuv->u) / 255.0f;
				buffer->v = ((Float)vertuv->v) / 255.0f;
				buffer->diffuse = PolyBuffVertexColor.color;
				++buffer;
				++meshes;
				++vertuv;
			}
		}
		else if (meshset->vertcolor)
		{
			NJS_COLOR* vertcolor = meshset->vertcolor;
			for(int i = 0; i < vertex_count; ++i)
			{
				Sint16 pt = *meshes;
				buffer->position = points[pt];
				buffer->normal = normals[pt];
				buffer->u = ((Float)vertuv->u) / 255.0f;
				buffer->v = ((Float)vertuv->v) / 255.0f;
				buffer->diffuse = vertcolor->color;
				++buffer;
				++meshes;
				++vertuv;
				++vertcolor;
			}
		}
	}

	PolyBuff_Unlock(&stru_3D0FF20);
	PolyBuff_DrawTriangleList(&stru_3D0FF20);
}

void polybuff::rewrite_init()
{
	WriteJump(polybuff_vcolor_strip, polybuff_vcolor_strip_r);
	WriteJump(polybuff_vcolor_tri, polybuff_vcolor_tri_r);
	WriteJump(polybuff_normal_vcolor_strip, polybuff_normal_vcolor_strip_r);
	WriteJump(polybuff_normal_vcolor_tri, polybuff_normal_vcolor_tri_r);
	WriteJump(polybuff_normal_vcolor_uv_strip, polybuff_normal_vcolor_uv_strip_r);
	WriteJump(polybuff_normal_vcolor_uv_tri, polybuff_normal_vcolor_uv_tri_r);
}