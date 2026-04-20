#include "stdafx.h"
#include "IniFile.hpp"
#include "UsercallFunctionHandler.h"

using std::string;
using std::vector;
using std::unordered_map;

vector<PL_JOIN_VERTEX> so_jvlist;
vector<PL_JOIN_VERTEX> egg_jvlist;
vector<PL_JOIN_VERTEX> miles_jvlist;
vector<PL_JOIN_VERTEX> knux_jvlist;
vector<PL_JOIN_VERTEX> tikal_jvlist;
vector<PL_JOIN_VERTEX> amy_jvlist;
vector<PL_JOIN_VERTEX> big_jvlist;

DataPointer(NJS_ACTION, action_g_g0001_eggman, 0x0089E254);
DataPointer(NJS_ACTION, action_ti_dame, 0x008F46BC);

NJS_OBJECT* eggman_objects[17];
NJS_OBJECT* tikal_objects[23];

static UsercallFuncVoid(Knuckles_Upgrades_t, (playerwk* a1), (a1), 0x4726A0, rEAX);
static vector<uint16_t> Knux_HandIndices;
static vector<uint16_t> Knux_ShovelClawIndices;
static vector<uint16_t> Knux_FightingGlovesIndices;

// These are all part of processing Knuckles upgrades a bit easier.
static unordered_map<string, int> Knuckles_Hands;
static unordered_map<string, vector<uint16_t>> Knuckles_Indices;
string hr = "hand_right";
string hl = "hand_left";
string bhr = "base" + hr;
string bhl = "base" + hl;
string fhr = "flight" + hr;
string fhl = "flight" + hl;
string scr = "shovelclaw_right";
string scl = "shovelclaw_left";
string fgr = "fightinggloves_right";
string fgl = "fightinggloves_left";
string fgscr = "fightingshovelclaw_right";
string fgscl = "fightingshovelclaw_left";

void FreeJVListIndices()
{
	for (uint32_t i = 0; i < so_jvlist.size(); i++)
	{
		auto pnum = so_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

	for (uint32_t i = 0; i < egg_jvlist.size(); i++)
	{
		auto pnum = egg_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

	for (uint32_t i = 0; i < miles_jvlist.size(); i++)
	{
		auto pnum = miles_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

	for (uint32_t i = 0; i < knux_jvlist.size(); i++)
	{
		auto pnum = knux_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

	for (uint32_t i = 0; i < tikal_jvlist.size(); i++)
	{
		auto pnum = tikal_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

	for (uint32_t i = 0; i < amy_jvlist.size(); i++)
	{
		auto pnum = amy_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

	for (uint32_t i = 0; i < big_jvlist.size(); i++)
	{
		auto pnum = big_jvlist.at(i).pnum;

		if (pnum)
		{
			delete[] pnum;
		}
	}

}

static bool isWhiteSpace(const string s) 
{
	if (s.empty())
		return true;

	if (s.find_first_not_of(' ') != std::string::npos)
	{
		return false;
	}

	return true;
}

static string RemoveAnySpace(string str)
{
	std::string::iterator end_pos = std::remove(str.begin(), str.end(), ' ');
	str.erase(end_pos, str.end());
	return str;
}

void SetIndices(string indices, vector<uint16_t>& points)
{
	indices = RemoveAnySpace(indices);
	std::stringstream ss(indices);
	string out = "";
	const char de = ',';
	while (std::getline(ss, out, de))
	{
		points.push_back(stoi(out));
	}
}

PL_JOIN_VERTEX BuildJVListEntry(NJS_OBJECT* arr[], int base, int mdlA, int mdlB, int type, string indices)
{
	PL_JOIN_VERTEX jvEntry = {};

	jvEntry.objptr = (NJS_OBJECT*)arr[base];
	jvEntry.srcobj = (NJS_OBJECT*)arr[mdlA];
	jvEntry.inpmode = type;
	jvEntry.dstdepth = 0;
	jvEntry.org = 0;
	jvEntry.srcdepth = 0;

	if (mdlB == 0)
	{
		jvEntry.dstobj = 0;
		jvEntry.numVertex = 0;
		jvEntry.pnum = 0;
	}
	else
	{
		std::vector<uint16_t> points;
		SetIndices(indices, points);
		jvEntry.dstobj = (NJS_OBJECT*)arr[mdlB];
		jvEntry.numVertex = (points.size() / 2);
		uint16_t* ind_arr = new uint16_t[points.size()];
		for (int i = 0; i != points.size(); i++)
			ind_arr[i] = points.data()[i];
		jvEntry.pnum = ind_arr;
	}

	return jvEntry;
}

static int GetIntData(const string id, IniFile* ini, const string st)
{
	string s = ini->getString(id, st);
	s = RemoveAnySpace(s);
	return isWhiteSpace(s) ? 0 : stoi(s); //return number if it exists
}

void ProcessKnucklesUpgrades(const IniFile* ini)
{
	// Because Knuckles has weird handling for his upgrades, we get to do some annoying work arounds to resolve it without being overly intrusive.
	string group = "Upgrades";
	if (ini->hasGroup(group))
	{
		// If the keys exist and have content, we can go ahead and get the indices.
		if (ini->hasKeyNonEmpty(group, bhr) &&
			ini->hasKeyNonEmpty(group, bhl) &&
			ini->hasKeyNonEmpty(group, fhr) &&
			ini->hasKeyNonEmpty(group, fhl))
		{
			// These indices are the hand entries in the JVList that will be updated later.
			int bhr_idx = ini->getInt(group, bhr);
			int bhl_idx = ini->getInt(group, bhl);
			int fhr_idx = ini->getInt(group, fhr);
			int fhl_idx = ini->getInt(group, fhl);

			// We check to see if the right and left indices are the same for the base and flight models. If they are, we don't do any further processing.
			// This could lead to potential errors and/or crashes, so it's best to require these indices be different.
			if (bhr_idx != bhl_idx || fhr_idx != fhl_idx)
			{
				Knuckles_Hands.insert_or_assign(bhr, bhr_idx);
				Knuckles_Hands.insert_or_assign(bhl, bhl_idx);
				Knuckles_Hands.insert_or_assign(fhr, fhr_idx);
				Knuckles_Hands.insert_or_assign(fhl, fhl_idx);

				// We then process the indices for the hands (as fallbacks) and for each upgrade.
				vector<uint16_t> hr_vec;
				vector<uint16_t> hl_vec;
				vector<uint16_t> scr_vec;
				vector<uint16_t> scl_vec;
				vector<uint16_t> fgr_vec;
				vector<uint16_t> fgl_vec;
				vector<uint16_t> fgscr_vec;
				vector<uint16_t> fgscl_vec;

				string right = std::to_string(bhr_idx);
				string left = std::to_string(bhl_idx);
				string right_indices = ini->getString(right, "VertIndexes");
				string left_indices = ini->getString(left, "VertIndexes");
				SetIndices(right_indices, hr_vec);
				SetIndices(left_indices, hl_vec);
				Knuckles_Indices.insert_or_assign(hr, hr_vec);
				Knuckles_Indices.insert_or_assign(hl, hl_vec);

				// Running a check if the key exists is done to prevent unecessary attempts at processing potentially empty data.
				if (ini->hasKey(group, scr))
				{
					string scr_indices = ini->getString(group, scr);
					SetIndices(scr_indices, scr_vec);
				}

				if (ini->hasKey(group, scl))
				{
					string scl_indices = ini->getString(group, scl);
					SetIndices(scl_indices, scl_vec);
				}

				if (ini->hasKey(group, fgr))
				{
					string fgr_indices = ini->getString(group, fgr);
					SetIndices(fgr_indices, fgr_vec);
				}

				if (ini->hasKey(group, fgl))
				{
					string fgl_indices = ini->getString(group, fgl);
					SetIndices(fgl_indices, fgl_vec);
				}

				if (ini->hasKey(group, fgscr))
				{
					string fgscr_indices = ini->getString(group, fgscr);
					SetIndices(fgscr_indices, fgscr_vec);
				}

				if (ini->hasKey(group, fgscl))
				{
					string fgscl_indices = ini->getString(group, fgscl);
					SetIndices(fgscl_indices, fgscl_vec);
				}

				// Regardless of if the data is populated or is 0, we insert all of them into the map for use.
				Knuckles_Indices.insert_or_assign(scr, scr_vec);
				Knuckles_Indices.insert_or_assign(scl, scl_vec);
				Knuckles_Indices.insert_or_assign(fgr, fgr_vec);
				Knuckles_Indices.insert_or_assign(fgl, fgl_vec);
				Knuckles_Indices.insert_or_assign(fgscr, fgscr_vec);
				Knuckles_Indices.insert_or_assign(fgscl, fgscl_vec);
			}
		}
	}
}

void CreateJVList(NJS_OBJECT* arr[], IniFile* ini, vector<PL_JOIN_VERTEX>& jvlist, bool isKnux = false)
{
	int i = 0;
	while (ini->hasGroup(std::to_string(i)))
	{
		string s = std::to_string(i);

		if (isKnux && ini->hasKey(s, "shovel"))
		{
			string shovelIndice = ini->getString(s, "shovel", "");
			SetIndices(shovelIndice, Knux_ShovelClawIndices);
		}
		else
		{
			int base = GetIntData(s, ini, "BaseModel"); //ini->getInt(s, "BaseModel");
			int mdlA = GetIntData(s, ini, "ModelA"); // ini->getInt(s, "ModelA");
			int mdlB = GetIntData(s, ini, "ModelB"); //ini->getInt(s, "ModelB");
			int type = GetIntData(s, ini, "WeldType");// ini->getInt(s, "WeldType");
			string indices = ini->getString(s, "VertIndexes", "");

			PL_JOIN_VERTEX jvEntry = BuildJVListEntry(arr, base, mdlA, mdlB, type, indices);
			if (ini->hasKeyNonEmpty(s, "Direction"))
				jvEntry.numVertex = ini->getInt(s, "Direction");
			jvlist.push_back(jvEntry);

			if (mdlA == 20 && isKnux)
			{
				SetIndices(indices, Knux_HandIndices);
			}
		}

		i++;
	}

	jvlist.push_back({ 0 });

	if (isKnux)
		ProcessKnucklesUpgrades(ini);

	delete ini;
}

void SetNPCWelds(PL_JOIN_VERTEX* dest, vector<PL_JOIN_VERTEX> &origin, const uint16_t size)
{
	if (!dest || origin.empty())
		return;

	uint16_t count = 0;

	for (uint16_t i = 0; i < origin.size(); i++)
	{
		if (origin.at(i).inpmode <= 3 && origin.at(i).srcobj && origin.at(i).dstobj) //npcs only support a specific type of welds with valid models
		{
			dest[count] = origin.at(i);
			count++;
		}

		if (count >= size)
		{
			dest[count + 1] = { 0 };
			break;
		}		
	}
}

void SetSonicNewWelds(IniFile* file)
{
	so_jvlist.clear();

	CreateJVList(SONIC_OBJECTS, file, so_jvlist);
	WriteData((PL_JOIN_VERTEX**)0x49AB7E, so_jvlist.data());
	WriteData((PL_JOIN_VERTEX**)0x49ABAC, so_jvlist.data());
	WriteData((PL_JOIN_VERTEX**)0x49AC3C, so_jvlist.data());
	WriteData((PL_JOIN_VERTEX**)0x49ACB6, so_jvlist.data());

	const auto sizeNPC = NPCSonicWeldInfo.size();
	SetNPCWelds((PL_JOIN_VERTEX*)&NPCSonicWeldInfo, so_jvlist, sizeNPC);
	WriteData<1>((int*)0x7D14D0, 0xC3); //remove init NPC welds so they don't overwrite the changes here
}

void SetEggmanNewWelds(IniFile* file)
{
	egg_jvlist.clear();

	auto obj = action_g_g0001_eggman.object;
	eggman_objects[0] = obj;
	eggman_objects[1] = obj->getnode(9);
	eggman_objects[2] = obj->getnode(10);
	eggman_objects[3] = obj->getnode(4);
	eggman_objects[4] = obj->getnode(5);
	eggman_objects[5] = obj->getnode(11);
	eggman_objects[6] = obj->getnode(13);
	eggman_objects[7] = obj->getnode(6);
	eggman_objects[8] = obj->getnode(8);

	eggman_objects[9] = obj->getnode(20);
	eggman_objects[10] = obj->getnode(21);
	eggman_objects[11] = obj->getnode(16);
	eggman_objects[12] = obj->getnode(17);
	eggman_objects[13] = obj->getnode(22);
	eggman_objects[14] = obj->getnode(23);
	eggman_objects[15] = obj->getnode(18);
	eggman_objects[16] = obj->getnode(19);

	CreateJVList(eggman_objects, file, egg_jvlist);
	WriteData((PL_JOIN_VERTEX**)0x7B4FBF, egg_jvlist.data());
}

void SetMilesNewWelds(IniFile* file)
{
	miles_jvlist.clear();

	CreateJVList(MILES_OBJECTS, file, miles_jvlist);
	WriteData((PL_JOIN_VERTEX**)0x461896, miles_jvlist.data());

	const auto size = NPCTailsWeldInfo.size();
	SetNPCWelds((PL_JOIN_VERTEX*)&NPCTailsWeldInfo, miles_jvlist, size);
	WriteData<1>((int*)0x7C7560, 0xC3);
}

void Knuckles_Upgrades_r(playerwk* pwp)
{
	Knuckles_Upgrades_t.Original(pwp);

	// If the Hands and Upgrades maps have data, we process them the new way, otherwise we default to the old method.
	if (Knuckles_Hands.size() > 0 && Knuckles_Indices.size() > 0)
	{
		uint16_t* right_indices = Knuckles_Indices[hr].data();
		int right_count = Knuckles_Indices[hr].size() / 2;
		uint16_t* left_indices = Knuckles_Indices[hl].data();
		int left_count = Knuckles_Indices[hl].size() / 2;

		switch (pwp->equipment & (Upgrades_ShovelClaw | Upgrades_FightingGloves))
		{
		case Upgrades_ShovelClaw:
			right_indices = Knuckles_Indices[scr].data();
			right_count = Knuckles_Indices[scr].size() / 2;
			left_indices = Knuckles_Indices[scl].data();
			left_count = Knuckles_Indices[scl].size() / 2;
			break;
		case Upgrades_FightingGloves:
			right_indices = Knuckles_Indices[fgr].data();
			right_count = Knuckles_Indices[fgr].size() / 2;
			left_indices = Knuckles_Indices[fgl].data();
			left_count = Knuckles_Indices[fgl].size() / 2;
			break;
		case Upgrades_ShovelClaw | Upgrades_FightingGloves:
			right_indices = Knuckles_Indices[fgscr].data();
			right_count = Knuckles_Indices[fgscr].size() / 2;
			left_indices = Knuckles_Indices[fgscl].data();
			left_count = Knuckles_Indices[fgscl].size() / 2;
			break;
		}

		// If either of the indices is an invalid pointer, we abort assignment to the jvlist for safety.
		if (!right_indices || !left_indices)
			return;

		int baseright = Knuckles_Hands[bhr];
		int baseleft = Knuckles_Hands[bhr];
		int flightright = Knuckles_Hands[fhr];
		int flightleft = Knuckles_Hands[fhl];

		knux_jvlist[baseright].pnum = right_indices;
		knux_jvlist[baseleft].numVertex = right_count;
		knux_jvlist[baseright].pnum = left_indices;
		knux_jvlist[baseleft].numVertex = left_count;
		knux_jvlist[flightright].pnum = right_indices;
		knux_jvlist[flightright].numVertex = right_count;
		knux_jvlist[flightleft].pnum = left_indices;
		knux_jvlist[flightleft].numVertex = left_count;

		NPCKnucklesWeldInfo[baseright].VertIndexes = right_indices;
		NPCKnucklesWeldInfo[baseright].VertexPairCount = right_count;
		NPCKnucklesWeldInfo[baseleft].VertIndexes = left_indices;
		NPCKnucklesWeldInfo[baseleft].VertexPairCount = left_count;
		NPCKnucklesWeldInfo[flightright].VertIndexes = right_indices;
		NPCKnucklesWeldInfo[flightright].VertexPairCount = right_count;
		NPCKnucklesWeldInfo[flightleft].VertIndexes = left_indices;
		NPCKnucklesWeldInfo[flightleft].VertexPairCount = left_count;
	}
	else
	{
		uint16_t* indice = Knux_HandIndices.data();

		if (!indice)
			return;

		switch (pwp->equipment & (Upgrades_ShovelClaw | Upgrades_FightingGloves))
		{
		case Upgrades_ShovelClaw:
		case Upgrades_ShovelClaw | Upgrades_FightingGloves:

			if (Knux_ShovelClawIndices.size() > 0)
			{
				indice = Knux_ShovelClawIndices.data();
			}
			else
			{
				indice = (uint16_t*)&Knuckles_ShovelClawIndices;
			}

			break;
		}

		knux_jvlist.at(23).pnum = indice;
		knux_jvlist.at(22).pnum = indice;
		knux_jvlist.at(11).pnum = indice;
		knux_jvlist.at(10).pnum = indice;

		NPCKnucklesWeldInfo[23].VertIndexes = indice;
		NPCKnucklesWeldInfo[22].VertIndexes = indice;
		NPCKnucklesWeldInfo[11].VertIndexes = indice;
		NPCKnucklesWeldInfo[10].VertIndexes = indice;
	}
}

void SetKnucklesNewWelds(IniFile* file)
{
	knux_jvlist.clear();

	CreateJVList(KNUCKLES_OBJECTS, file, knux_jvlist, true);
	WriteData((PL_JOIN_VERTEX**)0x47A89E, knux_jvlist.data());

	const auto size = NPCKnucklesWeldInfo.size();
	SetNPCWelds((PL_JOIN_VERTEX*)&NPCKnucklesWeldInfo, knux_jvlist, size);
	WriteData<1>((int*)0x7C9C80, 0xc3);
	Knuckles_Upgrades_t.Hook(Knuckles_Upgrades_r);
}

void SetTikalNewWelds(IniFile* file)
{
	tikal_jvlist.clear();

	auto obj = action_ti_dame.object;

	tikal_objects[0] = obj;

	//right arm
	tikal_objects[1] = obj->getnode(86);
	tikal_objects[2] = obj->getnode(85);
	tikal_objects[3] = obj->getnode(84);
	tikal_objects[4] = obj->getnode(82);
	tikal_objects[5] = obj->getnode(81);
	//left arm
	tikal_objects[6] = obj->getnode(72);
	tikal_objects[7] = obj->getnode(71);
	tikal_objects[8] = obj->getnode(70);
	tikal_objects[9] = obj->getnode(68);
	tikal_objects[10] = obj->getnode(67);
	//right leg
	tikal_objects[11] = obj->getnode(41);
	tikal_objects[12] = obj->getnode(40);
	tikal_objects[13] = obj->getnode(39);
	tikal_objects[14] = obj->getnode(38);
	tikal_objects[15] = obj->getnode(37);
	//left leg
	tikal_objects[16] = obj->getnode(25);
	tikal_objects[17] = obj->getnode(24);
	tikal_objects[18] = obj->getnode(23);
	tikal_objects[19] = obj->getnode(22);
	tikal_objects[20] = obj->getnode(21);
	//dress, torso
	tikal_objects[21] = obj->getnode(42);
	tikal_objects[22] = obj->getnode(4);

	CreateJVList(tikal_objects, file, tikal_jvlist);
	WriteData((PL_JOIN_VERTEX**)0x7B41AB, tikal_jvlist.data());
}

void SetAmyNewWelds(IniFile* file)
{
	amy_jvlist.clear();

	CreateJVList(AMY_OBJECTS, file, amy_jvlist);
	WriteData((PL_JOIN_VERTEX**)0x48AD0B, amy_jvlist.data());

	const auto size = NPCAmyWeldInfo.size();
	SetNPCWelds((PL_JOIN_VERTEX*)&NPCAmyWeldInfo, amy_jvlist, size);
	WriteData<1>((int*)0x7CD000, 0xC3);
}

void SetBigNewWelds(IniFile* file)
{
	big_jvlist.clear();

	CreateJVList(BIG_OBJECTS, file, big_jvlist);
	WriteData((PL_JOIN_VERTEX**)0x490C14, big_jvlist.data());

	const auto size = NPCBigWeldInfo.size();
	SetNPCWelds((PL_JOIN_VERTEX*)&NPCBigWeldInfo, big_jvlist, size);
	WriteData<1>((int*)0x7D5EB0, 0xC3);
}

void SetNewWelds(const uint8_t character, IniFile* file)
{
	switch (character)
	{
	case Characters_Sonic:
		SetSonicNewWelds(file);
		break;
	case Characters_Eggman:
		SetEggmanNewWelds(file);
		break;
	case Characters_Tails:
		SetMilesNewWelds(file);
		break;
	case Characters_Knuckles:
		SetKnucklesNewWelds(file);
		break;
	case Characters_Tikal:
		SetTikalNewWelds(file);
		break;
	case Characters_Amy:
		SetAmyNewWelds(file);
		break;
	case Characters_Big:
		SetBigNewWelds(file);
		break;
	}
}