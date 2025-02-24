#pragma once
#include "MemAccess.h"
#include <cassert>
#define USERCALLDEBUG 0

enum registers
{
	rEAX,
	rEBX,
	rECX,
	rEDX,
	rESI,
	rEDI,
	rEBP,
	rAX,
	rBX,
	rCX,
	rDX,
	rSI,
	rDI,
	rBP,
	rAL,
	rBL,
	rCL,
	rDL,
	rAH,
	rBH,
	rCH,
	rDH,
	stack1,
	stack2,
	stack4,
	rst0,
	noret
};

static int regfamilies[] = {
	rEAX,
	rEBX,
	rECX,
	rEDX,
	rESI,
	rEDI,
	rEBP,
	rEAX,
	rEBX,
	rECX,
	rEDX,
	rESI,
	rEDI,
	rEBP,
	rEAX,
	rEBX,
	rECX,
	rEDX,
	rEAX,
	rEBX,
	rECX,
	rEDX,
	stack1,
	stack2,
	stack4,
	rst0,
	noret
};

inline bool regsequal(int reg1, int reg2)
{
	return regfamilies[reg1] == regfamilies[reg2];
}

inline void writebytes(uint8_t* dst, int& dstoff, uint8_t a1, uint8_t a2)
{
	dst[dstoff++] = a1;
	dst[dstoff++] = a2;
}

inline void writebytes(uint8_t* dst, int& dstoff, uint8_t a1, uint8_t a2, uint8_t a3)
{
	dst[dstoff++] = a1;
	dst[dstoff++] = a2;
	dst[dstoff++] = a3;
}

inline void writebytes(uint8_t* dst, int& dstoff, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4)
{
	dst[dstoff++] = a1;
	dst[dstoff++] = a2;
	dst[dstoff++] = a3;
	dst[dstoff++] = a4;
}

inline void writebytes(uint8_t* dst, int& dstoff, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5)
{
	dst[dstoff++] = a1;
	dst[dstoff++] = a2;
	dst[dstoff++] = a3;
	dst[dstoff++] = a4;
	dst[dstoff++] = a5;
}

inline void writebytes(uint8_t* dst, int& dstoff, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5, uint8_t a6)
{
	dst[dstoff++] = a1;
	dst[dstoff++] = a2;
	dst[dstoff++] = a3;
	dst[dstoff++] = a4;
	dst[dstoff++] = a5;
	dst[dstoff++] = a6;
}

inline void writebytes(uint8_t* dst, int& dstoff, uint8_t a1, uint8_t a2, uint8_t a3, uint8_t a4, uint8_t a5, uint8_t a6, uint8_t a7, uint8_t a8)
{
	dst[dstoff++] = a1;
	dst[dstoff++] = a2;
	dst[dstoff++] = a3;
	dst[dstoff++] = a4;
	dst[dstoff++] = a5;
	dst[dstoff++] = a6;
	dst[dstoff++] = a7;
	dst[dstoff++] = a8;
}

static uint8_t* curpg = nullptr;
static int pgoff = 0;
static int pgsz = -1;
static uint8_t* AllocateCode(int sz)
{
	if (pgsz == -1)
	{
		SYSTEM_INFO sysinf;
		GetNativeSystemInfo(&sysinf);
		pgsz = sysinf.dwPageSize;
	}
	if (curpg == nullptr || (pgoff + sz) > pgsz)
	{
		curpg = (uint8_t*)VirtualAlloc(nullptr, pgsz, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
		pgoff = 0;
		assert(curpg != nullptr);
	}
	uint8_t* result = &curpg[pgoff];
	pgoff += sz;
	if (pgoff % 0x10 != 0)
		pgoff += 0x10 - (pgoff % 0x10);
	return result;
}

template<typename T, typename... TArgs>
constexpr T const GenerateUsercallWrapper(int ret, intptr_t address, TArgs... args)
{
	const size_t argc = sizeof...(TArgs);
	int argarray[] = { args... };
	int stackcnt = 0;
	int memsz = 0;
	for (size_t i = 0; i < argc; ++i)
	{
		switch (argarray[i])
		{
		case rEAX:
		case rECX:
		case rEDX:
			memsz += 4;
			break;
		case rEBX:
		case rESI:
		case rEDI:
		case rEBP:
			memsz += 6;
			break;
		case rAX:
		case rCX:
		case rDX:
			memsz += 5;
			break;
		case rBX:
		case rSI:
		case rDI:
		case rBP:
			memsz += 7;
			break;
		case rAL:
		case rCL:
		case rDL:
		case rAH:
		case rCH:
		case rDH:
			memsz += 4;
			break;
		case rBL:
		case rBH:
			memsz += 6;
			break;
		case stack1:
		case stack2:
			memsz += 6;
			++stackcnt;
			break;
		case stack4:
			memsz += 4;
			++stackcnt;
			break;
		}
	}
	memsz += 5; // call
	if (stackcnt > 0)
		memsz += 3;
	switch (ret)
	{
	case rEBX:
	case rECX:
	case rEDX:
	case rESI:
	case rEDI:
	case rEBP:
		memsz += 2;
		break;
	case rAX:
	case rBX:
	case rCX:
	case rDX:
	case rSI:
	case rDI:
	case rBP:
	case rAL:
	case rBL:
	case rCL:
	case rDL:
	case rAH:
	case rBH:
	case rCH:
	case rDH:
		memsz += 3;
		break;
	case rst0:
		memsz += 8;
		break;
	}
	++memsz; // retn
	auto codeData = AllocateCode(memsz);
	int cdoff = 0;
	uint8_t stackoff = argc * 4;
	for (size_t i = 0; i < argc; ++i)
	{
		switch (argarray[i])
		{
		case rEBX:
		case rBX:
		case rBH:
		case rBL:
			codeData[cdoff++] = 0x53;
			stackoff += 4;
			break;
		case rESI:
		case rSI:
			codeData[cdoff++] = 0x56;
			stackoff += 4;
			break;
		case rEDI:
		case rDI:
			codeData[cdoff++] = 0x57;
			stackoff += 4;
			break;
		case rEBP:
		case rBP:
			codeData[cdoff++] = 0x55;
			stackoff += 4;
			break;
		}
	}
	for (int i = argc - 1; i >= 0; --i)
	{
		switch (argarray[i])
		{
		case rEAX:
			writebytes(codeData, cdoff, 0x8B, 0x44, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEBX:
			writebytes(codeData, cdoff, 0x8B, 0x5C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rECX:
			writebytes(codeData, cdoff, 0x8B, 0x4C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEDX:
			writebytes(codeData, cdoff, 0x8B, 0x54, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rESI:
			writebytes(codeData, cdoff, 0x8B, 0x74, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEDI:
			writebytes(codeData, cdoff, 0x8B, 0x7C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEBP:
			writebytes(codeData, cdoff, 0x8B, 0x6C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rAX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x44, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x5C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rCX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x4C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x54, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rSI:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x74, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDI:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x7C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBP:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x6C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rAL:
			writebytes(codeData, cdoff, 0x8A, 0x44, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBL:
			writebytes(codeData, cdoff, 0x8A, 0x5C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rCL:
			writebytes(codeData, cdoff, 0x8A, 0x4C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDL:
			writebytes(codeData, cdoff, 0x8A, 0x54, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rAH:
			writebytes(codeData, cdoff, 0x8A, 0x64, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBH:
			writebytes(codeData, cdoff, 0x8A, 0x7C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rCH:
			writebytes(codeData, cdoff, 0x8A, 0x6C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDH:
			writebytes(codeData, cdoff, 0x8A, 0x74, 0x24, stackoff);
			stackoff -= 4;
			break;
		case stack1:
			writebytes(codeData, cdoff, 0x0F, 0xBE, 0x44, 0x24, stackoff, 0x50);
			break;
		case stack2:
			writebytes(codeData, cdoff, 0x0F, 0xBF, 0x44, 0x24, stackoff, 0x50);
			break;
		case stack4:
			writebytes(codeData, cdoff, 0xFF, 0x74, 0x24, stackoff);
			break;
		}
	}
	WriteCall(&codeData[cdoff], (void*)address);
	cdoff += 5;
	if (stackcnt > 0)
		writebytes(codeData, cdoff, 0x83, 0xC4, (uint8_t)(stackcnt * 4));
	for (int i = argc - 1; i >= 0; --i)
	{
		switch (argarray[i])
		{
		case rEBX:
		case rBX:
		case rBL:
		case rBH:
			codeData[cdoff++] = 0x5B;
			break;
		case rESI:
		case rSI:
			codeData[cdoff++] = 0x5E;
			break;
		case rEDI:
		case rDI:
			codeData[cdoff++] = 0x5F;
			break;
		case rEBP:
		case rBP:
			codeData[cdoff++] = 0x5D;
			break;
		}
	}
	switch (ret)
	{
	case rEBX:
		writebytes(codeData, cdoff, 0x89, 0xD8);
		break;
	case rECX:
		writebytes(codeData, cdoff, 0x89, 0xC8);
		break;
	case rEDX:
		writebytes(codeData, cdoff, 0x89, 0xD0);
		break;
	case rESI:
		writebytes(codeData, cdoff, 0x89, 0xF0);
		break;
	case rEDI:
		writebytes(codeData, cdoff, 0x89, 0xF8);
		break;
	case rEBP:
		writebytes(codeData, cdoff, 0x89, 0xE8);
		break;
	case rAX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC0);
		break;
	case rBX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC3);
		break;
	case rCX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC1);
		break;
	case rDX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC2);
		break;
	case rSI:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC6);
		break;
	case rDI:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC7);
		break;
	case rBP:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC5);
		break;
	case rAL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC0);
		break;
	case rBL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC3);
		break;
	case rCL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC1);
		break;
	case rDL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC2);
		break;
	case rAH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC4);
		break;
	case rBH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC7);
		break;
	case rCH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC5);
		break;
	case rDH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC6);
		break;
	case rst0:
		writebytes(codeData, cdoff, 0xD9, 0x5C, 0x24, 0xFC, 0x8B, 0x44, 0x24, 0xFC);
		break;
	}
	codeData[cdoff++] = 0xC3;
#if USERCALLDEBUG
	char fn[MAX_PATH];
	sprintf_s(fn, "usercallwrapper@%08X.bin", address);
	auto fh = fopen(fn, "wb");
	fwrite(codeData, memsz, 1, fh);
	fclose(fh);
#endif
	assert(cdoff == memsz);
	return (T)codeData;
}

template<typename T, typename... TArgs>
constexpr void const GenerateUsercallHook(T func, int ret, intptr_t address, TArgs... args)
{
	const size_t argc = sizeof...(TArgs);
	int argarray[] = { args... };
	int stackcnt = 0;
	int memsz = 0;
	for (size_t i = 0; i < argc; ++i)
	{
		switch (argarray[i])
		{
		case rEAX:
		case rAX:
		case rAL:
		case rEBX:
		case rBX:
		case rBL:
		case rECX:
		case rCX:
		case rCL:
		case rEDX:
		case rDX:
		case rDL:
		case rESI:
		case rSI:
		case rEDI:
		case rDI:
		case rEBP:
		case rBP:
			++memsz;
			break;
		case rAH:
			break;
		case rBH:
			break;
		case rCH:
			break;
		case rDH:
			break;
		case stack1:
		case stack2:
		case stack4:
			memsz += 4;
			++stackcnt;
			break;
		}
	}
	memsz += 5; // call
	switch (ret)
	{
	case rEBX:
	case rBX:
	case rBL:
	case rECX:
	case rCX:
	case rCL:
	case rEDX:
	case rDX:
	case rDL:
	case rESI:
	case rSI:
	case rEDI:
	case rDI:
	case rEBP:
	case rBP:
		memsz += 2;
		break;
	case rAH:
		break;
	case rBH:
		break;
	case rCH:
		break;
	case rDH:
		break;
	}
	for (int i = 0; i < argc; ++i)
	{
		if (regsequal(argarray[i], ret))
			memsz += 3;
		else
			switch (argarray[i])
			{
			case rEAX:
			case rAX:
			case rAL:
			case rAH:
			case rEBX:
			case rBX:
			case rBL:
			case rBH:
			case rECX:
			case rCX:
			case rCL:
			case rCH:
			case rEDX:
			case rDX:
			case rDL:
			case rDH:
			case rESI:
			case rSI:
			case rEDI:
			case rDI:
			case rEBP:
			case rBP:
				++memsz;
				break;
			}
	}
	if (stackcnt > 0)
		memsz += 3;
	++memsz; // retn
	auto codeData = AllocateCode(memsz);
	int cdoff = 0;
	uint8_t stackoff = stackcnt * 4;
	for (int i = argc - 1; i >= 0; --i)
	{
		switch (argarray[i])
		{
		case rEAX:
		case rAX:
		case rAL:
			codeData[cdoff++] = 0x50;
			break;
		case rEBX:
		case rBX:
		case rBL:
			codeData[cdoff++] = 0x53;
			break;
		case rECX:
		case rCX:
		case rCL:
			codeData[cdoff++] = 0x51;
			break;
		case rEDX:
		case rDX:
		case rDL:
			codeData[cdoff++] = 0x52;
			break;
		case rESI:
		case rSI:
			codeData[cdoff++] = 0x56;
			break;
		case rEDI:
		case rDI:
			codeData[cdoff++] = 0x57;
			break;
		case rEBP:
		case rBP:
			codeData[cdoff++] = 0x55;
			break;
		case rAH:
			break;
		case rBH:
			break;
		case rCH:
			break;
		case rDH:
			break;
		case stack1:
		case stack2:
		case stack4:
			writebytes(codeData, cdoff, 0xFF, 0x74, 0x24, stackoff);
			break;
		}
	}
	WriteCall(&codeData[cdoff], func);
	cdoff += 5;
	switch (ret)
	{
	case rEBX:
	case rBX:
	case rBL:
		writebytes(codeData, cdoff, 0x89, 0xC3);
		break;
	case rECX:
	case rCX:
	case rCL:
		writebytes(codeData, cdoff, 0x89, 0xC1);
		break;
	case rEDX:
	case rDX:
	case rDL:
		writebytes(codeData, cdoff, 0x89, 0xC2);
		break;
	case rESI:
	case rSI:
		writebytes(codeData, cdoff, 0x89, 0xC6);
		break;
	case rEDI:
	case rDI:
		writebytes(codeData, cdoff, 0x89, 0xC7);
		break;
	case rEBP:
	case rBP:
		writebytes(codeData, cdoff, 0x89, 0xC5);
		break;
	case rAH:
		break;
	case rBH:
		break;
	case rCH:
		break;
	case rDH:
		break;
	}
	for (int i = 0; i < argc; ++i)
	{
		if (regsequal(argarray[i], ret))
			writebytes(codeData, cdoff, 0x83, 0xC4, 4);
		else
			switch (argarray[i])
			{
			case rEAX:
			case rAX:
			case rAL:
			case rAH:
				codeData[cdoff++] = 0x58;
				break;
			case rEBX:
			case rBX:
			case rBL:
			case rBH:
				codeData[cdoff++] = 0x5B;
				break;
			case rECX:
			case rCX:
			case rCL:
			case rCH:
				codeData[cdoff++] = 0x59;
				break;
			case rEDX:
			case rDX:
			case rDL:
			case rDH:
				codeData[cdoff++] = 0x5A;
				break;
			case rESI:
			case rSI:
				codeData[cdoff++] = 0x5E;
				break;
			case rEDI:
			case rDI:
				codeData[cdoff++] = 0x5F;
				break;
			case rEBP:
			case rBP:
				codeData[cdoff++] = 0x5D;
				break;
			}
	}
	if (stackcnt > 0)
		writebytes(codeData, cdoff, 0x83, 0xC4, (uint8_t)(stackcnt * 4));
	codeData[cdoff++] = 0xC3;
#if USERCALLDEBUG
	char fn[MAX_PATH];
	sprintf_s(fn, "usercallhook@%08X.bin", address);
	auto fh = fopen(fn, "wb");
	fwrite(codeData, memsz, 1, fh);
	fclose(fh);
#endif
	assert(cdoff == memsz);
	if (*(uint8_t*)address == 0xE8)
		WriteCall((void*)address, codeData);
	else
		WriteJump((void*)address, codeData);
}

template<typename T, typename... TArgs>
constexpr T const GenerateUserpurgeWrapper(int ret, intptr_t address, TArgs... args)
{
	const size_t argc = sizeof...(TArgs);
	int argarray[] = { args... };
	int stackcnt = 0;
	int memsz = 0;
	for (size_t i = 0; i < argc; ++i)
	{
		switch (argarray[i])
		{
		case rEAX:
		case rECX:
		case rEDX:
			memsz += 4;
			break;
		case rEBX:
		case rESI:
		case rEDI:
		case rEBP:
			memsz += 6;
			break;
		case rAX:
		case rCX:
		case rDX:
			memsz += 5;
			break;
		case rBX:
		case rSI:
		case rDI:
		case rBP:
			memsz += 7;
			break;
		case rAL:
		case rCL:
		case rDL:
		case rAH:
		case rCH:
		case rDH:
			memsz += 4;
			break;
		case rBL:
		case rBH:
			memsz += 6;
			break;
		case stack1:
		case stack2:
			memsz += 6;
			++stackcnt;
			break;
		case stack4:
			memsz += 4;
			++stackcnt;
			break;
		}
	}
	memsz += 5; // call
	switch (ret)
	{
	case rEBX:
	case rECX:
	case rEDX:
	case rESI:
	case rEDI:
	case rEBP:
		memsz += 2;
		break;
	case rAX:
	case rBX:
	case rCX:
	case rDX:
	case rSI:
	case rDI:
	case rBP:
	case rAL:
	case rBL:
	case rCL:
	case rDL:
	case rAH:
	case rBH:
	case rCH:
	case rDH:
		memsz += 3;
		break;
	case rst0:
		memsz += 8;
		break;
	}
	++memsz; // retn
	auto codeData = AllocateCode(memsz);
	int cdoff = 0;
	uint8_t stackoff = argc * 4;
	for (size_t i = 0; i < argc; ++i)
	{
		switch (argarray[i])
		{
		case rEBX:
		case rBX:
		case rBH:
		case rBL:
			codeData[cdoff++] = 0x53;
			stackoff += 4;
			break;
		case rESI:
		case rSI:
			codeData[cdoff++] = 0x56;
			stackoff += 4;
			break;
		case rEDI:
		case rDI:
			codeData[cdoff++] = 0x57;
			stackoff += 4;
			break;
		case rEBP:
		case rBP:
			codeData[cdoff++] = 0x55;
			stackoff += 4;
			break;
		}
	}
	for (int i = argc - 1; i >= 0; --i)
	{
		switch (argarray[i])
		{
		case rEAX:
			writebytes(codeData, cdoff, 0x8B, 0x44, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEBX:
			writebytes(codeData, cdoff, 0x8B, 0x5C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rECX:
			writebytes(codeData, cdoff, 0x8B, 0x4C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEDX:
			writebytes(codeData, cdoff, 0x8B, 0x54, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rESI:
			writebytes(codeData, cdoff, 0x8B, 0x74, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEDI:
			writebytes(codeData, cdoff, 0x8B, 0x7C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rEBP:
			writebytes(codeData, cdoff, 0x8B, 0x6C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rAX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x44, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x5C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rCX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x4C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDX:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x54, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rSI:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x74, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDI:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x7C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBP:
			writebytes(codeData, cdoff, 0x66, 0x8B, 0x6C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rAL:
			writebytes(codeData, cdoff, 0x8A, 0x44, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBL:
			writebytes(codeData, cdoff, 0x8A, 0x5C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rCL:
			writebytes(codeData, cdoff, 0x8A, 0x4C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDL:
			writebytes(codeData, cdoff, 0x8A, 0x54, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rAH:
			writebytes(codeData, cdoff, 0x8A, 0x64, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rBH:
			writebytes(codeData, cdoff, 0x8A, 0x7C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rCH:
			writebytes(codeData, cdoff, 0x8A, 0x6C, 0x24, stackoff);
			stackoff -= 4;
			break;
		case rDH:
			writebytes(codeData, cdoff, 0x8A, 0x74, 0x24, stackoff);
			stackoff -= 4;
			break;
		case stack1:
			writebytes(codeData, cdoff, 0x0F, 0xBE, 0x44, 0x24, stackoff, 0x50);
			break;
		case stack2:
			writebytes(codeData, cdoff, 0x0F, 0xBF, 0x44, 0x24, stackoff, 0x50);
			break;
		case stack4:
			writebytes(codeData, cdoff, 0xFF, 0x74, 0x24, stackoff);
			break;
		}
	}
	WriteCall(&codeData[cdoff], (void*)address);
	cdoff += 5;
	for (int i = argc - 1; i >= 0; --i)
	{
		switch (argarray[i])
		{
		case rEBX:
		case rBX:
		case rBL:
		case rBH:
			codeData[cdoff++] = 0x5B;
			break;
		case rESI:
		case rSI:
			codeData[cdoff++] = 0x5E;
			break;
		case rEDI:
		case rDI:
			codeData[cdoff++] = 0x5F;
			break;
		case rEBP:
		case rBP:
			codeData[cdoff++] = 0x5D;
			break;
		}
	}
	switch (ret)
	{
	case rEBX:
		writebytes(codeData, cdoff, 0x89, 0xD8);
		break;
	case rECX:
		writebytes(codeData, cdoff, 0x89, 0xC8);
		break;
	case rEDX:
		writebytes(codeData, cdoff, 0x89, 0xD0);
		break;
	case rESI:
		writebytes(codeData, cdoff, 0x89, 0xF0);
		break;
	case rEDI:
		writebytes(codeData, cdoff, 0x89, 0xF8);
		break;
	case rEBP:
		writebytes(codeData, cdoff, 0x89, 0xE8);
		break;
	case rAX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC0);
		break;
	case rBX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC3);
		break;
	case rCX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC1);
		break;
	case rDX:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC2);
		break;
	case rSI:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC6);
		break;
	case rDI:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC7);
		break;
	case rBP:
		writebytes(codeData, cdoff, 0x0F, 0xBF, 0xC5);
		break;
	case rAL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC0);
		break;
	case rBL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC3);
		break;
	case rCL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC1);
		break;
	case rDL:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC2);
		break;
	case rAH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC4);
		break;
	case rBH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC7);
		break;
	case rCH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC5);
		break;
	case rDH:
		writebytes(codeData, cdoff, 0x0F, 0xBE, 0xC6);
		break;
	case rst0:
		writebytes(codeData, cdoff, 0xD9, 0x5C, 0x24, 0xFC, 0x8B, 0x44, 0x24, 0xFC);
		break;
	}
	codeData[cdoff++] = 0xC3;
#if USERCALLDEBUG
	char fn[MAX_PATH];
	sprintf_s(fn, "usercallwrapper@%08X.bin", address);
	auto fh = fopen(fn, "wb");
	fwrite(codeData, memsz, 1, fh);
	fclose(fh);
#endif
	assert(cdoff == memsz);
	return (T)codeData;
}

template<typename T, typename... TArgs>
constexpr void const GenerateUserpurgeHook(T func, int ret, intptr_t address, TArgs... args)
{
	const size_t argc = sizeof...(TArgs);
	int argarray[] = { args... };
	int stackcnt = 0;
	int memsz = 0;
	for (size_t i = 0; i < argc; ++i)
	{
		switch (argarray[i])
		{
		case rEAX:
		case rAX:
		case rAL:
		case rEBX:
		case rBX:
		case rBL:
		case rECX:
		case rCX:
		case rCL:
		case rEDX:
		case rDX:
		case rDL:
		case rESI:
		case rSI:
		case rEDI:
		case rDI:
		case rEBP:
		case rBP:
			++memsz;
			break;
		case rAH:
			break;
		case rBH:
			break;
		case rCH:
			break;
		case rDH:
			break;
		case stack1:
		case stack2:
		case stack4:
			memsz += 4;
			++stackcnt;
			break;
		}
	}
	memsz += 5; // call
	switch (ret)
	{
	case rEBX:
	case rBX:
	case rBL:
	case rECX:
	case rCX:
	case rCL:
	case rEDX:
	case rDX:
	case rDL:
	case rESI:
	case rSI:
	case rEDI:
	case rDI:
	case rEBP:
	case rBP:
		memsz += 2;
		break;
	case rAH:
		break;
	case rBH:
		break;
	case rCH:
		break;
	case rDH:
		break;
	}
	for (int i = 0; i < argc; ++i)
	{
		if (regsequal(argarray[i], ret))
			memsz += 3;
		else
			switch (argarray[i])
			{
			case rEAX:
			case rAX:
			case rAL:
			case rAH:
			case rEBX:
			case rBX:
			case rBL:
			case rBH:
			case rECX:
			case rCX:
			case rCL:
			case rCH:
			case rEDX:
			case rDX:
			case rDL:
			case rDH:
			case rESI:
			case rSI:
			case rEDI:
			case rDI:
			case rEBP:
			case rBP:
				++memsz;
				break;
			}
	}
	if (stackcnt > 0)
		memsz += 3 + 2;
	++memsz; // retn
	auto codeData = AllocateCode(memsz);
	int cdoff = 0;
	uint8_t stackoff = stackcnt * 4;
	for (int i = argc - 1; i >= 0; --i)
	{
		switch (argarray[i])
		{
		case rEAX:
		case rAX:
		case rAL:
			codeData[cdoff++] = 0x50;
			break;
		case rEBX:
		case rBX:
		case rBL:
			codeData[cdoff++] = 0x53;
			break;
		case rECX:
		case rCX:
		case rCL:
			codeData[cdoff++] = 0x51;
			break;
		case rEDX:
		case rDX:
		case rDL:
			codeData[cdoff++] = 0x52;
			break;
		case rESI:
		case rSI:
			codeData[cdoff++] = 0x56;
			break;
		case rEDI:
		case rDI:
			codeData[cdoff++] = 0x57;
			break;
		case rEBP:
		case rBP:
			codeData[cdoff++] = 0x55;
			break;
		case rAH:
			break;
		case rBH:
			break;
		case rCH:
			break;
		case rDH:
			break;
		case stack1:
		case stack2:
		case stack4:
			writebytes(codeData, cdoff, 0xFF, 0x74, 0x24, stackoff);
			break;
		}
	}
	WriteCall(&codeData[cdoff], func);
	cdoff += 5;
	switch (ret)
	{
	case rEBX:
	case rBX:
	case rBL:
		writebytes(codeData, cdoff, 0x89, 0xC3);
		break;
	case rECX:
	case rCX:
	case rCL:
		writebytes(codeData, cdoff, 0x89, 0xC1);
		break;
	case rEDX:
	case rDX:
	case rDL:
		writebytes(codeData, cdoff, 0x89, 0xC2);
		break;
	case rESI:
	case rSI:
		writebytes(codeData, cdoff, 0x89, 0xC6);
		break;
	case rEDI:
	case rDI:
		writebytes(codeData, cdoff, 0x89, 0xC7);
		break;
	case rEBP:
	case rBP:
		writebytes(codeData, cdoff, 0x89, 0xC5);
		break;
	case rAH:
		break;
	case rBH:
		break;
	case rCH:
		break;
	case rDH:
		break;
	}
	for (int i = 0; i < argc; ++i)
	{
		if (regsequal(argarray[i], ret))
			writebytes(codeData, cdoff, 0x83, 0xC4, 4);
		else
			switch (argarray[i])
			{
			case rEAX:
			case rAX:
			case rAL:
			case rAH:
				codeData[cdoff++] = 0x58;
				break;
			case rEBX:
			case rBX:
			case rBL:
			case rBH:
				codeData[cdoff++] = 0x5B;
				break;
			case rECX:
			case rCX:
			case rCL:
			case rCH:
				codeData[cdoff++] = 0x59;
				break;
			case rEDX:
			case rDX:
			case rDL:
			case rDH:
				codeData[cdoff++] = 0x5A;
				break;
			case rESI:
			case rSI:
				codeData[cdoff++] = 0x5E;
				break;
			case rEDI:
			case rDI:
				codeData[cdoff++] = 0x5F;
				break;
			case rEBP:
			case rBP:
				codeData[cdoff++] = 0x5D;
				break;
			}
	}
	if (stackcnt > 0) {
		writebytes(codeData, cdoff, 0x83, 0xC4, (uint8_t)(stackcnt * 4));
		writebytes(codeData, cdoff, 0xC2, (uint8_t)(stackcnt * 4), 0x00); // retn + callee cleanup
	}
	else
		codeData[cdoff++] = 0xC3;
#if USERCALLDEBUG
	char fn[MAX_PATH];
	sprintf_s(fn, "usercallhook@%08X.bin", address);
	auto fh = fopen(fn, "wb");
	fwrite(codeData, memsz, 1, fh);
	fclose(fh);
#endif
	assert(cdoff == memsz);
	if (*(uint8_t*)address == 0xE8)
		WriteCall((void*)address, codeData);
	else
		WriteJump((void*)address, codeData);
}

#define UsercallFunc(RETURN_TYPE, NAME, ARGS, ARGNAMES, ADDRESS, RETURN_LOC, ...) \
class _##NAME##_t \
{ \
public: \
	typedef RETURN_TYPE(*PointerType)ARGS; \
 \
	RETURN_TYPE operator()ARGS \
	{ \
		return wrapper##ARGNAMES; \
	} \
 \
	PointerType operator&() \
	{ \
		return wrapper; \
	} \
 \
	operator PointerType() \
	{ \
		return wrapper; \
	} \
 \
	void Hook(PointerType hookfunc) \
	{ \
		if (ishooked) \
			throw new std::exception("Attempted to hook already hooked function!"); \
		memcpy(origdata, getptr(), 5); \
		GenerateUsercallHook<PointerType>(hookfunc, RETURN_LOC, ADDRESS, __VA_ARGS__); \
		ishooked = true; \
	} \
 \
	RETURN_TYPE Original##ARGS \
	{ \
		if (ishooked) \
		{ \
			uint8_t hookdata[5]; \
			memcpy(hookdata, getptr(), 5); \
			memcpy(getptr(), origdata, 5); \
			RETURN_TYPE retval = wrapper##ARGNAMES; \
			memcpy(getptr(), hookdata, 5); \
			return retval; \
		} \
		else \
			return wrapper##ARGNAMES; \
	} \
 \
private: \
	bool ishooked = false; \
	uint8_t origdata[5]{}; \
	const PointerType wrapper = GenerateUsercallWrapper<PointerType>(RETURN_LOC, ADDRESS, __VA_ARGS__); \
 \
	constexpr PointerType getptr() \
	{ \
		return reinterpret_cast<PointerType>(ADDRESS); \
	} \
}; \
static _##NAME##_t NAME

#define UsercallFuncVoid(NAME, ARGS, ARGNAMES, ADDRESS, ...) \
class _##NAME##_t \
{ \
public: \
	typedef void (*PointerType)ARGS; \
 \
	void operator()ARGS \
	{ \
		wrapper##ARGNAMES; \
	} \
 \
	PointerType operator&() \
	{ \
		return wrapper; \
	} \
 \
	operator PointerType() \
	{ \
		return wrapper; \
	} \
 \
	void Hook(PointerType hookfunc) \
	{ \
		if (ishooked) \
			throw new std::exception("Attempted to hook already hooked function!"); \
		memcpy(origdata, getptr(), 5); \
		GenerateUsercallHook<PointerType>(hookfunc, noret, ADDRESS, __VA_ARGS__); \
		ishooked = true; \
	} \
 \
	void Original##ARGS \
	{ \
		if (ishooked) \
		{ \
			uint8_t hookdata[5]; \
			memcpy(hookdata, getptr(), 5); \
			memcpy(getptr(), origdata, 5); \
			wrapper##ARGNAMES; \
			memcpy(getptr(), hookdata, 5); \
		} \
		else \
			wrapper##ARGNAMES; \
	} \
 \
private: \
	bool ishooked = false; \
	uint8_t origdata[5]{}; \
	const PointerType wrapper = GenerateUsercallWrapper<PointerType>(noret, ADDRESS, __VA_ARGS__); \
 \
	constexpr PointerType getptr() \
	{ \
		return reinterpret_cast<PointerType>(ADDRESS); \
	} \
}; \
static _##NAME##_t NAME

#define UserpurgeFunc(RETURN_TYPE, NAME, ARGS, ARGNAMES, ADDRESS, RETURN_LOC, ...) \
class _##NAME##_t \
{ \
public: \
	typedef RETURN_TYPE(*PointerType)ARGS; \
 \
	RETURN_TYPE operator()ARGS \
	{ \
		return wrapper##ARGNAMES; \
	} \
 \
	PointerType operator&() \
	{ \
		return wrapper; \
	} \
 \
	operator PointerType() \
	{ \
		return wrapper; \
	} \
 \
	void Hook(PointerType hookfunc) \
	{ \
		if (ishooked) \
			throw new std::exception("Attempted to hook already hooked function!"); \
		memcpy(origdata, getptr(), 5); \
		GenerateUserpurgeHook<PointerType>(hookfunc, RETURN_LOC, ADDRESS, __VA_ARGS__); \
		ishooked = true; \
	} \
 \
	RETURN_TYPE Original##ARGS \
	{ \
		if (ishooked) \
		{ \
			uint8_t hookdata[5]; \
			memcpy(hookdata, getptr(), 5); \
			memcpy(getptr(), origdata, 5); \
			RETURN_TYPE retval = wrapper##ARGNAMES; \
			memcpy(getptr(), hookdata, 5); \
			return retval; \
		} \
		else \
			return wrapper##ARGNAMES; \
	} \
 \
private: \
	bool ishooked = false; \
	uint8_t origdata[5]{}; \
	const PointerType wrapper = GenerateUserpurgeWrapper<PointerType>(RETURN_LOC, ADDRESS, __VA_ARGS__); \
 \
	constexpr PointerType getptr() \
	{ \
		return reinterpret_cast<PointerType>(ADDRESS); \
	} \
}; \
static _##NAME##_t NAME

#define UserpurgeFuncVoid(NAME, ARGS, ARGNAMES, ADDRESS, ...) \
class _##NAME##_t \
{ \
public: \
	typedef void (*PointerType)ARGS; \
 \
	void operator()ARGS \
	{ \
		wrapper##ARGNAMES; \
	} \
 \
	PointerType operator&() \
	{ \
		return wrapper; \
	} \
 \
	operator PointerType() \
	{ \
		return wrapper; \
	} \
 \
	void Hook(PointerType hookfunc) \
	{ \
		if (ishooked) \
			throw new std::exception("Attempted to hook already hooked function!"); \
		memcpy(origdata, getptr(), 5); \
		GenerateUserpurgeHook<PointerType>(hookfunc, noret, ADDRESS, __VA_ARGS__); \
		ishooked = true; \
	} \
 \
	void Original##ARGS \
	{ \
		if (ishooked) \
		{ \
			uint8_t hookdata[5]; \
			memcpy(hookdata, getptr(), 5); \
			memcpy(getptr(), origdata, 5); \
			wrapper##ARGNAMES; \
			memcpy(getptr(), hookdata, 5); \
		} \
		else \
			wrapper##ARGNAMES; \
	} \
 \
private: \
	bool ishooked = false; \
	uint8_t origdata[5]{}; \
	const PointerType wrapper = GenerateUserpurgeWrapper<PointerType>(noret, ADDRESS, __VA_ARGS__); \
 \
	constexpr PointerType getptr() \
	{ \
		return reinterpret_cast<PointerType>(ADDRESS); \
	} \
}; \
static _##NAME##_t NAME
