#include "wordFilter.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SWordFilter gWordFilterHandle;



static
int utf8prevLen(const char *s)
{
	int ret = 0;
	while ((*s-- & 0xc0) == 0x80)
	{
		ret++;
	}


	return ret + 1;
}

void initWordFilter(const char *filepath)
{
	FILE *fp = fopen(filepath, "rb");
	fseek(fp, 0, SEEK_END);
	int fsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	char *wordFilter = (char*)malloc(fsize);
	gWordFilterHandle.fileSize = fsize;
	gWordFilterHandle.pHeader = (SHeader*)wordFilter;

	fread(wordFilter, fsize, 1, fp);
	fclose(fp);
}
void initWordFilterWithData(const char *data, int fsize)
{
	if (gWordFilterHandle.pHeader != nullptr)
		free(gWordFilterHandle.pHeader);

	char *tmp = (char*)malloc(fsize);
	memcpy(tmp, data, fsize);

	gWordFilterHandle.fileSize = fsize;
	gWordFilterHandle.pHeader = (SHeader*)tmp;
}

void destroyWordFilter()
{
	free(gWordFilterHandle.pHeader);
	gWordFilterHandle.pHeader = nullptr;
}


static SJumpTable *getJumpTable(int charIdx) {
	if (charIdx >= gWordFilterHandle.pHeader->charCount)
		return NULL;
	SJumpTable *table = (SJumpTable*)((char*)gWordFilterHandle.pHeader + gWordFilterHandle.pHeader->jumpTableArrayOff);

	return table + charIdx;
}

static
SJumpTableEntry *getJumpTableEntry(int charIdx, int *n) {
	SJumpTable *table = getJumpTable(charIdx);
	if (table == NULL)
		return NULL;

	SJumpTableEntry *entry = (SJumpTableEntry*)(table->entryOff + (char*)gWordFilterHandle.pHeader);

	if (n)
		*n = table->nTotalEntries;

	return entry;
}

static
int searchChar(unsigned short c)
{
	unsigned short *pChars = (unsigned short*)((char*)gWordFilterHandle.pHeader + gWordFilterHandle.pHeader->charOff);
	int nChars = gWordFilterHandle.pHeader->charCount;
	int mid = nChars>>1;
	int s = 0;
	int end = nChars-1;
	while(1) {
		if (s > end)
			return -1;
		if (pChars[mid] == c)
		{
			SJumpTable *jt = getJumpTable(mid);
			// only the initial character can be searched for first char
			if (jt->flags & 0x2)
				return mid;
			else
				return -1;
		}
		else if(pChars[mid] < c) {
			s = mid+1;
			mid = s + ((end+1-s)>>1);
		} else if(pChars[mid] > c) {
			end = mid-1;
			mid = s + ((mid-s)>>1);
		}
	}

	return -1;
}

template<typename T>
int searchCharIndirect(T *entries, int nEntries, unsigned short c, T **entry)
{
	unsigned short *pChars = (unsigned short*)((char*)gWordFilterHandle.pHeader + gWordFilterHandle.pHeader->charOff);
	int s = 0, end = s + nEntries - 1;
	int mid = s + (nEntries >> 1);
	if (entry)
		*entry = nullptr;
	while (1) {
		if (s>end)
		{
			return -1;
		}
		int curChar = pChars[entries[mid].charIndex];
		if (curChar == c) {
			if (entry)
				*entry = &entries[mid];
			return entries[mid].charIndex;
		}
		else if (curChar < c) {
			s = mid + 1;
			mid = s + ((end + 1 - s) >> 1);
		}
		else if (curChar > c) {
			end = mid - 1;
			mid = s + ((mid - s) >> 1);
		}
	}

	return -1;
}

static
int searchCharIndirect(SJumpTable *jumpTable, unsigned short c, int depth, SJumpTableEntry **entry)
{
	SJumpTableEntry *entries = (SJumpTableEntry*)((char*)gWordFilterHandle.pHeader + jumpTable->entryOff);
	int nEntries = jumpTable->nTotalEntries;

	int idx = searchCharIndirect<SJumpTableEntry>(entries, nEntries, c, entry);
	if (idx != -1 && (**entry).flags & (1 << (depth)))
		return idx;
	return -1;
}

static
bool checkEnds(const char *s, int startIdx, int depth)
{
	SEndTable *t = (SEndTable*)((char*)gWordFilterHandle.pHeader + gWordFilterHandle.pHeader->endTableOff);
	unsigned short c = utf2unicode(s + startIdx, nullptr);
	SEndTable *cur;
	int idx = searchCharIndirect<SEndTable>(t, gWordFilterHandle.pHeader->endTableLen, c, &cur);
	if(idx == -1)
		return false;

	SStrings *ss = (SStrings*)((char*)gWordFilterHandle.pHeader + cur->off);
	unsigned short buf[32];
	memset(buf, 0, sizeof(buf));

	char *startAddr = (char*)gWordFilterHandle.pHeader;

	for (int i = 0; i < cur->length; ++i)
	{
		SStrings *curs = ss + i;
		int l = curs->length >> 1;
		if (l < depth)
			continue;
		else if (l > depth)
			break;

		bool equal = true;
		unsigned short *pStr = (unsigned short*)(startAddr + curs->off);
		for (int j = depth - 1; j >= 0; --j)
		{
			if (buf[j] == 0)
			{
				buf[j] = utf2unicode(s + startIdx, nullptr);
				startIdx -= utf8prevLen(s + startIdx - 1);
			}
			if (buf[j] != pStr[j])
			{
				equal = false;
				break;
			}
		}
		if (equal)
			return true;
	}

	return false;
}

unsigned short utf2unicode(const char *str, int *n)
{
	char cur = *str;
	if ((cur & 0x80) == 0)
	{
		if (n)
			*n = 1;
		return cur;
	}
	if ((cur & 0xe0) == 0xc0)
	{
		if (n)
			*n = 2;
		short ret = (cur&~0xe0) << 6;
		ret |= (*(str + 1) & 0x3f);
		return ret;
	}
	else if ((cur & 0xf0) == 0xe0)
	{
		if (n)
			*n = 3;
		short ret = (cur&~0xf0) << 12;
		cur = *(str + 1);
		ret |= (cur & 0x3f) << 6;
		ret |= (*(str + 2) & 0x3f);
		return ret;
	}
	return 0;
}

int utf8charsize(short c)
{
	if ((unsigned short)c <= 0x3f)
		return 1;
	if ((unsigned short)c <= 0x7ff)
		return 2;
	if ((unsigned short)c <= 0xffff)
		return 3;
	return 0;
}

int filterWord(const char *str, int n)
{
	if (gWordFilterHandle.pHeader == nullptr)
		return -1;
	int depth = 0;
	SJumpTable *table = NULL;
	int s = 0;
	SJumpTableEntry *lastEntry = NULL;
	int nChars = 0;
	int nLastAdvance = 0;
	for(int i=0;i<n;)
	{
		int advance=0;
		short curChar = utf2unicode(str+ i, &advance);

		int idx;
		SJumpTableEntry *entry = nullptr;
		if(table == NULL)
			idx = searchChar(curChar);
		else
			idx = searchCharIndirect(table, curChar, depth, &entry);
		if(idx == -1)
		{
			if(depth > 0)
			{
				int flags = (1 << (depth-1)) | 1;
				if(lastEntry && ((lastEntry->flags & flags) == flags))
				{
					if (table->nTotalEntries == 0)
						return nChars;
					else if (checkEnds(str, i-nLastAdvance, depth))
						return nChars;
				}
			}
			depth = 0;
			table = NULL;

			utf2unicode(str + s, &advance);
			s += advance;
			nChars++;
			nLastAdvance = 0;

			i = s;
			lastEntry = NULL;
			continue;
		}
		lastEntry = entry;

		i += advance;
		depth++;
		nLastAdvance = advance;

		table = getJumpTable(idx);
		if(table->nTotalEntries == 0)
		{
			return nChars;
		}
	}
	int ret = -1;
	int flags = ((1 << (depth-1)) | 1);
	if (table != nullptr && depth == 1)
		ret = (table->flags & 0x1) ? nChars : -1;
	else if (depth > 0 && lastEntry && (lastEntry->flags & flags) == flags) {
		ret = nChars;
	}
	return ret;
}