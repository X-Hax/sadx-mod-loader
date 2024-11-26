#include "stdafx.h"

#include <FunctionHook.h>

#include "polybuff.h"

float uv_multiply = 0.00390625f; // 0.0039215689f in vanilla
float uv_divide = 256.0f; // 255.0f in vanilla

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
	NJS_COLOR v6 = { 0 };

	Sint16* meshes = meshset->meshes;
	int v5 = 0;
	v6.color = PolyBuffVertexColor.color;
	Sint16* v7 = meshset->meshes;

	if (meshset->nbMesh)
	{
		int v8 = meshset->nbMesh;
		do
		{
			--v8;
			int v9 = *v7 & 0x3FFF;
			v5 += v9 + 2;
			v7 += v9 + 1;
		} while (v8);
	}

	PolyBuff_SetCurrent(&stru_3D0FEB4);
	auto buffer = (FVFStruct_F*)PolyBuff_LockTriangleStrip(&stru_3D0FEB4, v5, Direct3D_CurrentCullMode);

	if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
	{
		if (meshset->nbMesh)
		{
			NJS_POINT3* v21 = points;
			int v32 = meshset->nbMesh;
			do
			{
				int v22 = meshes[1];
				Sint16 v23 = *meshes;
				++meshes;
				float* v24 = &v21[v22].x;
				NJS_VECTOR* v25 = &buffer->position;
				buffer->position.x = *v24;
				buffer->position.y = v24[1];
				float v26 = v24[2];
				unsigned __int16 v27 = v23 & 0x3FFF;
				buffer->diffuse = v6.color;
				FVFStruct_F* v28 = buffer + 1;
				v25->z = v26;
				v21 = points;
				if (v27)
				{
					int a3b = v27;
					do
					{
						NJS_POINT3* v29 = &points[*meshes];
						v28->position.x = v29->x;
						v28->position.y = v29->y;
						v28->position.z = v29->z;
						v28->diffuse = v6.color;
						++v28;
						++meshes;
						--a3b;
					} while (a3b);
				}
				NJS_POINT3* v30 = &points[*(meshes - 1)];
				v28->position.x = v30->x;
				v28->position.y = v30->y;
				v28->position.z = v30->z;
				v28->diffuse = v6.color;
				buffer = v28 + 1;
				--v32;
			} while (v32);
		}
	}
	else if (meshset->nbMesh && meshset->vertcolor)
	{
		size_t index = 0;
		size_t c = 0;

		for (int i = 0; i < meshset->nbMesh; i++)
		{
			const auto n = static_cast<Sint16>(meshes[index++] & 0x3FFF);
			auto head = buffer++;

			for (int j = 0; j < n; j++)
			{
				const short vert_index = meshes[index++];

				buffer->position = points[vert_index];

				const auto vertcolor = meshset->vertcolor[c].color;
				++c;

				buffer->diffuse = vertcolor;
				++buffer;
			}

			*head = *(head + 1);

			*buffer = *(buffer - 1);
			++buffer;
		}
	}

	PolyBuff_Unlock(&stru_3D0FEB4);
	PolyBuff_DrawTriangleStrip(&stru_3D0FEB4);
}

void __cdecl polybuff_vcolor_tri_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points)
{
	NJS_COLOR v5 = { 0 };
	FVFStruct_F* v9; 
	float v10;
	FVFStruct_F* v12; 
	float v13; 

	Sint16* v4 = meshset->meshes;
	v5.color = PolyBuffVertexColor.color;
	unsigned int v6 = 3 * meshset->nbMesh;

	PolyBuff_SetCurrent(&stru_3D0FEB4);
	auto buffer = (FVFStruct_F*)PolyBuff_LockTriangleList(&stru_3D0FEB4, v6, Direct3D_CurrentCullMode);

	if (buffer)
	{
		if (_RenderFlags & RenderFlags_OffsetMaterial || !meshset->vertcolor)
		{
			for (; v6; v12->position.z = v13)
			{
				NJS_POINT3* v11 = &points[*v4];
				v12 = buffer;
				buffer->position.x = v11->x;
				buffer->position.y = v11->y;
				v13 = v11->z;
				buffer->diffuse = v5.color;
				++buffer;
				++v4;
				--v6;
			}
		}
		else if (meshset->vertcolor)
		{
			size_t i = 0;

			for (; v6; v9->position.z = v10)
			{
				NJS_POINT3* v8 = &points[*v4];
				v9 = buffer;
				buffer->position.x = v8->x;
				buffer->position.y = v8->y;
				v10 = v8->z;
				buffer->diffuse = meshset->vertcolor[i++].color;
				++buffer;
				++v4;
				--v6;
			}
		}
	}
	PolyBuff_Unlock(&stru_3D0FEB4);
	PolyBuff_DrawTriangleList(&stru_3D0FEB4);
}

void __cdecl polybuff_normal_vcolor_strip_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	NJS_COLOR v6 = { 0 };

	Sint16* meshes = meshset->meshes;
	int v5 = 0;
	v6.color = PolyBuffVertexColor.color;
	char v33 = _RenderFlags;
	Sint16* v7 = meshset->meshes;

	if (meshset->nbMesh)
	{
		int v8 = meshset->nbMesh;
		do
		{
			--v8;
			int v9 = *v7 & 0x3FFF;
			v5 += v9 + 2;
			v7 += v9 + 1;
		} while (v8);
	}

	PolyBuff_SetCurrent(&stru_3D0FED8);
	auto buffer = (FVFStruct_G*)PolyBuff_LockTriangleStrip(&stru_3D0FED8, v5, Direct3D_CurrentCullMode);

	if (v33 & RenderFlags_OffsetMaterial || !meshset->vertcolor)
	{
		if (meshset->nbMesh)
		{
			int v35 = meshset->nbMesh;
			do
			{
				Sint16 v22 = *meshes;
				++meshes;
				int a3c = v22 & 0x3FFF;
				int v23 = *meshes;
				buffer->position.x = points[v23].x;
				buffer->position.y = points[v23].y;
				buffer->position.z = points[v23].z;
				NJS_VECTOR* v24 = &normals[v23];
				NJS_VECTOR* v25 = &buffer->normal;
				v25->x = v24->x;
				v25->y = v24->y;
				v25->z = v24->z;
				buffer->diffuse = v6.color;
				FVFStruct_G* v26 = buffer + 1;
				if ((WORD)a3c)
				{
					int a3d = (unsigned __int16)a3c;
					do
					{
						int v27 = *meshes;
						v26->position.x = points[v27].x;
						v26->position.y = points[v27].y;
						v26->position.z = points[v27].z;
						NJS_VECTOR* v28 = &normals[v27];
						NJS_VECTOR* v29 = &v26->normal;
						v29->x = v28->x;
						v29->y = v28->y;
						v29->z = v28->z;
						v26->diffuse = v6.color;
						++v26;
						++meshes;
						--a3d;
					} while (a3d);
				}
				int v30 = *(meshes - 1);
				v26->position.x = points[v30].x;
				v26->position.y = points[v30].y;
				v26->position.z = points[v30].z;
				NJS_VECTOR* v31 = &normals[v30];
				NJS_VECTOR* v32 = &v26->normal;
				v32->x = v31->x;
				v32->y = v31->y;
				v32->z = v31->z;
				v26->diffuse = v6.color;
				buffer = v26 + 1;
				--v35;
			} while (v35);
		}
	}
	else if (meshset->nbMesh && meshset->vertcolor)
	{
		size_t index = 0;
		size_t c = 0;

		for (int i = 0; i < meshset->nbMesh; i++)
		{
			const auto n = static_cast<Sint16>(meshes[index++] & 0x3FFF);
			auto head = buffer++;

			for (int j = 0; j < n; j++)
			{
				const short vert_index = meshes[index++];

				buffer->position = points[vert_index];
				buffer->normal = normals[vert_index];

				const auto vertcolor = meshset->vertcolor[c].color;
				++c;

				buffer->diffuse = vertcolor;
				++buffer;
			}

			*head = *(head + 1);

			*buffer = *(buffer - 1);
			++buffer;
		}
	}

	PolyBuff_Unlock(&stru_3D0FED8);
	PolyBuff_DrawTriangleStrip(&stru_3D0FED8);
}

void __cdecl polybuff_normal_vcolor_tri_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	NJS_COLOR v6 = { 0 };

	Sint16* meshes = meshset->meshes;
	int nbMesh = meshset->nbMesh;
	RenderFlags meshseta = _RenderFlags;
	v6.color = PolyBuffVertexColor.color;
	int vertex_count = 3 * nbMesh;

	PolyBuff_SetCurrent(&stru_3D0FED8);
	auto buffer = (FVFStruct_G*)PolyBuff_LockTriangleList(&stru_3D0FED8, vertex_count, Direct3D_CurrentCullMode);

	if (buffer)
	{
		if (meshseta & RenderFlags_OffsetMaterial || !meshset->vertcolor)
		{
			if (vertex_count)
			{
				int meshsetc = vertex_count;
				do
				{
					int v12 = *meshes;
					buffer->position.x = points[v12].x;
					buffer->position.y = points[v12].y;
					buffer->position.z = points[v12].z;
					NJS_VECTOR* v13 = &normals[v12];
					NJS_VECTOR* v14 = &buffer->normal;
					v14->x = v13->x;
					v14->y = v13->y;
					v14->z = v13->z;
					buffer->diffuse = v6.color;
					++buffer;
					++meshes;
					meshsetc = meshsetc - 1;
				} while (meshsetc);
			}
		}
		else if (vertex_count && meshset->vertcolor)
		{
			int meshsetb = vertex_count;
			auto vertcolor = meshset->vertcolor;
			do
			{
				int v9 = *meshes;
				buffer->position.x = points[v9].x;
				buffer->position.y = points[v9].y;
				buffer->position.z = points[v9].z;
				NJS_VECTOR* v10 = &normals[v9];
				NJS_VECTOR* v11 = &buffer->normal;
				v11->x = v10->x;
				v11->y = v10->y;
				v11->z = v10->z;
				buffer->diffuse = vertcolor->color;
				++buffer;
				++vertcolor;
				++meshes;
				meshsetb = meshsetb - 1;
			} while (meshsetb);
		}
	}

	PolyBuff_Unlock(&stru_3D0FED8);
	PolyBuff_DrawTriangleList(&stru_3D0FED8);
}

void __cdecl polybuff_normal_vcolor_uv_strip_r(NJS_MESHSET_SADX* meshset, NJS_POINT3* points, NJS_VECTOR* normals)
{
	NJS_COLOR v3 = { 0 };
	int primitiveCount = 0;
	_BOOL1 done = false;
	Sint16 _n = 0;
	int v26;
	NJS_POINT3* v27 = nullptr;
	NJS_VECTOR* normal = { 0 };
	FVFStruct_I* v29 = nullptr;
	int v30;
	NJS_VECTOR* v31 = nullptr;
	NJS_VECTOR* v32 = nullptr;
	signed int v33 = 0;
	int v34;
	NJS_POINT3* v35 = nullptr;
	NJS_VECTOR* v36 = nullptr;
	NJS_TEX* v37 = nullptr;
	NJS_COLOR v41 = { 0 };
	unsigned __int16 n_masked;
	int a1f = 0;
	int a1b = 0;
	int a1g = 0;
	int a1h = 0;

	v3.color = PolyBuffVertexColor.color;
	NJS_TEX* uv = meshset->vertuv;
	Sint16* meshes = meshset->meshes;
	v41.color = PolyBuffVertexColor.color;
	char v42 = _RenderFlags;
	Sint16* _meshes = meshset->meshes;

	if (meshset->nbMesh)
	{
		int i = meshset->nbMesh;
		do
		{
			--i;
			int n = *_meshes & 0x3FFF;
			primitiveCount += n + 2;
			_meshes += n + 1;
		} while (i);
	}

	PolyBuff_SetCurrent(&stru_3D0FF20);
	auto buffer = (FVFStruct_I*)PolyBuff_LockTriangleStrip(&stru_3D0FF20, primitiveCount, Direct3D_CurrentCullMode);

	if (v42 & RenderFlags_OffsetMaterial || !meshset->vertcolor)
	{
		if (meshset->nbMesh)
		{
			int j = meshset->nbMesh;
			do
			{
				_n = *meshes;
				++meshes;
				n_masked = _n & 0x3FFF;
				v26 = *meshes;
				a1f = v26 * 12;
				v27 = &points[v26];
				buffer->position.x = v27->x;
				buffer->position.y = v27->y;
				buffer->position.z = v27->z;
				normal = &buffer->normal;
				normal->x = *(float*)((char*)&normals->x + a1f);
				normal->y = *(float*)((char*)&normals->y + a1f);
				normal->z = *(float*)((char*)&normals->z + a1f);
				buffer->diffuse = v3.color;
				v29 = buffer + 1;
				v29[-1].u = (float)uv->u * uv_multiply;
				v29[-1].v = (float)uv->v * uv_multiply;
				if (n_masked)
				{
					a1b = n_masked;
					do
					{
						v30 = *meshes;
						v29->position.x = points[v30].x;
						v29->position.y = points[v30].y;
						v3.color = v41.color;
						v29->position.z = points[v30].z;
						v31 = &normals[v30];
						v32 = &v29->normal;
						v32->x = v31->x;
						v32->y = v31->y;
						v32->z = v31->z;
						v29->diffuse = v41.color;
						v33 = uv->u;
						++v29;
						++meshes;
						++uv;
						v29[-1].u = (float)v33 * uv_multiply;
						done = a1b-- == 1;
						v29[-1].v = (float)uv[-1].v * uv_multiply;
					} while (!done);
				}
				v34 = *(meshes - 1);
				a1g = v34 * 12;
				v35 = &points[v34];
				v29->position.x = v35->x;
				v29->position.y = v35->y;
				v29->position.z = v35->z;
				v36 = &v29->normal;
				v36->x = *(float*)((char*)&normals->x + a1g);
				v36->y = *(float*)((char*)&normals->y + a1g);
				v37 = uv - 1;
				v36->z = *(float*)((char*)&normals->z + a1g);
				v29->diffuse = v3.color;
				a1h = v37->u;
				buffer = v29 + 1;
				uv = v37 + 1;
				done = j-- == 1;
				buffer[-1].u = (float)a1h * uv_multiply;
				buffer[-1].v = (float)uv[-1].v * uv_multiply;
			} while (!done);
		}
	}
	else if (meshset->nbMesh && meshset->vertcolor)
	{
		size_t index = 0;
		size_t c = 0;

		for (int i = 0; i < meshset->nbMesh; i++)
		{
			const auto n = static_cast<Sint16>(meshes[index++] & 0x3FFF);
			auto head = buffer++;

			for (int j = 0; j < n; j++)
			{
				const short vert_index = meshes[index++];

				buffer->position = points[vert_index];
				buffer->normal = normals[vert_index];

				const auto vertuv = meshset->vertuv[c];
				const auto vertcolor = meshset->vertcolor[c].color;
				++c;

				buffer->diffuse = vertcolor;
				buffer->u = static_cast<float>(vertuv.u) / uv_divide;
				buffer->v = static_cast<float>(vertuv.v) / uv_divide;
				++buffer;
			}

			*head = *(head + 1);

			*buffer = *(buffer - 1);
			++buffer;
		}
	}

	PolyBuff_Unlock(&stru_3D0FF20);
	PolyBuff_DrawTriangleStrip(&stru_3D0FF20);
}

void __cdecl polybuff_normal_vcolor_uv_tri_r(NJS_MESHSET* a1, NJS_POINT3* points, NJS_VECTOR* normals)
{
	NJS_COLOR v18 = { 0 };

	int v3 = Direct3D_CurrentCullMode;
	v18.color = PolyBuffVertexColor.color;
	NJS_TEX* uv = a1->vertuv;
	Sint16* meshes = a1->meshes;
	const int nbMesh = a1->nbMesh;
	int a1a = _RenderFlags;
	int v7 = 3 * nbMesh;

	PolyBuff_SetCurrent(&stru_3D0FF20);
	auto buffer = (FVFStruct_I*)PolyBuff_LockTriangleList(&stru_3D0FF20, v7, v3);

	if (buffer)
	{
		if (a1a & RenderFlags_OffsetMaterial && a1->vertcolor)
		{
			if (v7)
			{
				int a1c = v7;
				do
				{
					int v14 = *meshes;
					buffer->position.x = points[v14].x;
					buffer->position.y = points[v14].y;
					buffer->position.z = points[v14].z;
					NJS_POINT3* v15 = &normals[v14];
					NJS_VECTOR* v16 = &buffer->normal;
					v16->x = v15->x;
					v16->y = v15->y;
					v16->z = v15->z;
					buffer->diffuse = v18.color;
					signed int v17 = uv->u;
					++buffer;
					++meshes;
					++uv;
					buffer[-1].u = (float)v17 * uv_multiply;
					buffer[-1].v = (float)uv[-1].v * uv_multiply;
				} while (a1c-- != 1);
			}
		}
		else if (v7)
		{
			auto vertcolor = a1->vertcolor;
			int a1b = v7;
			do
			{
				int v9 = *meshes;
				buffer->position.x = points[v9].x;
				buffer->position.y = points[v9].y;
				buffer->position.z = points[v9].z;
				NJS_POINT3* v10 = &normals[v9];
				NJS_VECTOR* v11 = &buffer->normal;
				v11->x = v10->x;
				v11->y = v10->y;
				v11->z = v10->z;
				buffer->diffuse = vertcolor->color;
				const int v12 = uv->u;
				++buffer;
				++meshes;
				++vertcolor;
				++uv;
				buffer[-1].u = (float)v12 * uv_multiply;
				buffer[-1].v = (float)uv[-1].v * uv_multiply;
			} while (a1b-- != 1);
		}
	}

	PolyBuff_Unlock(&stru_3D0FF20);
	PolyBuff_DrawTriangleList(&stru_3D0FF20);
}

static IDirect3DVertexBuffer8* particle_quad = nullptr;

struct ParticleData
{
	NJS_COLOR diffuse = { 0xFFFFFFFF };
	float u1 = 0.0f;
	float v1 = 0.0f;
	float u2 = 1.0f;
	float v2 = 1.0f;

	bool operator==(const ParticleData& other) const
	{
		return diffuse.color == other.diffuse.color &&
			u1 == other.u1 &&
			v1 == other.v1 &&
			u2 == other.u2 &&
			v2 == other.v2;
	}

	bool operator!=(const ParticleData& other) const
	{
		return !(*this == other);
	}
};

ParticleData last_particle;

#pragma pack(push, 1)
struct ParticleVertex
{
	static const UINT format = D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1;
	D3DXVECTOR3 position{};
	uint32_t diffuse = 0;
	D3DXVECTOR2 tex_coord{};
};
#pragma pack(pop)

static void draw_particle(NJS_SPRITE* sp, int n, uint32_t attr)
{
	ParticleData particle;

	if (attr & NJD_SPRITE_COLOR)
	{
		particle.diffuse.argb.b = static_cast<uint8_t>(_nj_constant_material_.b * 255.0);
		particle.diffuse.argb.g = static_cast<uint8_t>(_nj_constant_material_.g * 255.0);
		particle.diffuse.argb.r = static_cast<uint8_t>(_nj_constant_material_.r * 255.0);
		particle.diffuse.argb.a = static_cast<uint8_t>(_nj_constant_material_.a * 255.0);
	}

	const auto& tanim = sp->tanim[n];

	particle.u1 = tanim.u1 / uv_divide;
	particle.v1 = tanim.v1 / uv_divide;

	particle.u2 = tanim.u2 / uv_divide;
	particle.v2 = tanim.v2 / uv_divide;

	if (attr & NJD_SPRITE_HFLIP)
	{
		std::swap(particle.u1, particle.u2);
	}

	if ((!(attr & NJD_SPRITE_VFLIP) && (attr & NJD_SPRITE_SCALE)) || attr & NJD_SPRITE_VFLIP)
	{
		std::swap(particle.v1, particle.v2);
	}

	// diffuse color should probably just be done with a material since this is fixed function stuff!
	if (particle != last_particle || particle_quad == nullptr)
	{
		last_particle = particle;

		if (particle_quad == nullptr)
		{
			Direct3D_Device->CreateVertexBuffer(4 * sizeof(ParticleVertex), 0, ParticleVertex::format, D3DPOOL_MANAGED, &particle_quad);
		}

		BYTE* ppbData;
		particle_quad->Lock(0, 4 * sizeof(ParticleVertex), &ppbData, D3DLOCK_DISCARD);

		auto quad = reinterpret_cast<ParticleVertex*>(ppbData);

		// top left
		quad[0].position = D3DXVECTOR3(-0.5f, -0.5f, 0.0f);
		quad[0].diffuse = particle.diffuse.color;
		quad[0].tex_coord = D3DXVECTOR2(particle.u1, particle.v1);

		// top right
		quad[1].position = D3DXVECTOR3(0.5f, -0.5f, 0.0f);
		quad[1].diffuse = particle.diffuse.color;
		quad[1].tex_coord = D3DXVECTOR2(particle.u2, particle.v1);

		// bottom left
		quad[2].position = D3DXVECTOR3(-0.5f, 0.5f, 0.0f);
		quad[2].diffuse = particle.diffuse.color;
		quad[2].tex_coord = D3DXVECTOR2(particle.u1, particle.v2);

		// bottom right
		quad[3].position = D3DXVECTOR3(0.5f, 0.5f, 0.0f);
		quad[3].diffuse = particle.diffuse.color;
		quad[3].tex_coord = D3DXVECTOR2(particle.u2, particle.v2);

		particle_quad->Unlock();
	}

	const auto old_world = WorldMatrix;

	if (attr & NJD_SPRITE_SCALE)
	{
		njAlphaMode((attr & NJD_SPRITE_ALPHA) ? 2 : 0);
		ProjectToWorldSpace();

		auto m = WorldMatrix;

		// translate to world position
		njTranslateV(&m._11, &sp->p);

		// identity-ify the rotation so it can be replaced with the camera's
		// rotation; we don't need it anymore in billboard mode
		*reinterpret_cast<NJS_VECTOR*>(&m._11) = { 1.0f, 0.0f, 0.0f };
		*reinterpret_cast<NJS_VECTOR*>(&m._21) = { 0.0f, 1.0f, 0.0f };
		*reinterpret_cast<NJS_VECTOR*>(&m._31) = { 0.0f, 0.0f, 1.0f };

		njPushMatrix(&m._11);
		{
			const float scale_x = tanim.sx * sp->sx;
			const float scale_y = tanim.sy * sp->sy;
			const float offset_x = scale_x * ((static_cast<float>(tanim.cx) / static_cast<float>(tanim.sx)) - 0.5f);
			const float offset_y = scale_y * ((static_cast<float>(tanim.cy) / static_cast<float>(tanim.sy)) - 0.5f);

			// match camera's rotation
			const auto& cam_rot = Camera_Data1->Rotation;
			njRotateEx(&cam_rot.x, 1);

			// rotate in screen space around the z axis
			if (attr & NJD_SPRITE_ANGLE && sp->ang)
			{
				njRotateZ(nullptr, -sp->ang);
			}

			// apply center-offset
			njTranslate(nullptr, offset_x, offset_y, 0.0f);

			// scale to size
			njScale(nullptr, scale_x, scale_y, 1.0f);

			njGetMatrix(&WorldMatrix._11);
			Direct3D_SetWorldTransform();
		}
		njPopMatrix(1);
	}
	else
	{
		njTextureShadingMode(attr & NJD_SPRITE_ALPHA ? 2 : 0);
		ProjectToWorldSpace();

		njPushMatrix(&WorldMatrix._11);
		{
			const float scale_x = tanim.sx * sp->sx;
			const float scale_y = tanim.sy * sp->sy;
			const float offset_x = scale_x * ((static_cast<float>(tanim.cx) / static_cast<float>(tanim.sx)) - 0.5f);
			const float offset_y = scale_y * ((static_cast<float>(tanim.cy) / static_cast<float>(tanim.sy)) - 0.5f);

			// translate to world position (if applicable)
			njTranslateEx(&sp->p);

			// rotate in screen space
			if (attr & NJD_SPRITE_ANGLE && sp->ang)
			{
				njRotateZ(nullptr, -sp->ang);
			}

			// apply center-offset
			njTranslate(nullptr, -offset_x, -offset_y, 0.0f);

			// scale to size
			njScale(nullptr, scale_x, scale_y, 1.0f);

			njGetMatrix(&WorldMatrix._11);
			Direct3D_SetWorldTransform();
		}
		njPopMatrixEx();
	}

	// save original vbuffer
	IDirect3DVertexBuffer8* stream;
	UINT stride;
	Direct3D_Device->GetStreamSource(0, &stream, &stride);

	// store original FVF
	DWORD FVF;
	Direct3D_Device->GetVertexShader(&FVF);

	// draw
	Direct3D_Device->SetVertexShader(ParticleVertex::format);
	Direct3D_Device->SetStreamSource(0, particle_quad, sizeof(ParticleVertex));
	Direct3D_Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

	// restore original vbuffer
	Direct3D_Device->SetStreamSource(0, stream, stride);

	// restore original FVF
	Direct3D_Device->SetVertexShader(FVF);

	if (stream)
	{
		stream->Release();
	}

	WorldMatrix = old_world;
	Direct3D_SetWorldTransform();
}

void __cdecl njDrawSprite3D_DrawNow_r(NJS_SPRITE* sp, int n, NJD_SPRITE attr);
static Trampoline njDrawSprite3D_DrawNow_t(0x0077E390, 0x0077E398, &njDrawSprite3D_DrawNow_r);

void __cdecl njDrawSprite3D_DrawNow_r(NJS_SPRITE* sp, int n, NJD_SPRITE attr)
{
	if (!sp)
	{
		return;
	}

	const auto tlist = sp->tlist;

	if (tlist)
	{
		const auto tanim = &sp->tanim[n];
		Direct3D_SetTexList(tlist);
		njSetTextureNum_(tanim->texid);

		Direct3D_Device->SetTextureStageState(0, D3DTSS_ADDRESSU, D3DTADDRESS_CLAMP);
		Direct3D_Device->SetTextureStageState(0, D3DTSS_ADDRESSV, D3DTADDRESS_CLAMP);
		Direct3D_DiffuseSourceVertexColor();
	}
	else
	{
		return;
	}

#ifdef _DEBUG
	if (ControllerPointers[0] && ControllerPointers[0]->HeldButtons & Buttons_Z)
	{
		auto original = reinterpret_cast<decltype(njDrawSprite3D_DrawNow_r)*>(njDrawSprite3D_DrawNow_t.Target());
		original(sp, n, attr);
		return;
	}
#endif

	draw_particle(sp, n, attr);
}

Uint32 uvPatchList[] = 
{
	// polybuff_uv_strip
	0x007823D4,
	0x007823ED,
	0x00782438,
	0x00782456,
	0x0078249E,
	0x007824BC,
	// polybuff_uv_tri
	0x00782578,
	0x00782596,
	// polybuff_uv_quad
	0x007826B4,
	0x007826C9,
	// polybuff_vcolor_uv_strip
	0x00782DC7,
	0x00782DDC,
	0x00782E38,
	0x00782E51,
	0x00782EAA,
	0x00782EC4,
	// polybuff_vcolor_uv_tri
	0x00783017,
	0x00783031,
	0x00782FA3,
	0x00782FBC,
	// polybuff_vcolor_uv_quad
	0x007831DE,
	0x007831FC,
	// polybuff_normal_uv_strip
	0x007836CF,
	0x007836E4,
	0x0078375C,
	0x00783775,
	0x007837E1,
	0x007837FA,
	// polybuff_normal_uv_tri
	0x007838DE,
	0x007838F7,
	// polybuff_normal_uv_quad
	0x00783A39,
	0x00783A57,
	// polybuff_normal_vcolor_uv_strip
	0x0078431E,
	0x00784333,
	0x007843A7,
	0x007843C5,
	0x00784435,
	0x0078444E,
	0x00784160,
	0x00784175,
	0x007841E7,
	0x00784205,
	0x00784275,
	0x0078428E,
	// polybuff_normal_vcolor_uv_tri
	0x007845DC,
	0x007845F5,
	0x0078454C,
	0x00784565,
	// polybuff_normal_vcolor_uv_quad
	0x007847BD,
	0x007847DF,
	// meshset_uv_strip
	0x00785B66,
	0x00785B82,
	0x00785BD4,
	0x00785BEE,
	0x00785C2D,
	0x00785C42,
	0x00785A28,
	0x00785A41,
	0x00785A89,
	0x00785AA7,
	0x00785AEF,
	0x00785B0D,
	// meshset_uv_tri
	0x00785D91,
	0x00785DAF,
	0x00785D22,
	0x00785D3B,
	// meshset_uv_quad
	0x0078601E,
	0x0078603C,
	0x00785EF5,
	0x00785F0A,
	// meshset_uv_normal_strip
	0x007869CA,
	0x007869E6,
	0x00786A61,
	0x00786A7F,
	0x00786AFB,
	0x00786B19,
	0x0078680E,
	0x00786823,
	0x0078689C,
	0x007868B5,
	0x00786921,
	0x0078693A,
	// meshset_uv_normal_tri
	0x00786CAC,
	0x00786CCA,
	0x00786C1C,
	0x00786C35,
	// meshset_uv_normal_quad
	0x00786F8D,
	0x00786FAF,
	0x00786E39,
	0x00786E57,
	// njDrawSprite2D
	0x0077E0A5,
	0x0077E0C4,
	0x0077E0DE,
	0x0077E0F4,
	// njDrawSprite3D
	0x0077E4FF,
	0x0077E511,
	0x0077E523,
	0x0077E531,
	// __SAnjDrawPolygon2D
	0x0040126E,
	0x0040127C,
	0x0040128A,
	0x00401294,
	// Point3ColToVertexBuffer_UV
	0x0077E90D,
	0x0077E922,
	// sub_78F1B0
	0x0078F28D,
	0x0078F2A5,
	0x0078F32C,
	0x0078F312,
	0x0078F3AF,
	0x0078F3CB,
	// sub_790290
	0x00790361,
	0x00790379,
	0x007903FF,
	0x007903E2,
	0x00790480,
	0x00790499,
	// sub_790950
	0x00790A24,
	0x00790A39,
	0x00790A79,
	0x00790A9A,
	0x00790AE2,
	0x00790B06
};

void polybuff::rewrite_init()
{
	WriteJump(polybuff_vcolor_strip, polybuff_vcolor_strip_r);
	WriteJump(polybuff_vcolor_tri, polybuff_vcolor_tri_r);
	WriteJump(polybuff_normal_vcolor_strip, polybuff_normal_vcolor_strip_r);
	WriteJump(polybuff_normal_vcolor_tri, polybuff_normal_vcolor_tri_r);
	WriteJump(polybuff_normal_vcolor_uv_strip, polybuff_normal_vcolor_uv_strip_r);
	WriteJump(polybuff_normal_vcolor_uv_tri, polybuff_normal_vcolor_uv_tri_r);
	for (int i = 0; i < LengthOfArray(uvPatchList); i++)
	{
#ifdef DEBUG
		Uint32& orig = *(Uint32*)uvPatchList[i];
		PrintDebug("UV fix at %X: %X\n", uvPatchList[i], orig);
#endif // DEBUG
		WriteData((float**)uvPatchList[i], &uv_multiply);
	}
}
