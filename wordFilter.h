#pragma once
#ifndef __WORD_FILTER_H__
#define __WORD_FILTER_H__

#if _WIN32 || _WIN64
#define DLLAPI __declspec(dllexport)
#else
#define DLLAPI
#endif

#ifdef __cplusplus
extern "C" {
#endif

	struct SJumpTable
	{
		unsigned short nTotalEntries;
		unsigned short flags;  // hold weather the character can be the first character and weather the character can be a single character
		unsigned entryOff;
	};

	struct SJumpTableEntry
	{
		unsigned short charIndex;
		unsigned short padding;
		int flags; // hold which index the character can appear
	};

	struct SEndTable
	{
		unsigned short charIndex;
		unsigned short length;
		int off;
	};

	struct SStrings
	{
		int off;
		unsigned short length;
	};

	struct SHeader
	{
		int charCount;
		int charOff;
		int jumpTableArrayOff;
		int endTableOff;
		int endTableLen;
	};


	struct SWordFilter
	{
		int fileSize;
		struct SHeader *pHeader;
	};
	
	DLLAPI void initWordFilter(const char *filepath);
	DLLAPI void initWordFilterWithData(const char *data, int fsize);
	DLLAPI void destroyWordFilter();
	DLLAPI int filterWord(const char *str, int n);
	DLLAPI unsigned short utf2unicode(const char *str, int *n);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // !__WORD_FILTER__
