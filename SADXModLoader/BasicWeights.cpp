#include "stdafx.h"
#include "BasicWeights.h"
#include "IniFile.hpp"

using std::vector;

WeightInfo* LoadWeights(const char* filename)
{
	IniFile weldini(filename);
	if (weldini.cbegin() == weldini.cend())
		return nullptr;
	vector<WeightNode> nodes;
	for (auto& group : weldini)
		if (!group.first.empty())
		{
			int nbPoint = group.second->getInt("VertexCount", 10000);
			vector<WeightVertexList> weights;
			weights.reserve(nbPoint);
			for (int i = 0; i < nbPoint; i++)
			{
				vector<WeightVertex> verts;
				verts.reserve(10);
				char buf[100];
				for (int j = 0; j < 10; j++)
				{
					sprintf_s(buf, "Vertices[%d][%d]", i, j);
					if (group.second->hasKeyNonEmpty(buf))
					{
						auto str = group.second->getString(buf);
						int ind = str.find(',');
						int ind2 = str.find(',', ind + 1);
						verts.push_back({
							std::stoi(str.substr(0, ind)),
							std::stoi(str.substr(ind + 1, ind2 - ind + 1)),
							std::stof(str.substr(ind2 + 1))
							});
					}
					else
						break;
				}
				if (verts.size() > 0)
				{
					WeightVertex* tmp = new WeightVertex[verts.size()];
					memcpy(tmp, verts.data(), verts.size() * sizeof(WeightVertex));
					weights.push_back({
						i,
						tmp,
						static_cast<int>(verts.size())
						});
				}
			}
			WeightVertexList* weightbuf = new WeightVertexList[weights.size()];
			memcpy(weightbuf, weights.data(), weights.size() * sizeof(WeightVertexList));
			nodes.push_back({
				std::stoi(group.first),
				weightbuf,
				static_cast<int>(weights.size())
				});
		}
	WeightNode* nodebuf = new WeightNode[nodes.size()];
	memcpy(nodebuf, nodes.data(), nodes.size() * sizeof(WeightNode));
	return new WeightInfo{
		nodebuf,
		static_cast<int>(nodes.size())
	};
}

void FreeWeights(WeightInfo* weights)
{
	for (int ni = 0; ni < weights->nodeCount; ++ni)
	{
		auto& node = weights->nodes[ni];
		for (int wi = 0; wi < node.weightCount; ++wi)
			delete[] node.weights[wi].vertices;
		delete[] node.weights;
		if (node.vertices_orig)
			delete[] node.vertices_orig;
		if (node.normals_orig)
			delete[] node.normals_orig;
	}
	delete[] weights->nodes;
	delete weights;
}

void InitWeights(WeightInfo* weights, NJS_OBJECT* object)
{
	for (int ni = 0; ni < weights->nodeCount; ++ni)
	{
		auto& node = weights->nodes[ni];
		if (node.vertices_orig)
			delete[] node.vertices_orig;
		if (node.normals_orig)
			delete[] node.normals_orig;
		auto model = object->getnode(node.index)->basicdxmodel;
		node.vertices_orig = new NJS_VECTOR[model->nbPoint];
		memcpy(node.vertices_orig, model->points, model->nbPoint * sizeof(NJS_VECTOR));
		node.normals_orig = new NJS_VECTOR[model->nbPoint];
		memcpy(node.normals_orig, model->normals, model->nbPoint * sizeof(NJS_VECTOR));
	}
}

void ApplyWeights(WeightInfo* weights, NJS_ACTION* action, float frame)
{
	ProcessAnimatedModelNode_Instanced(
		nj_unit_matrix_,
		action,
		frame,
		0,
		0);
	NJS_MATRIX basemat, matrix;
	NJS_VECTOR tmpvert, tmpnorm, vd;
	for (int ni = 0; ni < weights->nodeCount; ++ni)
	{
		auto& node = weights->nodes[ni];
		NJS_MODEL_SADX* model = action->object->getnode(node.index)->basicdxmodel;
		memcpy(model->points, node.vertices_orig, model->nbPoint * sizeof(NJS_VECTOR));
		memcpy(model->normals, node.normals_orig, model->nbPoint * sizeof(NJS_VECTOR));
		SetInstancedMatrix(node.index, basemat);
		njInvertMatrix(basemat);
		for (int wi = 0; wi < node.weightCount; ++wi)
		{
			auto& weight = node.weights[wi];
			tmpvert = {};
			tmpnorm = {};
			for (int vi = 0; vi < weight.vertexCount; ++vi)
			{
				auto& vert = weight.vertices[vi];
				SetInstancedMatrix(vert.node, matrix);
				NJS_MODEL_SADX* vm = action->object->getnode(vert.node)->basicdxmodel;
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
		auto model = object->getnode(node.index)->basicdxmodel;
		memcpy(model->points, node.vertices_orig, model->nbPoint * sizeof(NJS_VECTOR));
		memcpy(model->normals, node.normals_orig, model->nbPoint * sizeof(NJS_VECTOR));
		delete[] node.vertices_orig;
		delete[] node.normals_orig;
	}
}