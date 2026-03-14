#pragma once

// Thin C-style wrapper around StormLib to avoid TCHAR conflicts with UE.
// All functions use const char* instead of TCHAR*.

typedef void* STORMLIB_HANDLE;
typedef unsigned int STORMLIB_UINT32;

#ifdef __cplusplus
extern "C" {
#endif

bool StormLib_OpenArchive(const char* Path, STORMLIB_HANDLE* OutHandle);
void StormLib_CloseArchive(STORMLIB_HANDLE Handle);
bool StormLib_OpenFile(STORMLIB_HANDLE Archive, const char* FileName, STORMLIB_HANDLE* OutFile);
void StormLib_CloseFile(STORMLIB_HANDLE File);
STORMLIB_UINT32 StormLib_GetFileSize(STORMLIB_HANDLE File);
bool StormLib_ReadFile(STORMLIB_HANDLE File, void* Buffer, STORMLIB_UINT32 Size, STORMLIB_UINT32* BytesRead);

#ifdef __cplusplus
}
#endif
