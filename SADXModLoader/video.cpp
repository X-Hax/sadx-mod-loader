#include "stdafx.h"
#include <DShow.h>

DataPointer(IMediaControl*, g_pMC, 0x3C600F8);

Bool PauseVideo()
{
	return g_pMC && SUCCEEDED(g_pMC->Pause());
}

Bool ResumeVideo()
{
	return g_pMC && SUCCEEDED(g_pMC->Run());
}