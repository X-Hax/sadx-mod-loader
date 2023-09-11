#pragma once
#include "ninja.h"

WeightInfo* LoadWeights(const char* filename);

void FreeWeights(WeightInfo* weights);

void InitWeights(WeightInfo* weights, NJS_OBJECT* object);

void ApplyWeights(WeightInfo* weights, NJS_ACTION* action, float frame);

void DeInitWeights(WeightInfo* weights, NJS_OBJECT* object);