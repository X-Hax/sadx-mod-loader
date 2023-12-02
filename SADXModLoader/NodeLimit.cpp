#include "stdafx.h"

//SADX originally only allow 111 nodes / bones max per character for interpolation 
//We hack some functions to increase the limit.

FunctionHook<void, mtnjvwk*> PInitializeInterpolateMotion2_t(0x43F2A0);

#define AnimLimit 0xFFui8 //for now, the new limit is 255 since "objnum" is an uint8_t, it can be increased to more with a custom struct, but would require to hack all the places where the game uses the node count

/// <summary>
/// Initializes the interpolation for player motions
/// </summary>
/// <param name="mjp">- Player's MotionJvWk</param>
void PInitializeInterpolateMotion2_r(mtnjvwk* mjp)
{
	if (mjp)
	{
		PL_ACTION* plactptr = mjp->plactptr;

		//if the animation has less than 112 nodes, we let the original code handle it
		if (plactptr && plactptr[mjp->reqaction].objnum <= 111)
		{
			return PInitializeInterpolateMotion2_t.Original(mjp);
		}
	}

	NJS_MKEY_A* key_ang = nullptr;
	NJS_MKEY_F* key_pos = nullptr;
	NJS_MKEY_F* key_scl = nullptr;
	NJS_MKEY_F* req_scl = nullptr;
	NJS_OBJECT* objlist[AnimLimit] = { nullptr }; //use an array of 255 instead of 111
	Bool foundValue = FALSE;

	mjp->actwkptr->object = mjp->plactptr[mjp->reqaction].actptr->object;
	ListTheObjectTreeByTheEnd(mjp->actwkptr->object, objlist);
	PL_ACTION* plactptr = mjp->plactptr;
	NJS_MOTION* actptr_mtn = plactptr[mjp->action].actptr->motion;

	//Create an MDATA2 and an MDATA3 if needed later.
	NJS_MDATA2* mdata = (NJS_MDATA2*)actptr_mtn->mdata;
	NJS_MDATA2* actwk_mdata = (NJS_MDATA2*)mjp->actwkptr->motion->mdata;
	NJS_MDATA2* req_mdata = (NJS_MDATA2*)plactptr[mjp->reqaction].actptr->motion->mdata;
	NJS_MDATA3* mdata3 = (NJS_MDATA3*)actptr_mtn->mdata;
	NJS_MDATA3* actwk_mdata3 = (NJS_MDATA3*)mjp->actwkptr->motion->mdata;
	NJS_MDATA3* req_mdata3 = (NJS_MDATA3*)plactptr[mjp->reqaction].actptr->motion->mdata;
	Uint32 objnum = 0;

	Bool reqaction_IsMData3 = (((plactptr[mjp->reqaction].actptr->motion->type & 4 | 8u) >> 2) == 3);
	Bool action_IsMData3 = (((plactptr[mjp->action].actptr->motion->type & 4 | 8u) >> 2) == 3);

	if (plactptr[mjp->reqaction].objnum != 0)
	{
		do
		{
			NJS_OBJECT* obj = objlist[objnum];

			//Pos
			if ((req_mdata->p[0] || mdata->p[0]))
			{
				key_pos = (NJS_MKEY_F*)MAlloc(0x20 * 4);
				actwk_mdata->p[0] = key_pos;
				key_pos->keyframe = 0;
				NJS_MKEY_F* pPos = (NJS_MKEY_F*)mdata->p[0];
				if (mdata->p[0])
				{
					Uint32 frame_count = 0;
					Uint32 pos_id = 0;
					foundValue = 0;
					if (pPos->keyframe < mjp->nframe)
					{
						frame_count = mdata->nb[0] - 1;//nb[0]
						NJS_MKEY_F* pPos_1 = (NJS_MKEY_F*)mdata->p[0];
						while (pos_id < frame_count)
						{
							++pPos_1;
							frame_count = pos_id++;
							if (pPos_1->keyframe >= mjp->nframe)
							{
								foundValue = TRUE;
								break;
							}
							else {
								frame_count = mdata->nb[0] - 1; //nb[0]
							}
						}
						if (!foundValue) {
							pos_id = 0;
						}
					}
					NJS_MKEY_F* pos = &pPos[frame_count];
					key_pos->key[0] = pos->key[0];
					key_pos->key[1] = pos->key[1];
					key_pos->key[2] = pos->key[2];
					if (pos_id != frame_count)
					{
						NJS_MKEY_F* next_pos = &pPos[pos_id];
						Uint32 frame_1 = next_pos->keyframe - pos->keyframe;
						if (pos_id < frame_count)
						{
							frame_1 += actptr_mtn->nbFrame;
						}
						Float frame_2 = (mjp->nframe - (Float)pos->keyframe) / (Float)frame_1;
						key_pos->key[0] = (next_pos->key[0] - pos->key[0]) * frame_2 + key_pos->key[0];
						key_pos->key[1] = (next_pos->key[1] - pos->key[1]) * frame_2 + key_pos->key[1];
						key_pos->key[2] = (next_pos->key[2] - pos->key[2]) * frame_2 + key_pos->key[2];
					}
				}
				else
				{
					key_pos->key[0] = obj->pos[0];
					key_pos->key[1] = obj->pos[1];
					key_pos->key[2] = obj->pos[2];
				}
				key_pos[1].keyframe = 1;
				NJS_MKEY_F* req_pos = (NJS_MKEY_F*)req_mdata->p[0];
				if (req_mdata->p[0])
				{
					key_pos[1].key[0] = req_pos->key[0];
					key_pos[1].key[1] = req_pos->key[1];
					key_pos[1].key[2] = req_pos->key[2];
				}
				else
				{
					key_pos[1].key[0] = obj->pos[0];
					key_pos[1].key[1] = obj->pos[1];
					key_pos[1].key[2] = obj->pos[2];
				}
				actwk_mdata->nb[0] = 2;
			}
			else
			{
				actwk_mdata->p[0] = NULL;
				actwk_mdata->nb[0] = NULL;
			}

			//Angle
			if ((req_mdata->p[1] || mdata->p[1]))
			{
				key_ang = (NJS_MKEY_A*)MAlloc(0x20 * 4);
				actwk_mdata->p[1] = key_ang;
				key_ang->keyframe = 0;
				NJS_MKEY_A* pAngle = (NJS_MKEY_A*)mdata->p[1];
				if (pAngle)
				{
					Uint32 ang_frame_id = 0;
					Uint32 ang_id = 0;
					foundValue = 0;
					if (pAngle->keyframe < mjp->nframe)
					{

						ang_frame_id = mdata->nb[1] - 1;
						NJS_MKEY_A* pAngle_1 = (NJS_MKEY_A*)mdata->p[1];
						while (ang_id < ang_frame_id)
						{
							++pAngle_1;
							ang_frame_id = ang_id++;
							if (pAngle_1->keyframe >= mjp->nframe)
							{
								foundValue = 1;
								break;
							}
							else {
								ang_frame_id = mdata->nb[1] - 1;
							}
						}
						if (!foundValue)
						{
							ang_id = 0;
						}
					}
					NJS_MKEY_A* next_angle = &pAngle[ang_frame_id];
					key_ang->key[0] = next_angle->key[0];
					key_ang->key[1] = next_angle->key[1];
					key_ang->key[2] = next_angle->key[2];
					if (ang_id != ang_frame_id)
					{
						NJS_MKEY_A* angle = &pAngle[ang_id];
						Uint32 frame = angle->keyframe - next_angle->keyframe;
						if (ang_id < ang_frame_id)
						{
							frame += actptr_mtn->nbFrame;
						}
						Float ang_frame = (mjp->nframe - (Float)next_angle->keyframe) / (Float)frame;
						key_ang->key[0] += (Sint32)((Float)SubAngle(next_angle->key[0], angle->key[0]) * ang_frame);
						key_ang->key[1] += (Sint32)((Float)SubAngle(next_angle->key[1], angle->key[1]) * ang_frame);
						key_ang->key[2] += (Sint32)((Float)SubAngle(next_angle->key[2], angle->key[2]) * ang_frame);
					}
				}
				else
				{
					key_ang->key[0] = obj->ang[0];
					key_ang->key[1] = obj->ang[1];
					key_ang->key[2] = obj->ang[2];
				}
				key_ang[1].keyframe = 1;
				NJS_MKEY_A* req_ang = (NJS_MKEY_A*)req_mdata->p[1];
				if (req_ang)
				{
					key_ang[1].key[0] = req_ang->key[0];
					key_ang[1].key[1] = req_ang->key[1];
					key_ang[1].key[2] = req_ang->key[2];
				}
				else
				{
					key_ang[1].key[0] = obj->ang[0];
					key_ang[1].key[1] = obj->ang[1];
					key_ang[1].key[2] = obj->ang[2];
				}
				key_ang->key[0] = (Sint16)(key_ang->key[0]);
				key_ang->key[1] = (Sint16)(key_ang->key[1]);
				key_ang->key[2] = (Sint16)(key_ang->key[2]);
				key_ang[1].key[0] = (Sint16)(key_ang[1].key[0]);
				key_ang[1].key[1] = (Sint16)(key_ang[1].key[1]);
				key_ang[1].key[2] = (Sint16)(key_ang[1].key[2]);
				key_ang[1].key[0] = CreateNinjaAngle(key_ang->key[0], key_ang[1].key[0]);
				key_ang[1].key[1] = CreateNinjaAngle(key_ang->key[1], key_ang[1].key[1]);
				key_ang[1].key[2] = CreateNinjaAngle(key_ang->key[2], key_ang[1].key[2]);
				actwk_mdata->nb[1] = 2;
			}
			else
			{
				actwk_mdata->p[1] = NULL;
				actwk_mdata->nb[1] = NULL;
			}

			//Scale - Only if MDATA3
			if (action_IsMData3 || reqaction_IsMData3)
			{
				//Shift nb to account for 4-byte difference between NJS_MDATA2 and NJS_MDATA3
				actwk_mdata3 = (NJS_MDATA3*)actwk_mdata;
				actwk_mdata3->nb[1] = actwk_mdata->nb[1];
				actwk_mdata3->nb[0] = actwk_mdata->nb[0];

				if (((reqaction_IsMData3 && req_mdata3->p[2]) || (action_IsMData3 && mdata3->p[2])))
				{
					key_scl = (NJS_MKEY_F*)MAlloc(0x20 * 4);
					actwk_mdata3->p[2] = key_scl;
					key_scl->keyframe = 0;
					if (action_IsMData3)
					{
						NJS_MKEY_F* pScl = (NJS_MKEY_F*)mdata3->p[2];
						if (pScl)
						{
							Uint32 scl_frame_id = 0;
							Uint32 scl_id = 0;
							foundValue = 0;
							if (pScl->keyframe < mjp->nframe)
							{
								scl_frame_id = mdata3->nb[2] - 1;
								NJS_MKEY_F* pScl_1 = (NJS_MKEY_F*)mdata3->p[2];
								while (scl_id < scl_frame_id)
								{
									++pScl_1;
									scl_frame_id = scl_id++;
									if (pScl_1->keyframe >= mjp->nframe)
									{
										foundValue = 1;
										break;
									}
									else {
										scl_frame_id = mdata3->nb[2] - 1;
									}
								}

								if (!foundValue)
								{
									scl_id = 0;
								}
							}
							NJS_MKEY_F* scl = &pScl[scl_frame_id];
							key_scl->key[0] = scl->key[0];
							key_scl->key[1] = scl->key[1];
							key_scl->key[2] = scl->key[2];
							if (scl_id != scl_frame_id)
							{
								NJS_MKEY_F* next_scl = &pScl[scl_id];
								Uint32 scl_keyf = next_scl->keyframe - scl->keyframe;
								if (scl_id < scl_frame_id)
								{
									scl_keyf += actptr_mtn->nbFrame;
								}
								Float frame_scl = (mjp->nframe - (Float)scl->keyframe) / (Float)scl_keyf;
								key_scl->key[0] = (next_scl->key[0] - scl->key[0]) * frame_scl + key_scl->key[0];
								key_scl->key[1] = (next_scl->key[1] - scl->key[1]) * frame_scl + key_scl->key[1];
								key_scl->key[2] = (next_scl->key[2] - scl->key[2]) * frame_scl + key_scl->key[2];
							}
						}
					}
					else {
						key_scl->key[0] = obj->scl[0];
						key_scl->key[1] = obj->scl[1];
						key_scl->key[2] = obj->scl[2];
					}

					key_scl[1].keyframe = 1;
					if (reqaction_IsMData3 && (req_scl = (NJS_MKEY_F*)req_mdata3->p[2]) != 0)
					{
						key_scl[1].key[0] = req_scl->key[0];
						key_scl[1].key[1] = req_scl->key[1];
						key_scl[1].key[2] = req_scl->key[2];
						actwk_mdata3->nb[2] = 2;
					}
					else
					{
						key_scl[1].key[0] = obj->scl[0];
						key_scl[1].key[1] = obj->scl[1];
						key_scl[1].key[2] = obj->scl[2];
						actwk_mdata3->nb[2] = 2;
					}
				}
				else {
					actwk_mdata3->p[2] = NULL;
					actwk_mdata3->nb[2] = NULL;
				}
			}

			//Increment MDATA
			if (reqaction_IsMData3 || action_IsMData3)
			{
				++actwk_mdata3;
				actwk_mdata = (NJS_MDATA2*)actwk_mdata3;
			}
			else
			{
				++actwk_mdata;
			}

			if (reqaction_IsMData3)
			{
				++req_mdata3;
				req_mdata = (NJS_MDATA2*)req_mdata3;
			}
			else
			{
				++req_mdata;
			}

			if (action_IsMData3)
			{
				++mdata3;
				mdata = (NJS_MDATA2*)mdata3;
			}
			else
			{
				++mdata;
			}

			++objnum;
		} while (objnum < plactptr[mjp->reqaction].objnum);
	}
	if (reqaction_IsMData3 || action_IsMData3)
	{
		mjp->actwkptr->motion->type = 7;
		mjp->actwkptr->motion->inp_fn = 3;
	}
	else
	{
		mjp->actwkptr->motion->type = 3;
		mjp->actwkptr->motion->inp_fn = 2;
	}
}


NJS_MATRIX InstanceMatrices_r[AnimLimit];
void CalcMMMatrix_r(NJS_MATRIX_PTR inpmat, NJS_ACTION* actptr, Float motfrm, Uint32 srcnmb, NJS_MATRIX_PTR ansmat)
{
	SMMparams params;
	CurrentModelIndex = 0;
	LastModelIndex = srcnmb + 1;

	njPushMatrix(0);
	if (inpmat)
	{
		njSetMatrix(0, inpmat);
	}

	NJS_OBJECT* object = actptr->object;
	NJS_MOTION* motion = actptr->motion;
	params.mot = motion->mdata;
	params.mbp = InstanceMatrices_r;
	params.type = motion->type;
	params.motfrm = motion->nbFrame;
	params.frm = motfrm - (Float)(motion->nbFrame * ((Uint32)motfrm / motion->nbFrame));
	ScanMotionModel(object, &params);
	if (ansmat)
	{
		njSetMatrix(ansmat, InstanceMatrices_r[srcnmb]);
	}
	njPopMatrix(1);
}

Sint32 __cdecl GetMMMatrix_r(Uint32 index, NJS_MATRIX_PTR matrix)
{
	if (index >= CurrentModelIndex)
	{
		return 0;
	}

	njSetMatrix(matrix, InstanceMatrices_r[index]);
	return 1;
}

void* __cdecl CAlloc_r(int count, int size)
{
	return CAlloc(AnimLimit, size);
}

void IncreaseNodeLimit()
{
	WriteCall((void*)0x44A8B5, CAlloc_r);
	WriteJump(CalcMMMatrix, CalcMMMatrix_r);
	PInitializeInterpolateMotion2_t.Hook(PInitializeInterpolateMotion2_r);
	WriteJump((void*)0x4B82D0, GetMMMatrix_r);
}