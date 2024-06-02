extern "C"
{
	__declspec(dllexport) void ffPlayerPlay();
	__declspec(dllexport) void ffPlayerPause();
	__declspec(dllexport) bool ffPlayerFinished();
	__declspec(dllexport) bool ffPlayerOpen(const char* path, bool sfd);
	__declspec(dllexport) void ffPlayerClose();
	__declspec(dllexport) bool ffPlayerGetFrameBuffer(unsigned char* pBuffer);
	__declspec(dllexport) unsigned int ffPlayerWidth();
	__declspec(dllexport) unsigned int ffPlayerHeight();
}