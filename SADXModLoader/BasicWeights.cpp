#include "stdafx.h"
#include "BasicWeights.h"
#include "IniFile.hpp"

using std::vector;

void InitWeights(WeightInfo* weights, NJS_OBJECT* object)
{
	for (int ni = 0; ni < weights->nodeCount; ++ni)
	{
		auto& node = weights->nodes[ni];
		if (node.vertices_orig)
			delete[] node.vertices_orig;
		if (node.normals_orig)
			delete[] node.normals_orig;
		node.nodeIndex = GetNodeIndex(object, node.node);
		for (int wi = 0; wi < node.weightCount; ++wi)
		{
			auto& weight = node.weights[wi];
			for (int vi = 0; vi < weight.vertexCount; ++vi)
			{
				auto& vert = weight.vertices[vi];
				vert.nodeIndex = GetNodeIndex(object, vert.node);
			}
		}
		auto model = node.node->basicdxmodel;
		node.vertices_orig = new NJS_VECTOR[model->nbPoint];
		memcpy(node.vertices_orig, model->points, model->nbPoint * sizeof(NJS_VECTOR));
		node.normals_orig = new NJS_VECTOR[model->nbPoint];
		memcpy(node.normals_orig, model->normals, model->nbPoint * sizeof(NJS_VECTOR));
	}
}

void ApplyWeights(WeightInfo* weights, NJS_ACTION* action, float frame)
{
	CalcMMMatrix(
		nj_unit_matrix_,
		action,
		frame,
		action->object->countnodes(),
		0);
	NJS_MATRIX basemat, matrix;
	NJS_VECTOR tmpvert, tmpnorm, vd;
	for (int ni = 0; ni < weights->nodeCount; ++ni)
	{
		auto& node = weights->nodes[ni];
		if (node.nodeIndex == -1)
		{
			node.nodeIndex = GetNodeIndex(action->object, node.node);
			if (node.nodeIndex == -1)
				continue;
		}
		NJS_MODEL_SADX* model = node.node->basicdxmodel;
		memcpy(model->points, node.vertices_orig, model->nbPoint * sizeof(NJS_VECTOR));
		memcpy(model->normals, node.normals_orig, model->nbPoint * sizeof(NJS_VECTOR));
		SetInstancedMatrix(node.nodeIndex, basemat);
		njInvertMatrix(basemat);
		for (int wi = 0; wi < node.weightCount; ++wi)
		{
			auto& weight = node.weights[wi];
			tmpvert = {};
			tmpnorm = {};
			for (int vi = 0; vi < weight.vertexCount; ++vi)
			{
				auto& vert = weight.vertices[vi];
				if (vert.nodeIndex == -1)
				{
					vert.nodeIndex = GetNodeIndex(action->object, vert.node);
					if (vert.nodeIndex == -1)
						continue;
				}
				SetInstancedMatrix(vert.nodeIndex, matrix);
				NJS_MODEL_SADX* vm = vert.node->basicdxmodel;
				njCalcPoint(matrix, &vm->points[vert.vertex], &vd);
				tmpvert.x += vd.x * vert.weight;
				tmpvert.y += vd.y * vert.weight;
				tmpvert.z += vd.z * vert.weight;
				njCalcVector(matrix, &vm->normals[vert.vertex], &vd);
				tmpnorm.x += vd.x * vert.weight;
				tmpnorm.y += vd.y * vert.weight;
				tmpnorm.z += vd.z * vert.weight;
			}
			njCalcPoint(basemat, &tmpvert, &model->points[weight.index]);
			njCalcVector(basemat, &tmpnorm, &model->normals[weight.index]);
		}
	}
}

void DeInitWeights(WeightInfo* weights, NJS_OBJECT* object)
{
	for (int ni = 0; ni < weights->nodeCount; ++ni)
	{
		auto& node = weights->nodes[ni];
		auto model = node.node->basicdxmodel;
		memcpy(model->points, node.vertices_orig, model->nbPoint * sizeof(NJS_VECTOR));
		memcpy(model->normals, node.normals_orig, model->nbPoint * sizeof(NJS_VECTOR));
		delete[] node.vertices_orig;
		delete[] node.normals_orig;
		node.vertices_orig = nullptr;
		node.normals_orig = nullptr;
	}
}