#include <vector>
#include <string>
#include <map>
#include <iomanip>
#include "error.h"
#include "linkerStructures.h"

using namespace std;

class Linker {
public:

	void proccess(vector<string> entryFiles);

	void write(string outputfile, bool hex, bool relocateable, map<string, unsigned short> places);

	void writeRelocateable(fstream& output);

	void writeRelocateableTxt(fstream& output);

	map<string, vector<unsigned char>> sections;
	vector<stEntry*> symbolTable;
	vector<shEntry*> sectionHeaders;
	vector<rtEntry*> relocationsTable;
	vector<string> stringTable;

	vector<unsigned char> finalBytes;

	stEntry* findSymbol(string name) {
		for (stEntry* symbol : symbolTable)
			if (symbol->name == name) return symbol;
		return nullptr;
	}

	shEntry* findHeader(string name) {
		for (shEntry* header : sectionHeaders)
			if (header->name == name) return header;
		return nullptr;
	}

	vector<rtEntry*> findRelocsFromSection(shEntry* section) {
		vector<rtEntry*> relocs;
		for (rtEntry* rt : relocationsTable)
			if (rt->section == section) relocs.push_back(rt);
		return relocs;
	}

};