#include <map>
#include <vector>
#include "wordFilter.h"
#include <boost/tokenizer.hpp>
#include <iostream>
#include <fstream>
#include <string>
#include <set>
#include <algorithm>

#pragma execution_character_set("utf-8") 

void makeFilterFile(const char *fpath, const char *outfile)
{
	typedef boost::tokenizer<boost::char_separator<char>> TokenizerType;

	std::map<short, int> wordIdxMap;
	std::map<short, int> initialCharSet;
	std::map<std::wstring, int> words;

	std::map<short, std::vector<std::wstring>*> endList;
	std::set<short> endsInMiddle;
	std::set<short> shownInMiddle;

	boost::char_separator<char> charSep(", \r\n");

	std::ifstream inputFile(fpath, std::ios_base::binary);
	if (!inputFile.is_open())
	{
		std::cout << "fuck!!!\n";
		return;
	}
	inputFile.seekg(0, std::ios_base::end);
	int fileSize = inputFile.tellg();
	inputFile.seekg(0, std::ios_base::beg);
	std::string data(fileSize, 0);
	if (!inputFile.read(&data[0], fileSize))
	{
		std::cout << "read file error!!!\n";
		inputFile.close();
		return;
	}
	inputFile.close();

	TokenizerType tok(data, charSep);
	for (auto iter = tok.begin(); iter != tok.end(); ++iter)
	{
		std::string w = *iter;
		const char *pStr = w.c_str();
		int n = w.length();
		std::wstring ws;
		for (int i = 0; i < n;)
		{
			int b;
			short c = utf2unicode(pStr+i, &b);
			wordIdxMap[c] = 0;
			ws += c;
			i += b;
		}
		initialCharSet[ws[0]] |= (ws.size() == 1) | 0x2;
		words[std::move(ws)] += 1;
	}
	std::vector<short> chars;
	for (auto iter : wordIdxMap) {
		wordIdxMap[iter.first] = chars.size();
		chars.push_back(iter.first);
	}
	std::cout << "total chars : " << chars.size() << std::endl;

	std::vector<std::map<int, SJumpTableEntry>*> jumpEntries(wordIdxMap.size());
	std::vector<SJumpTable> jumpTables(wordIdxMap.size());

	std::cout << "total words : " << words.size() << std::endl;

	int singleCharCount = 0;
	for (auto &key : words)
	{
		std::wstring word = key.first;
		int n = word.length();
		if (n == 1)
		{
			singleCharCount++;
			continue;
		}
		n--;
		int idx, idxNext;
		for (int i = 0; i < n; ++i) {
			idx = wordIdxMap[word[i]];
			if (jumpEntries[idx] == nullptr)
			{
				jumpEntries[idx] = new std::map<int, SJumpTableEntry>();
			}
			idxNext = wordIdxMap[word[i + 1]];

			auto iter = jumpEntries[idx]->find(idxNext);
			if (iter != jumpEntries[idx]->end())
			{
				SJumpTableEntry jte = iter->second;
				if (jte.charIndex != idxNext)
				{
					std::cout << "opps index not equal!!" << jte.charIndex << " : " << idxNext << ".\n";
					return;
				}
				jte.flags |= 1 << (i + 1);
				if (i > 14)
				{
					std::cout << "now the longest string can only be 14 characters long\n";
					return;
				}

				if (i + 1 == n)
				{
					jte.flags |= 1;
					auto iter = endList.find(word[i + 1]);
					if (iter == endList.end())
					{
						auto newList = new std::vector<std::wstring>();
						endList[word[i + 1]] = newList;

						newList->push_back(word);
					}
					else
					{
						iter->second->push_back(word);
					}
					if (shownInMiddle.find(word[i + 1]) != shownInMiddle.end())
						endsInMiddle.insert(word[i + 1]);
				}
				else if (jte.flags & 1) {
					endsInMiddle.insert(word[i + 1]);
				}
				else {
					shownInMiddle.insert(word[i + 1]);
				}
				(*jumpEntries[idx])[idxNext] = jte;
			}
			else {
				SJumpTableEntry jte;
				jte.charIndex = idxNext;
				jte.flags = 1 << (i + 1);
				if (i > 14)
				{
					std::cout << "now the longest string can only be 14 characters long\n";
					return;
				}
				if (i + 1 == n)
				{
					jte.flags |= 1;
					auto iter = endList.find(word[i + 1]);
					if (iter == endList.end())
					{
						auto newList = new std::vector<std::wstring>();
						endList[word[i + 1]] = newList;

						newList->push_back(word);
					}
					else
					{
						iter->second->push_back(word);
					}
				}
				(*jumpEntries[idx])[idxNext] = jte;
			}
		}
	}

	int n = 0;
	/*
	for (auto iter : endList) {
		n += iter.second->size();
	}
	std::cout << "n = " << n << ", sigle char count = " << singleCharCount << std::endl;
	*/

	// sort using a custom function object
	struct customComparar {
	private:
		const std::vector<short> &_chars;

	public:
		customComparar(std::vector<short> &chars): _chars(chars){}
		bool operator()(SJumpTableEntry &a, SJumpTableEntry &b)
		{
			return _chars[a.charIndex] < _chars[b.charIndex];
		}
	};


	std::ofstream of(outfile, std::ios_base::binary);
	int off = sizeof(SHeader);
	of.seekp(off);


	customComparar cmp(chars);
	std::vector<SJumpTableEntry> allEntries;
	for (unsigned i = 0; i < jumpTables.size(); ++i) {
		jumpTables[i].nTotalEntries = jumpEntries[i] ? jumpEntries[i]->size() : 0;
		jumpTables[i].flags = initialCharSet[chars[i]];  // allowed single character
		if (jumpTables[i].nTotalEntries != 0)
		{
			int start = allEntries.size();
			for (auto iter : *jumpEntries[i]) {
				allEntries.push_back(iter.second);
			}

			std::sort(allEntries.begin()+ start, allEntries.end(), cmp);
			jumpTables[i].entryOff = start * sizeof(SJumpTableEntry) + off;
		}
		else
		{
			jumpTables[i].entryOff = 0;
		}
	}
	of.write((const char*)&allEntries[0], allEntries.size() * sizeof(SJumpTableEntry));

	of.write((const char*)&chars[0], chars.size() * sizeof(short));
	of.write((const char*)&jumpTables[0], jumpTables.size() * sizeof(SJumpTable));

	off = of.tellp();

	struct customComparar2 {
	public:
		bool operator()(std::wstring &a, std::wstring &b)
		{
			return a.length() < b.length();
		}
	};

	std::vector<SStrings> strings;
	customComparar2 cmp2;
	for (auto iter : endsInMiddle)
	{
		auto l = endList[iter];
		std::sort(l->begin(), l->end(), cmp2);
		
		for (auto &iter2 : *l)
		{
			of.write((const char*)iter2.c_str(), iter2.length() * sizeof(wchar_t));

			SStrings s;
			s.off = off;
			s.length = iter2.length() * sizeof(wchar_t);
			off += s.length;
			strings.push_back(s);
		}
	}
	int stringTableOff = of.tellp();
	of.write((const char*)&strings[0], strings.size() * sizeof(SStrings));
	off = stringTableOff;

	std::vector<SEndTable> endTable(endsInMiddle.size());
	int i = 0;
	for (auto iter: endsInMiddle)
	{
		auto l = endList[iter];
		endTable[i].charIndex = wordIdxMap[iter];
		endTable[i].length = l->size();
		endTable[i].off = off;
		off += l->size() * sizeof(SStrings);
		++i;
	}
	int endTableOff = of.tellp();
	of.write((const char*)&endTable[0], endTable.size() * sizeof(SEndTable));

	char buf[128];
	n = wordIdxMap.size();
	memcpy(buf, &n, 4); // char count
	n = sizeof(SHeader) + allEntries.size() * sizeof(SJumpTableEntry);
	memcpy(buf + 4, &n, 4);   // char offset
	n += chars.size() * sizeof(short);
	memcpy(buf + 8, &n, 4);  // jump table offset
	memcpy(buf + 12, &endTableOff, 4);
	memcpy(buf + 16, &i, 4);
	of.seekp(0);
	of.write(buf, sizeof(SHeader));


	of.close();
}

int main(int argc, char *argv[])
{
	//makeFilterFile("aa.txt", "badWord.bytes");
	initWordFilter("badWord.bytes");
	//const char *text = "对对草泥马的fuck you日你哦";
	const char *text = "毛茉莉花蚕泽东";
	int ret = filterWord(text, strlen(text));
	std::cout << ret;
	return 0;
}