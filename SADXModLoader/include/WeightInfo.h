#pragma once
#include "ninja.h"

struct WeightVertex
{
	NJS_OBJECT* node;
	int nodeIndex;
	int vertex;
	float weight;
};

struct WeightVertexList
{
	int index;
	WeightVertex* vertices;
	int vertexCount;
};

struct WeightNode
{
	NJS_OBJECT* node;
	int nodeIndex;
	WeightVertexList* weights;
	int weightCount;
	NJS_VECTOR* vertices_orig;
	NJS_VECTOR* normals_orig;
};

struct WeightInfo
{
	WeightNode* nodes;
	int nodeCount;
};
