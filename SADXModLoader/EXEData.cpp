#include "stdafx.h"
#include <Shlwapi.h>
#include <cassert>
#include <unordered_map>
#include <vector>
#include <string>
#include <fstream>
#include "IniFile.hpp"
#include "TextConv.hpp"
#include "SADXModLoader.h"
#include "LandTableInfo.h"
#include "ModelInfo.h"
#include "AnimationFile.h"
#include "EXEData.h"
#include "FileSystem.h"

using std::unordered_map;
using std::vector;
using std::string;
using std::wstring;
using std::ifstream;

UINT CodepageJapanese = 932;
UINT CodepageEnglish = 932;
UINT CodepageFrench = 1252;
UINT CodepageGerman = 1252;
UINT CodepageSpanish = 1252;

static vector<string>& split(const string& s, char delim, vector<string>& elems)
{
	std::stringstream ss(s);
	string item;

	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}

	return elems;
}


static vector<string> split(const string& s, char delim)
{
	vector<string> elems;
	split(s, delim, elems);
	return elems;
}

static string trim(const string& s)
{
	auto st = s.find_first_not_of(' ');

	if (st == string::npos)
	{
		st = 0;
	}

	auto ed = s.find_last_not_of(' ');

	if (ed == string::npos)
	{
		ed = s.size() - 1;
	}

	return s.substr(st, (ed + 1) - st);
}

template <typename T>
static inline T* arrcpy(T* dst, const T* src, size_t cnt)
{
	return (T*)memcpy(dst, src, cnt * sizeof(T));
}

template <typename T>
static inline void clrmem(T* mem)
{
	ZeroMemory(mem, sizeof(T));
}

static const unordered_map<string, uint8_t> levelidsnamemap = {
	{ "hedgehoghammer",    LevelIDs_HedgehogHammer },
	{ "emeraldcoast",      LevelIDs_EmeraldCoast },
	{ "windyvalley",       LevelIDs_WindyValley },
	{ "twinklepark",       LevelIDs_TwinklePark },
	{ "speedhighway",      LevelIDs_SpeedHighway },
	{ "redmountain",       LevelIDs_RedMountain },
	{ "skydeck",           LevelIDs_SkyDeck },
	{ "lostworld",         LevelIDs_LostWorld },
	{ "icecap",            LevelIDs_IceCap },
	{ "casinopolis",       LevelIDs_Casinopolis },
	{ "finalegg",          LevelIDs_FinalEgg },
	{ "hotshelter",        LevelIDs_HotShelter },
	{ "chaos0",            LevelIDs_Chaos0 },
	{ "chaos2",            LevelIDs_Chaos2 },
	{ "chaos4",            LevelIDs_Chaos4 },
	{ "chaos6",            LevelIDs_Chaos6 },
	{ "perfectchaos",      LevelIDs_PerfectChaos },
	{ "egghornet",         LevelIDs_EggHornet },
	{ "eggwalker",         LevelIDs_EggWalker },
	{ "eggviper",          LevelIDs_EggViper },
	{ "zero",              LevelIDs_Zero },
	{ "e101",              LevelIDs_E101 },
	{ "e101r",             LevelIDs_E101R },
	{ "stationsquare",     LevelIDs_StationSquare },
	{ "eggcarrieroutside", LevelIDs_EggCarrierOutside },
	{ "eggcarrierinside",  LevelIDs_EggCarrierInside },
	{ "mysticruins",       LevelIDs_MysticRuins },
	{ "past",              LevelIDs_Past },
	{ "twinklecircuit",    LevelIDs_TwinkleCircuit },
	{ "skychase1",         LevelIDs_SkyChase1 },
	{ "skychase2",         LevelIDs_SkyChase2 },
	{ "sandhill",          LevelIDs_SandHill },
	{ "ssgarden",          LevelIDs_SSGarden },
	{ "ecgarden",          LevelIDs_ECGarden },
	{ "mrgarden",          LevelIDs_MRGarden },
	{ "chaorace",          LevelIDs_ChaoRace },
	{ "invalid",           LevelIDs_Invalid }
};

static uint8_t ParseLevelID(const string& str)
{
	string str2 = trim(str);
	transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
	auto lv = levelidsnamemap.find(str2);
	return lv != levelidsnamemap.end() ? lv->second : (uint8_t)strtol(str.c_str(), nullptr, 10);
}

static uint16_t ParseLevelAndActID(const string& str)
{
	if (str.size() != 4)
	{
		vector<string> strs = split(str, ' ');
		return (uint16_t)((ParseLevelID(strs[0]) << 8) | strtol(strs[1].c_str(), nullptr, 10));
	}

	const char* cstr = str.c_str();
	char buf[3];
	buf[2] = 0;
	memcpy(buf, cstr, 2);
	auto result = (uint16_t)(strtol(buf, nullptr, 10) << 8);
	memcpy(buf, cstr + 2, 2);
	return result | (uint16_t)strtol(buf, nullptr, 10);
}

static const unordered_map<string, uint8_t> charflagsnamemap = {
	{ "sonic",    CharacterFlags_Sonic },
	{ "eggman",   CharacterFlags_Eggman },
	{ "tails",    CharacterFlags_Tails },
	{ "knuckles", CharacterFlags_Knuckles },
	{ "tikal",    CharacterFlags_Tikal },
	{ "amy",      CharacterFlags_Amy },
	{ "gamma",    CharacterFlags_Gamma },
	{ "big",      CharacterFlags_Big }
};

static uint8_t ParseCharacterFlags(const string& str)
{
	vector<string> strflags = split(str, ',');
	uint8_t flag            = 0;
	for (const auto& strflag : strflags)
	{
		string s = trim(strflag);
		transform(s.begin(), s.end(), s.begin(), ::tolower);
		auto ch = charflagsnamemap.find(s);

		if (ch != charflagsnamemap.end())
		{
			flag |= ch->second;
		}
	}
	return flag;
}

static const unordered_map<string, uint8_t> lspaletteflagsnamemap = {
	{ "Enabled",             0x01 },
	{ "UseLSLightDirection", 0x02 },
	{ "IgnoreDirection",     0x04 },
	{ "OverrideLastLight",   0x08 },
	{ "Unknown",             0x10 },
	{ "Unknown20",           0x20 },
	{ "Unknown20",           0x40 },
	{ "Unknown80",           0x80 },
};

static uint8_t ParseLSPaletteFlags(const string& str)
{
	vector<string> strflags = split(str, ',');
	uint8_t flag = 0;
	for (const auto& strflag : strflags)
	{
		string s = trim(strflag);
		auto ch = lspaletteflagsnamemap.find(s);

		if (ch != lspaletteflagsnamemap.end())
		{
			flag |= ch->second;
		}
	}
	return flag;
}

static const unordered_map<string, uint8_t> lslighttypesnamemap = {
	{ "Character",    0 },
	{ "CharacterAlt", 6 },
	{ "Boss",         8 },
};

static uint8_t ParseLSPaletteTypes(const string & str)
{
	string str2 = trim(str);
	auto lv = lslighttypesnamemap.find(str2);
	return lv != lslighttypesnamemap.end() ? lv->second : (uint8_t)strtol(str.c_str(), nullptr, 10);
}

static const unordered_map<string, uint8_t> languagesnamemap = {
	{ "japanese", Languages_Japanese },
	{ "english",  Languages_English },
	{ "french",   Languages_French },
	{ "spanish",  Languages_Spanish },
	{ "german",   Languages_German }
};

static uint8_t ParseLanguage(const string& str)
{
	string str2 = trim(str);
	transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
	auto lv = languagesnamemap.find(str2);

	if (lv != languagesnamemap.end())
	{
		return lv->second;
	}

	return Languages_Japanese;
}

static UINT GetCodepageForLanguageID(int language)
{
	switch (language)
	{
	case Languages_Japanese:
	default:
		return CodepageJapanese;
	case Languages_English:
		return CodepageEnglish;
	case Languages_French:
		return CodepageFrench;
	case Languages_German:
		return CodepageGerman;
	case Languages_Spanish:
		return CodepageSpanish;
	}
}

static string DecodeUTF8(const string& str, int language, unsigned int codepage)
{
	// If there is a non-global codepage override, use the old parsing
	if (codepage != 0)
		return (language <= Languages_English) ? UTF8toSJIS(str) : UTF8toCodepage(str, codepage);
	// If there is no override, use the global codepage value for the specified language
	return UTF8toCodepage(str, GetCodepageForLanguageID(language));
}

static string UnescapeNewlines(const string& str)
{
	string result;
	result.reserve(str.size());

	for (size_t c = 0; c < str.size(); c++)
	{
		switch (str[c])
		{
			case '\\': // escape character
				if (c + 1 == str.size())
				{
					result.push_back(str[c]);
					continue;
				}
				switch (str[++c])
				{
					case 'n': // line feed
						result.push_back('\n');
						break;
					case 'r': // carriage return
						result.push_back('\r');
						break;
					default: // literal character
						result.push_back(str[c]);
						break;
				}
				break;
			default:
				result.push_back(str[c]);
				break;
		}
	}

	return result;
}

static void ParseVertex(const string& str, NJS_VECTOR& vert)
{
	vector<string> vals = split(str, ',');
	assert(vals.size() == 3);

	if (vals.size() < 3)
	{
		return;
	}

	vert.x = static_cast<float>(strtod(vals[0].c_str(), nullptr));
	vert.y = static_cast<float>(strtod(vals[1].c_str(), nullptr));
	vert.z = static_cast<float>(strtod(vals[2].c_str(), nullptr));
}

static void ParseRGB(const string& str, NJS_RGB& vert)
{
	vector<string> vals = split(str, ',');
	assert(vals.size() == 3);

	if (vals.size() < 3)
	{
		return;
	}

	vert.r = static_cast<float>(strtod(vals[0].c_str(), nullptr));
	vert.g = static_cast<float>(strtod(vals[1].c_str(), nullptr));
	vert.b = static_cast<float>(strtod(vals[2].c_str(), nullptr));
}

static void ParseRotation(const string str, Rotation& rot)
{
	vector<string> vals = split(str, ',');
	assert(vals.size() == 3);

	if (vals.size() < 3)
	{
		return;
	}

	rot.x = (int)strtol(vals[0].c_str(), nullptr, 16);
	rot.y = (int)strtol(vals[1].c_str(), nullptr, 16);
	rot.z = (int)strtol(vals[2].c_str(), nullptr, 16);
}

template <typename T>
static void ProcessPointerList(const string& list, T* item)
{
	vector<string> ptrs = split(list, ',');

	for (auto& ptr : ptrs)
	{
		WriteData((T**)(strtol(ptr.c_str(), nullptr, 16) + 0x400000), item);
	}
}

static void ProcessLandTableINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto* const landtableinfo  = new LandTableInfo(filename);
	LandTable* const landtable = landtableinfo->getlandtable();
	if (group->hasKeyNonEmpty("pointer"))
		ProcessPointerList(group->getString("pointer"), landtable);
	else
		*(LandTable*)(strtol((group->getString("address")).c_str(), nullptr, 16) + 0x400000) = *landtable;
}

static unordered_map<string, NJS_OBJECT*> inimodels;

static void GetModelLabels(ModelInfo* mdlinf, NJS_OBJECT* obj)
{
	while (obj)
	{
		inimodels[mdlinf->getlabel(obj)] = obj;

		if (obj->child)
		{
			GetModelLabels(mdlinf, obj->child);
		}

		obj = obj->sibling;
	}
}

static void ProcessModelINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto* const mdlinf = new ModelInfo(filename);
	NJS_OBJECT* newobject  = mdlinf->getmodel();

	GetModelLabels(mdlinf, newobject);
	if (group->hasKeyNonEmpty("pointer"))
		ProcessPointerList(group->getString("pointer"), newobject);
	else
	{
		NJS_OBJECT* object = (NJS_OBJECT*)(strtol((group->getString("address")).c_str(), nullptr, 16) + 0x400000);
		if (object->basicdxmodel != nullptr)
		{
			*object->basicdxmodel = *newobject->basicdxmodel;
		}
		*object = *newobject;
	}
}

static void ProcessActionINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto* action              = new NJS_ACTION;
	auto* const animationFile = new AnimationFile(filename);

	action->motion = animationFile->getmotion();
	action->object = inimodels.find(group->getString("model"))->second;

	if (group->hasKeyNonEmpty("pointer"))
		ProcessPointerList(group->getString("pointer"), action);
	else
		*(NJS_ACTION*)(strtol((group->getString("address")).c_str(), nullptr, 16) + 0x400000) = *action;
}

static void ProcessAnimationINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto* const animationFile = new AnimationFile(filename);
	NJS_MOTION* animation     = animationFile->getmotion();
	if (group->hasKeyNonEmpty("pointer"))
		ProcessPointerList(group->getString("pointer"), animation);
	else
		*(NJS_MOTION*)(strtol((group->getString("address")).c_str(), nullptr, 16) + 0x400000) = *animation;
}

static void ProcessObjListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto objlistdata = new IniFile(filename);
	vector<ObjectListEntry> objs;

	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!objlistdata->hasGroup(key))
		{
			break;
		}

		const IniGroup* objdata = objlistdata->getGroup(key);
		ObjectListEntry entry {};

		// TODO: Make these ini fields match structure names
		entry.Flags           = objdata->getInt("Arg1");
		entry.ObjectListIndex = objdata->getInt("Arg2");
		entry.UseDistance     = objdata->getInt("Flags");
		entry.Distance        = objdata->getFloat("Distance");
		entry.LoadSub         = (ObjectFuncPtr)objdata->getIntRadix("Code", 16);
		entry.Name            = UTF8toSJIS(objdata->getString("Name").c_str());

		objs.push_back(entry);
	}
	delete objlistdata;
	auto* list  = new ObjectList;
	list->Count = objs.size();
	list->List  = new ObjectListEntry[list->Count];
	arrcpy(list->List, objs.data(), list->Count);
	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessStartPosINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto startposdata = new IniFile(filename);
	vector<StartPosition> poss;

	for (const auto& iter : *startposdata)
	{
		if (iter.first.empty())
		{
			continue;
		}

		StartPosition pos;
		uint16_t levelact = ParseLevelAndActID(iter.first);

		pos.LevelID = (int16_t)(levelact >> 8);
		pos.ActID   = (int16_t)(levelact & 0xFF);

		ParseVertex(iter.second->getString("Position", "0,0,0"), pos.Position);

		pos.YRot = iter.second->getIntRadix("YRotation", 16);

		poss.push_back(pos);
	}

	delete startposdata;
	size_t numents = poss.size();

	auto* list = new StartPosition[numents + 1];
	arrcpy(list, poss.data(), numents);
	clrmem(&list[numents]);
	list[numents].LevelID = LevelIDs_Invalid;
	ProcessPointerList(group->getString("pointer"), list);
}

static vector<PVMEntry> ProcessTexListINI_Internal(const IniFile* texlistdata)
{
	vector<PVMEntry> texs;
	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!texlistdata->hasGroup(key))
		{
			break;
		}

		const IniGroup* pvmdata = texlistdata->getGroup(key);
		PVMEntry entry {};

		if (pvmdata->hasKeyNonEmpty("Name"))
		{
			entry.Name = strdup(pvmdata->getString("Name").c_str());
		}
		else
		{
			entry.Name = nullptr;
		}

		entry.TexList = (NJS_TEXLIST*)pvmdata->getIntRadix("Textures", 16);
		texs.push_back(entry);
	}
	return texs;
}

static void ProcessTexListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto texlistdata = new IniFile(filename);
	vector<PVMEntry> texs = ProcessTexListINI_Internal(texlistdata);

	delete texlistdata;

	size_t numents = texs.size();
	auto* list = new PVMEntry[numents + 1];

	arrcpy(list, texs.data(), numents);
	clrmem(&list[numents]);

	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessLevelTexListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto texlistdata = new IniFile(filename);
	vector<PVMEntry> texs = ProcessTexListINI_Internal(texlistdata);

	size_t numents = texs.size();
	auto* list = new PVMEntry[numents];

	arrcpy(list, texs.data(), numents);

	auto* lvl = new LevelPVMList;
	lvl->Level = (int16_t)ParseLevelAndActID(texlistdata->getString("", "Level", "0000"));

	delete texlistdata;

	lvl->NumTextures = (int16_t)numents;
	lvl->PVMList = list;

	ProcessPointerList(group->getString("pointer"), lvl);
}

static void ProcessTrialLevelListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	ifstream fstr(mod_dir + L'\\' + group->getWString("filename"));
	vector<TrialLevelListEntry> lvls;

	while (fstr.good())
	{
		string str;
		getline(fstr, str);
		if (!str.empty())
		{
			TrialLevelListEntry ent {};
			uint16_t lvl = ParseLevelAndActID(str);

			ent.Level = (char)(lvl >> 8);
			ent.Act   = (char)lvl;

			lvls.push_back(ent);
		}
	}

	fstr.close();

	size_t numents = lvls.size();

	auto list = (TrialLevelList*)(group->getIntRadix("address", 16) + 0x400000);

	list->Levels = new TrialLevelListEntry[numents];
	arrcpy(list->Levels, lvls.data(), numents);
	list->Count = (int)numents;
}

static void ProcessBossLevelListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	ifstream fstr(mod_dir + L'\\' + group->getWString("filename"));
	vector<uint16_t> lvls;

	while (fstr.good())
	{
		string str;
		getline(fstr, str);

		if (!str.empty())
		{
			lvls.push_back(ParseLevelAndActID(str));
		}
	}

	fstr.close();

	size_t numents = lvls.size();

	auto* list = new uint16_t[numents + 1];
	arrcpy(list, lvls.data(), numents);
	list[numents] = levelact(LevelIDs_Invalid, 0);

	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessFieldStartPosINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto startposdata = new IniFile(filename);
	vector<FieldStartPosition> poss;

	for (const auto& iter : *startposdata)
	{
		if (iter.first.empty())
		{
			continue;
		}

		FieldStartPosition pos {};

		pos.LevelID = ParseLevelID(iter.first);
		pos.FieldID = ParseLevelID(iter.second->getString("Field", "Invalid"));

		ParseVertex(iter.second->getString("Position", "0,0,0"), pos.Position);
		pos.YRot = iter.second->getIntRadix("YRotation", 16);

		poss.push_back(pos);
	}

	delete startposdata;

	size_t numents = poss.size();
	auto* list = new FieldStartPosition[numents + 1];

	arrcpy(list, poss.data(), numents);
	clrmem(&list[numents]);
	
	list[numents].LevelID = LevelIDs_Invalid;

	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessSoundTestListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<SoundTestEntry> sounds;

	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!inidata->hasGroup(key))
		{
			break;
		}

		const IniGroup* snddata = inidata->getGroup(key);
		SoundTestEntry entry {};
		entry.Name = UTF8toSJIS(snddata->getString("Title").c_str());
		entry.ID   = snddata->getInt("Track");
		sounds.push_back(entry);
	}

	delete inidata;

	size_t numents = sounds.size();
	auto cat = (SoundTestCategory*)(group->getIntRadix("address", 16) + 0x400000);;

	cat->Entries = new SoundTestEntry[numents];
	arrcpy(cat->Entries, sounds.data(), numents);
	cat->Count = (int)numents;
}

static void ProcessMusicListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<MusicInfo> songs;

	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!inidata->hasGroup(key))
		{
			break;
		}

		const IniGroup* musdata = inidata->getGroup(key);
		MusicInfo entry {};
		entry.Name = strdup(musdata->getString("Filename").c_str());
		entry.Loop = (int)musdata->getBool("Loop");
		songs.push_back(entry);
	}

	delete inidata;
	size_t numents = songs.size();
	arrcpy((MusicInfo*)(group->getIntRadix("address", 16) + 0x400000), songs.data(), numents);
}

static void ProcessSoundListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<SoundFileInfo> sounds;

	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);
		if (!inidata->hasGroup(key))
			break;
		const IniGroup* snddata = inidata->getGroup(key);
		SoundFileInfo entry {};
		entry.Bank     = snddata->getInt("Bank");
		entry.Filename = strdup(snddata->getString("Filename").c_str());
		sounds.push_back(entry);
	}
	delete inidata;
	size_t numents = sounds.size();
	SoundList* list = (SoundList*)(group->getIntRadix("address", 16) + 0x400000);;
	list->List      = new SoundFileInfo[numents];
	arrcpy(list->List, sounds.data(), numents);
	list->Count = (int)numents;
}

static vector<char*> ProcessStringArrayINI_Internal(const wchar_t* filename, uint8_t language, unsigned int codepage)
{
	ifstream fstr(filename);
	vector<char*> strs;
	while (fstr.good())
	{
		string str;
		getline(fstr, str);
		str = DecodeUTF8(UnescapeNewlines(str), language, codepage);
		if (strlen(str.c_str()) == 0)
			strs.push_back(NULL);
		else
			strs.push_back(strdup(str.c_str()));
	}
	fstr.close();
	return strs;
}

static void ProcessStringArrayINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("address"))
	{
		return;
	}

	wchar_t filename[MAX_PATH]{};
	unsigned int codepage = group->getInt("codepage", 0);

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	vector<char*> strs = ProcessStringArrayINI_Internal(filename,
	                                                    ParseLanguage(group->getString("language")), codepage);
	size_t numents = strs.size();
	char** list  = (char**)(group->getIntRadix("address", 16) + 0x400000);;
	arrcpy(list, strs.data(), numents);
}

static void ProcessNextLevelListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<NextLevelData> ents;
	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);
		if (!inidata->hasGroup(key))
			break;
		const IniGroup* entdata = inidata->getGroup(key);
		NextLevelData entry {};
		entry.CGMovie             = (char)entdata->getInt("CGMovie");
		entry.CurrentLevel        = (char)ParseLevelID(entdata->getString("Level"));
		entry.NextLevelAdventure  = (char)ParseLevelID(entdata->getString("NextLevel"));
		entry.NextActAdventure    = (char)entdata->getInt("NextAct");
		entry.StartPointAdventure = (char)entdata->getInt("StartPos");
		entry.AltNextLevel        = (char)ParseLevelID(entdata->getString("AltNextLevel"));
		entry.AltNextAct          = (char)entdata->getInt("AltNextAct");
		entry.AltStartPoint       = (char)entdata->getInt("AltStartPos");
		ents.push_back(entry);
	}
	delete inidata;
	size_t numents = ents.size();
	auto* list   = new NextLevelData[numents + 1];
	arrcpy(list, ents.data(), numents);
	clrmem(&list[numents]);
	list[numents].CurrentLevel = -1;
	ProcessPointerList(group->getString("pointer"), list);
}

static const wchar_t* const languagenames[] = { L"Japanese", L"English", L"French", L"Spanish", L"German" };

static void ProcessCutsceneTextINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
		return;
	unsigned int codepage = group->getInt("codepage", 0);
	char*** addr = (char***)group->getIntRadix("address", 16);
	if (addr == nullptr)
		return;
	addr = (char***)((int)addr + 0x400000);
	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename") + L'\\';
	for (unsigned int i = 0; i < LengthOfArray(languagenames); i++)
	{
		wchar_t filename[MAX_PATH] {};

		swprintf(filename, LengthOfArray(filename), L"%s%s.txt",
		         pathbase.c_str(), languagenames[i]);

		vector<char*> strs = ProcessStringArrayINI_Internal(filename, i, codepage);

		size_t numents = strs.size();
		char** list = new char* [numents];
		arrcpy(list, strs.data(), numents);
		*addr++ = list;
	}
}

static void ProcessRecapScreenINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	unsigned int codepage = group->getInt("codepage", 0);
	int length = group->getInt("length");
	auto addr = (RecapScreen**)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (RecapScreen**)((intptr_t)addr + 0x400000);
	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename") + L'\\';

	for (size_t l = 0; l < LengthOfArray(languagenames); l++)
	{
		auto* list = new RecapScreen[length];
		for (int i = 0; i < length; i++)
		{
			wchar_t filename[MAX_PATH] {};

			swprintf(filename, LengthOfArray(filename), L"%s%d\\%s.ini",
			         pathbase.c_str(), i + 1, languagenames[l]);

			auto inidata = new IniFile(filename);

			vector<string> strs = split(inidata->getString("", "Text"), '\n');
			const size_t numents = strs.size();
			list[i].TextData = new const char* [numents];

			for (size_t j = 0; j < numents; j++)
			{
				list[i].TextData[j] = strdup(DecodeUTF8(strs[j], l, codepage).c_str());
			}

			list[i].LineCount = (int)numents;
			list[i].Speed     = inidata->getFloat("", "Speed", 1);

			delete inidata;
		}
		*addr++ = list;
	}
}

static void ProcessNPCTextINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}
	unsigned int codepage = group->getInt("codepage", 0);
	int length = group->getInt("length");
	auto addr = (HintText_Entry**)group->getIntRadix("address", 16);
	
	if (addr == nullptr)
	{
		return;
	}

	addr = (HintText_Entry**)((intptr_t)addr + 0x400000);
	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename") + L'\\';

	for (size_t l = 0; l < LengthOfArray(languagenames); l++)
	{
		auto* list = new HintText_Entry[length];
		for (int i = 0; i < length; i++)
		{
			wchar_t filename[MAX_PATH] {};

			swprintf(filename, LengthOfArray(filename), L"%s%d\\%s.ini",
			         pathbase.c_str(), i + 1, languagenames[l]);

			auto inidata = new IniFile(filename);
			vector<int16_t> props;
			vector<HintText_Text> text;

			for (unsigned int j = 0; j < 999; j++)
			{
				char buf[8] {};

				snprintf(buf, sizeof(buf), "%u", j);
				
				if (!inidata->hasGroup(buf))
				{
					break;
				}
				
				if (!props.empty())
				{
					props.push_back(NPCTextControl_NewGroup);
				}

				const IniGroup* entdata = inidata->getGroup(buf);

				if (entdata->hasKeyNonEmpty("EventFlags"))
				{
					vector<string> strs = split(entdata->getString("EventFlags"), ',');
					for (auto& str : strs)
					{
						props.push_back(NPCTextControl_EventFlag);
						props.push_back((int16_t)strtol(str.c_str(), nullptr, 10));
					}
				}

				if (entdata->hasKeyNonEmpty("NPCFlags"))
				{
					vector<string> strs = split(entdata->getString("NPCFlags"), ',');
					for (auto& str : strs)
					{
						props.push_back(NPCTextControl_NPCFlag);
						props.push_back((int16_t)strtol(str.c_str(), nullptr, 10));
					}
				}

				if (entdata->hasKeyNonEmpty("Character"))
				{
					props.push_back(NPCTextControl_Character);
					props.push_back((int16_t)ParseCharacterFlags(entdata->getString("Character")));
				}

				if (entdata->hasKeyNonEmpty("Voice"))
				{
					props.push_back(NPCTextControl_Voice);
					props.push_back((int16_t)entdata->getInt("Voice"));
				}

				if (entdata->hasKeyNonEmpty("SetEventFlag"))
				{
					props.push_back(NPCTextControl_SetEventFlag);
					props.push_back((int16_t)entdata->getInt("SetEventFlag"));
				}

				string linekey = buf;
				linekey += ".Lines[%u]";

				char* buf2   = new char[linekey.size() + 2];
				bool hasText = false;

				for (unsigned int k = 0; k < 999; k++)
				{
					snprintf(buf2, linekey.size() + 2, linekey.c_str(), k);
					if (!inidata->hasGroup(buf2))
						break;
					hasText = true;
					entdata = inidata->getGroup(buf2);
					HintText_Text entry {};
					entry.Message = strdup(DecodeUTF8(entdata->getString("Line"), l, codepage).c_str());
					entry.Time    = entdata->getInt("Time");
					text.push_back(entry);
				}

				delete[] buf2;

				if (hasText)
				{
					HintText_Text t = {};
					text.push_back(t);
				}
			}

			delete inidata;

			if (!props.empty())
			{
				props.push_back(NPCTextControl_End);
				list[i].Properties = new int16_t[props.size()];
				arrcpy(list[i].Properties, props.data(), props.size());
			}
			else
			{
				list[i].Properties = nullptr;
			}

			if (!text.empty())
			{
				list[i].Text = new HintText_Text[text.size()];
				arrcpy(list[i].Text, text.data(), text.size());
			}
			else
			{
				list[i].Text = nullptr;
			}
		}
		*addr++ = list;
	}
}

static void ProcessLevelClearFlagListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	ifstream fstr(filename);
	vector<LevelClearFlagData> lvls;

	while (fstr.good())
	{
		string str;
		getline(fstr, str);
		if (!str.empty())
		{
			LevelClearFlagData ent {};
			vector<string> parts = split(str, ' ');
			ent.Level = ParseLevelID(parts[0]);
			ent.FlagOffset = (int16_t)strtol(parts[1].c_str(), nullptr, 16);
			lvls.push_back(ent);
		}
	}

	fstr.close();
	size_t numents = lvls.size();

	auto list = new LevelClearFlagData[numents + 1];
	arrcpy(list, lvls.data(), numents);
	list[numents].Level = -1;

	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessDeathZoneINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t dzinipath[MAX_PATH] {};

	swprintf(dzinipath, LengthOfArray(dzinipath), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto dzdata = new IniFile(dzinipath);

	// Remove the filename portion of the path.
	// NOTE: This might be a lower directory than mod_dir,
	// since the filename can have subdirectories.
	wstring str = GetDirectory(dzinipath);
	wcscpy(dzinipath, str.c_str());

	vector<DeathZone> deathzones;
	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!dzdata->hasGroup(key))
		{
			break;
		}

		uint8_t flag = ParseCharacterFlags(dzdata->getString(key, "Flags"));

		wchar_t dzpath[MAX_PATH];
		if (dzdata->hasKey(key, "Filename"))
			swprintf(dzpath, LengthOfArray(dzpath), L"%s\\%s", dzinipath, dzdata->getWString(key, "Filename").c_str()); // .sa1mdl part added already
		else
			swprintf(dzpath, LengthOfArray(dzpath), L"%s\\%u.sa1mdl", dzinipath, i);
		auto* dzmdl = new ModelInfo(dzpath);
		DeathZone dz = { flag, dzmdl->getmodel() };
		deathzones.push_back(dz);
		// NOTE: dzmdl is not deleted because NJS_OBJECT
		// points to allocated memory in the ModelInfo.
	}

	auto* newlist = new DeathZone[deathzones.size() + 1];
	arrcpy(newlist, deathzones.data(), deathzones.size());
	clrmem(&newlist[deathzones.size()]);

	ProcessPointerList(group->getString("pointer"), newlist);
}

static void ProcessSkyboxScaleINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	int count = group->getInt("count");
	auto addr = (SkyboxScale**)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (SkyboxScale**)((int)addr + 0x400000);

	wchar_t filename[MAX_PATH] {};
	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);

	for (int i = 0; i < count; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%d", i);

		if (!inidata->hasGroup(key))
		{
			*addr++ = nullptr;
			continue;
		}

		const IniGroup* const entdata = inidata->getGroup(key);
		auto* entry = new SkyboxScale;
		ParseVertex(entdata->getString("Far", "1,1,1"), entry->Far);
		ParseVertex(entdata->getString("Normal", "1,1,1"), entry->Normal);
		ParseVertex(entdata->getString("Near", "1,1,1"), entry->Near);
		*addr++ = entry;
	}

	delete inidata;
}

static void ProcessLevelPathListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t inipath[MAX_PATH] {};

	swprintf(inipath, LengthOfArray(inipath), L"%s\\%s\\",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	vector<PathDataPtr> pathlist;
	WIN32_FIND_DATA data;

	HANDLE hFind = FindFirstFileEx(inipath, FindExInfoStandard, &data, FindExSearchLimitToDirectories, nullptr, 0);

	if (hFind == INVALID_HANDLE_VALUE)
	{
		return;
	}

	do
	{
		if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
		{
			continue;
		}

		uint16_t levelact;

		try
		{
			levelact = ParseLevelAndActID(UTF16toMBS(data.cFileName, CP_UTF8));
		}
		catch (...)
		{
			continue;
		}

		vector<LoopHead*> paths;
		for (unsigned int i = 0; i < 999; i++)
		{
			wchar_t levelpath[MAX_PATH] {};
			swprintf(levelpath, LengthOfArray(levelpath), L"%s\\%s\\%u.ini",
			         inipath, data.cFileName, i);

			if (!Exists(levelpath))
			{
				break;
			}

			auto inidata = new IniFile(levelpath);
			const IniGroup* entdata;
			vector<Loop> points;
			char buf2[8] {};

			for (unsigned int j = 0; j < 999; j++)
			{
				snprintf(buf2, LengthOfArray(buf2), "%u", j);
				
				if (!inidata->hasGroup(buf2))
				{
					break;
				}

				entdata = inidata->getGroup(buf2);
				Loop point {};
				point.Ang_X = (int16_t)entdata->getIntRadix("XRotation", 16);
				point.Ang_Y = (int16_t)entdata->getIntRadix("YRotation", 16);
				point.Dist  = entdata->getFloat("Distance");
				ParseVertex(entdata->getString("Position", "0,0,0"), point.Position);
				points.push_back(point);
			}

			entdata = inidata->getGroup("");

			auto* path = new LoopHead;

			path->Unknown_0 = (int16_t)entdata->getInt("Unknown");
			path->Count     = (int16_t)points.size();
			path->TotalDist = entdata->getFloat("TotalDistance");
			path->LoopList  = new Loop[path->Count];

			arrcpy(path->LoopList, points.data(), path->Count);

			path->Object = (ObjectFuncPtr)entdata->getIntRadix("Code", 16);
			paths.push_back(path);

			delete inidata;
		}

		size_t numents = paths.size();

		PathDataPtr ptr {};

		ptr.LevelAct = levelact;
		ptr.PathList = new LoopHead* [numents + 1];

		arrcpy(ptr.PathList, paths.data(), numents);
		ptr.PathList[numents] = nullptr;

		pathlist.push_back(ptr);
	} while (FindNextFile(hFind, &data));

	FindClose(hFind);

	auto* newlist = new PathDataPtr[pathlist.size() + 1];
	arrcpy(newlist, pathlist.data(), pathlist.size());
	clrmem(&newlist[pathlist.size()]);
	newlist[pathlist.size()].LevelAct = -1;

	ProcessPointerList(group->getString("pointer"), newlist);
}

static void ProcessStageLightDataListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<StageLightData> ents;

	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!inidata->hasGroup(key))
		{
			break;
		}

		const IniGroup* const entdata = inidata->getGroup(key);
		StageLightData entry {};

		entry.level         = (char)ParseLevelID(entdata->getString("Level"));
		entry.act           = (char)entdata->getInt("Act");
		entry.index         = (char)entdata->getInt("LightNum");
		entry.use_direction = (char)entdata->getBool("UseDirection");

		ParseVertex(entdata->getString("Direction"), entry.direction);

		// FIXME: all these INI strings should match the light field names
		entry.specular   = entdata->getFloat("Dif");
		entry.multiplier = entdata->getFloat("Multiplier");

		ParseVertex(entdata->getString("RGB"), *(NJS_VECTOR*)entry.diffuse);
		ParseVertex(entdata->getString("AmbientRGB"), *(NJS_VECTOR*)entry.ambient);

		ents.push_back(entry);
	}

	delete inidata;

	size_t numents = ents.size();

	auto* list = new StageLightData[numents + 1];
	arrcpy(list, ents.data(), numents);
	clrmem(&list[numents]);
	list[numents].level = -1;

	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessPaletteLightListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	wchar_t filename[MAX_PATH]{};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
		mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);

	for (unsigned int i = 0; i < 255; i++)
	{
		char key[8]{};
		snprintf(key, sizeof(key), "%u", i);

		if (!inidata->hasGroup(key))
		{
			break;
		}

		const IniGroup* const entdata = inidata->getGroup(key);

		le_plyrPal[i].stgNo = (char)ParseLevelID(entdata->getString("Level"));
		le_plyrPal[i].actNo = (char)entdata->getInt("Act");
		le_plyrPal[i].chrNo = (char)ParseLSPaletteTypes(entdata->getString("Type"));
		le_plyrPal[i].flgs = (char)ParseLSPaletteFlags(entdata->getString("Flags"));
		ParseVertex(entdata->getString("Direction"), le_plyrPal[i].vec);		
		le_plyrPal[i].dif = entdata->getFloat("Diffuse");
		ParseRGB(entdata->getString("Ambient"), le_plyrPal[i].amb);
		le_plyrPal[i].cpow = entdata->getFloat("Color1Power");
		ParseRGB(entdata->getString("Color1"), le_plyrPal[i].co);
		le_plyrPal[i].spow = entdata->getFloat("Specular1Power");
		ParseRGB(entdata->getString("Specular1"), le_plyrPal[i].spe);
		le_plyrPal[i].cpow2 = entdata->getFloat("Color2Power");
		ParseRGB(entdata->getString("Color2"), le_plyrPal[i].co2);
		le_plyrPal[i].spow2 = entdata->getFloat("Specular2Power");
		ParseRGB(entdata->getString("Specular2"), le_plyrPal[i].spe2);

	}

	delete inidata;

}

static void ProcessFogDataINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	int count = group->getInt("count");
	auto addr = (FogData**)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (FogData**)((int)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};
	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
		mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);

	vector<FogData> ents;

	for (int i = 0; i < count; i++)
	{
		char key[8]{};
		snprintf(key, sizeof(key), "%d", i);

		if (!inidata->hasGroup(key))
		{
			*addr++ = nullptr;
			continue;
		}

		const IniGroup* const entdata = inidata->getGroup(key);
		auto* entry = new FogData[3];
		// Far
		entry[0].Toggle = entdata->getInt("FogEnabledHigh");
		entry[0].Layer = entdata->getFloat("FogStartHigh");
		entry[0].Distance = entdata->getFloat("FogEndHigh");
		entry[0].Color = (((entdata->getInt("ColorA_High") & 0x0ff) << 24 ) | (entdata->getInt("ColorR_High") & 0x0ff) << 16) | ((entdata->getInt("ColorG_High") & 0x0ff) << 8) | (entdata->getInt("ColorB_High") & 0x0ff);
		// Medium
		entry[1].Toggle = entdata->getInt("FogEnabledMedium");
		entry[1].Layer = entdata->getFloat("FogStartMedium");
		entry[1].Distance = entdata->getFloat("FogEndMedium");
		entry[1].Color = (((entdata->getInt("ColorA_Medium") & 0x0ff) << 24) | (entdata->getInt("ColorR_Medium") & 0x0ff) << 16) | ((entdata->getInt("ColorG_Medium") & 0x0ff) << 8) | (entdata->getInt("ColorB_Medium") & 0x0ff);
		// Near
		entry[2].Toggle = entdata->getInt("FogEnabledLow");
		entry[2].Layer = entdata->getFloat("FogStartLow");
		entry[2].Distance = entdata->getFloat("FogEndLow");
		entry[2].Color = (((entdata->getInt("ColorA_Low") & 0x0ff) << 24) | (entdata->getInt("ColorR_Low") & 0x0ff) << 16) | ((entdata->getInt("ColorG_Low") & 0x0ff) << 8) | (entdata->getInt("ColorB_Low") & 0x0ff);
		*addr++ = entry;
	}

	delete inidata;
}

static void ProcessWeldListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename") || !group->hasKeyNonEmpty("pointer"))
	{
		return;
	}

	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<WeldInfo> ents;

	for (size_t i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!inidata->hasGroup(key))
		{
			break;
		}

		auto entdata = inidata->getGroup(key);
		uint16_t* vilist = nullptr;
		WeldInfo entry {};

		entry.BaseModel = inimodels[trim(entdata->getString("BaseModel"))];
		entry.ModelA    = inimodels[trim(entdata->getString("ModelA"))];
		entry.ModelB    = inimodels[trim(entdata->getString("ModelB"))];

		if (entdata->hasKeyNonEmpty("VertIndexes"))
		{
			vector<string> strs = split(entdata->getString("VertIndexes"), ',');
			vilist = new uint16_t[strs.size()];

			for (size_t j = 0; j < strs.size(); j++)
			{
				vilist[j] = (int16_t)strtol(strs[j].c_str(), nullptr, 10);
			}

			entry.VertexPairCount = static_cast<char>(strs.size() / 2);
		}
		else
		{
			entry.VertexPairCount = 0;
		}

		entry.WeldType     = (uint8_t)entdata->getInt("WeldType");
		entry.anonymous_5  = (int16_t)entdata->getInt("Unknown");
		entry.VertexBuffer = nullptr;
		entry.VertIndexes  = vilist;
		ents.push_back(entry);
	}

	delete inidata;

	size_t numents = ents.size();

	auto* list   = new WeldInfo[numents + 1];
	arrcpy(list, ents.data(), numents);
	clrmem(&list[numents]);

	ProcessPointerList(group->getString("pointer"), list);
}

static void ProcessCreditsTextListINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (CreditsList*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (CreditsList*)((intptr_t)addr + 0x400000);
	unsigned int codepage = group->getInt("codepage", 0);
	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	vector<CreditsEntry> ents;

	for (unsigned int i = 0; i < 999; i++)
	{
		char key[8] {};
		snprintf(key, sizeof(key), "%u", i);

		if (!inidata->hasGroup(key))
		{
			break;
		}

		const IniGroup* const entdata = inidata->getGroup(key);
		CreditsEntry entry {};

		entry.SomeIndex = entdata->getInt("Type");
		entry.field_1   = entdata->getInt("TexID", -1);
		entry.field_2   = entdata->getInt("Unknown1");
		entry.field_3   = entdata->getInt("Unknown2");
		entry.field_3   = entdata->getInt("Unknown2");
		entry.Line      = strdup(DecodeUTF8(entdata->getString("Text"), 0, codepage).c_str());

		ents.push_back(entry);
	}

	delete inidata;
	size_t numents = ents.size();

	addr->Entries = new CreditsEntry[numents];
	addr->Count   = numents;

	arrcpy(addr->Entries, ents.data(), numents);
}

static void ProcessPhysicsDataINI(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (player_parameter*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (player_parameter*)((intptr_t)addr + 0x400000);
	wchar_t filename[MAX_PATH] {};

	swprintf(filename, LengthOfArray(filename), L"%s\\%s",
	         mod_dir.c_str(), group->getWString("filename").c_str());

	auto inidata = new IniFile(filename);
	addr->jump2_timer = inidata->getInt("", "jump2_timer");
	addr->pos_error = inidata->getFloat("", "pos_error");
	addr->lim_h_spd = inidata->getFloat("", "lim_h_spd");
	addr->lim_v_spd = inidata->getFloat("", "lim_v_spd");
	addr->max_x_spd = inidata->getFloat("", "max_x_spd");
	addr->max_psh_spd = inidata->getFloat("", "max_psh_spd");
	addr->jmp_y_spd = inidata->getFloat("", "jmp_y_spd");
	addr->nocon_speed = inidata->getFloat("", "nocon_speed");
	addr->slide_speed = inidata->getFloat("", "slide_speed");
	addr->jog_speed = inidata->getFloat("", "jog_speed");
	addr->run_speed = inidata->getFloat("", "run_speed");
	addr->rush_speed = inidata->getFloat("", "rush_speed");
	addr->crash_speed = inidata->getFloat("", "crash_speed");
	addr->dash_speed = inidata->getFloat("", "dash_speed");
	addr->jmp_addit = inidata->getFloat("", "jmp_addit");
	addr->run_accel = inidata->getFloat("", "run_accel");
	addr->air_accel = inidata->getFloat("", "air_accel");
	addr->slow_down = inidata->getFloat("", "slow_down");
	addr->run_break = inidata->getFloat("", "run_break");
	addr->air_break = inidata->getFloat("", "air_break");
	addr->air_resist_air = inidata->getFloat("", "air_resist_air");
	addr->air_resist = inidata->getFloat("", "air_resist");
	addr->air_resist_y = inidata->getFloat("", "air_resist_y");
	addr->air_resist_z = inidata->getFloat("", "air_resist_z");
	addr->grd_frict = inidata->getFloat("", "grd_frict");
	addr->grd_frict_z = inidata->getFloat("", "grd_frict_z");
	addr->lim_frict = inidata->getFloat("", "lim_frict");
	addr->rat_bound = inidata->getFloat("", "rat_bound");
	addr->rad = inidata->getFloat("", "rad");
	addr->height = inidata->getFloat("", "height");
	addr->weight = inidata->getFloat("", "weight");
	addr->eyes_height = inidata->getFloat("", "eyes_height");
	addr->center_height = inidata->getFloat("", "center_height");
	delete inidata;
}

static void ProcessSingleString(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	if (!group->hasKeyNonEmpty("language"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	int length = group->getInt("length", 1);

	if (addr == nullptr)
	{
		return;
	}

	addr = (char*)((intptr_t)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename") + L'\\';

	wchar_t language[32]{};

	swprintf(language, LengthOfArray(language), group->getWString("language").c_str());

	int langid = Languages_Japanese;

	for (int i = 0; i < LengthOfArray(languagenames); i++)
	{
		if (!wcscmp(language, languagenames[i]))
		{
			langid = i;
			break;
		}
	}

	swprintf(filename, LengthOfArray(filename), L"%s\\%s.txt",
		pathbase.c_str(), language);

	vector<char*> strs = ProcessStringArrayINI_Internal(filename, langid, 0);

	for (int line = 0; line < length; line++)
	{
		WriteData((char**)(addr + 4 * line), strs[line]);
	}
}

static void ProcessFixedStringArray(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	int length = group->getInt("length", 0);

	int count = group->getInt("count", 0);

	if (addr == nullptr || length <= 0 || count <= 0)
	{
		return;
	}
	
	addr = (char*)((intptr_t)addr + 0x400000);

	int lang = ParseLanguage(group->getString("language"));

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename");

	swprintf(filename, LengthOfArray(filename), L"%s",pathbase.c_str());

	vector <char*> strs = ProcessStringArrayINI_Internal(filename, lang, 0);

	for (int i = 0;i < count;i++)
	{
		int charaddr = (int)addr + i * length;
		// Clear the whole string before writing
		for (int c = 0; c < length; c++)
		{
			WriteData<1>((char*)(charaddr + c), 0i8);
		}
		// Write the string
		for (int c2 = 0; c2 < strlen(strs[i]); c2++)
		{
			WriteData<1>((char*)(charaddr + c2), strs[i][c2]);
		}
	}
}

static void ProcessMultiString(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	int length = group->getInt("length", 1);

	bool doublepnt = group->getBool("doublepointer", false);

	addr = (char*)((intptr_t)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename") + L'\\';
	
	for (unsigned int i = 0; i < LengthOfArray(languagenames); i++)
	{
		wchar_t filename[MAX_PATH]{};

		swprintf(filename, LengthOfArray(filename), L"%s\\%s.txt",
			pathbase.c_str(), languagenames[i]);

		vector <char*> strs = ProcessStringArrayINI_Internal(filename, i, 0);

		if (doublepnt)
		{
			int charaddr = *(int*)addr;
			for (int l = 0; l < length; l++)
			{
				WriteData((char**)charaddr, strs[l]);
				charaddr += 4;
			}
			addr += 4;
		}
		else
		{
			WriteData((char**)addr, strs[0]);
			addr += 4;
		}
	}
}

static void ProcessTikalSingleHint(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	int length = group->getInt("length", 1);

	addr = (char*)((intptr_t)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename");

	int lang = ParseLanguage(group->getString("language"));
	
	swprintf(filename, LengthOfArray(filename), pathbase.c_str());

	auto inidata = new IniFile(filename);

	for (unsigned int j = 0; j < length; j++)
	{
		char buf[8]{};

		snprintf(buf, sizeof(buf), "%u", j);

		if (!inidata->hasGroup(buf))
		{
			break;
		}

		const IniGroup* entdata = inidata->getGroup(buf);

		HintText_Text* msgstring = (HintText_Text*)(addr + 8 * j);
		msgstring->Message = strdup(DecodeUTF8(entdata->getString("Line"), lang, 0).c_str());
		msgstring->Time = entdata->getInt("Time");
	}
}

static void ProcessMissionDescriptions(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (char*)((intptr_t)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename");

	int lang = ParseLanguage(group->getString("language"));

	swprintf(filename, LengthOfArray(filename), pathbase.c_str());

	auto inidata = new IniFile(filename);

	for (unsigned int j = 0; j < 70; j++)
	{
		char buf[8]{};

		snprintf(buf, sizeof(buf), "%u", j);

		char* desc = strdup(DecodeUTF8(inidata->getString("", buf), lang, 0).c_str());
		int write = (int)addr + 208 * j;
		memset((char*)write, 0, 208);
		snprintf((char*)write, 208, desc);
	}
}

static void ProcessMissionTutoText(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	addr = (char*)((intptr_t)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename");

	int lang = ParseLanguage(group->getString("language"));
	
	swprintf(filename, LengthOfArray(filename), pathbase.c_str());

	auto inidata = new IniFile(filename);
	int numPages = inidata->getInt("", "NumPages", 0);
	WriteData((int*)addr, numPages);
	addr += 4;

	for (unsigned int j = 0; j < numPages; j++)
	{
		char buf[8]{};

		snprintf(buf, sizeof(buf), "%u", j);

		if (!inidata->hasGroup(buf))
		{
			break;
		}

		const IniGroup* entdata = inidata->getGroup(buf);
		int numLines = entdata->getInt("NumLines",0);
		int addr_ent = *(int*)addr + 8 * j;
		int strpointer = *(int*)(addr_ent += 4);
		for (int l = 0;l < numLines;l++)
		{
			snprintf(buf, sizeof(buf), "%u", l);
			char* str = strdup(DecodeUTF8(entdata->getString(buf), lang, 0).c_str());
			WriteData((char**)strpointer, str);
			strpointer += 4;
		}
	}
}

static void ProcessTikalMultiHint(const IniGroup* group, const wstring& mod_dir)
{
	if (!group->hasKeyNonEmpty("filename"))
	{
		return;
	}

	auto addr = (char*)group->getIntRadix("address", 16);

	if (addr == nullptr)
	{
		return;
	}

	int length = group->getInt("length", 1);

	addr = (char*)((intptr_t)addr + 0x400000);

	wchar_t filename[MAX_PATH]{};

	const wstring pathbase = mod_dir + L'\\' + group->getWString("filename");

	bool doublepnt = group->getBool("doublepointer", false);

	HintText_Text* msgstring;

	for (int l = 0; l < LengthOfArray(languagenames); l++)
	{
		swprintf(filename, LengthOfArray(filename), L"%s\\%s.ini", pathbase.c_str(), languagenames[l]);
		auto inidata = new IniFile(filename);
		int pointer_lang = (int)addr + l * 4;
		int entrypnt = *(int*)pointer_lang;
		for (unsigned int j = 0; j < length; j++)
		{
			char buf[8]{};

			snprintf(buf, sizeof(buf), "%u", j);

			if (!inidata->hasGroup(buf))
			{
				break;
			}

			const IniGroup* entdata = inidata->getGroup(buf);
			int entry = entrypnt;
			if (doublepnt)
			{
				entry = *(int*)entrypnt;
				msgstring = (HintText_Text*)(entry);
			}
			else
				msgstring = (HintText_Text*)(entry + 8 * j);
			msgstring->Message = strdup(DecodeUTF8(entdata->getString("Line"), l, 0).c_str());
			msgstring->Time = entdata->getInt("Time");
			if (doublepnt)
				entrypnt += 4;
		}
	}
}

using exedatafunc_t = void(__cdecl*)(const IniGroup* group, const wstring& mod_dir);

static const unordered_map<string, exedatafunc_t> exedatafuncmap = {
	{ "landtable",          ProcessLandTableINI },
	{ "model",              ProcessModelINI },
	{ "basicdxmodel",       ProcessModelINI },
	{ "chunkmodel",         ProcessModelINI },
	{ "action",             ProcessActionINI },
	{ "animation",          ProcessAnimationINI },
	{ "objlist",            ProcessObjListINI },
	{ "startpos",           ProcessStartPosINI },
	{ "texturedata",        ProcessTexListINI },
	{ "texlist",	        ProcessTexListINI },
	{ "leveltexlist",       ProcessLevelTexListINI },
	{ "triallevellist",     ProcessTrialLevelListINI },
	{ "bosslevellist",      ProcessBossLevelListINI },
	{ "fieldstartpos",      ProcessFieldStartPosINI },
	{ "soundtestlist",      ProcessSoundTestListINI },
	{ "musiclist",          ProcessMusicListINI },
	{ "soundlist",          ProcessSoundListINI },
	{ "stringarray",        ProcessStringArrayINI },
	{ "nextlevellist",      ProcessNextLevelListINI },
	{ "cutscenetext",       ProcessCutsceneTextINI },
	{ "recapscreen",        ProcessRecapScreenINI },
	{ "npctext",            ProcessNPCTextINI },
	{ "levelclearflaglist", ProcessLevelClearFlagListINI },
	{ "deathzone",          ProcessDeathZoneINI },
	{ "skyboxscale",        ProcessSkyboxScaleINI },
	{ "levelpathlist",      ProcessLevelPathListINI },
	{ "stagelightdatalist", ProcessStageLightDataListINI },
	{ "palettelightlist",   ProcessPaletteLightListINI },
	{ "weldlist",           ProcessWeldListINI },
	{ "creditstextlist",    ProcessCreditsTextListINI },
	{ "physicsdata",		ProcessPhysicsDataINI },
	{ "fogdatatable",		ProcessFogDataINI },
	{ "singlestring",		ProcessSingleString },
	{ "multistring",		ProcessMultiString },
	{ "tikalhintsingle",	ProcessTikalSingleHint },
	{ "tikalhintmulti",		ProcessTikalMultiHint },
	{ "missiontutorial",	ProcessMissionTutoText },
	{ "missiondescription",	ProcessMissionDescriptions },
	{ "fixedstringarray",	ProcessFixedStringArray },
	// { "bmitemattrlist",     ProcessBMItemAttrListINI },
};

void ProcessEXEData(const wchar_t* filename, const wstring& mod_dir)
{
	if (FileExists(filename))
	{
		auto exedata = new IniFile(filename);

		for (const auto& iter : *exedata)
		{
			IniGroup* group = iter.second;
			auto type = exedatafuncmap.find(group->getString("type"));

			if (type != exedatafuncmap.end())
			{
				type->second(group, mod_dir);
			}
		}

		delete exedata;
	}
}
