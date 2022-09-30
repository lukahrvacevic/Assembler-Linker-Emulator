#include "parser.h"
#include "assemblerStructures.h"

using namespace std;

class Assembler {

public:

    Assembler() {
        locationCounter = 0;
        sectionLocationCounter = 0;
        currentSection = nullptr;
        currentSectionHeader = nullptr;
    }

    ~Assembler(){
        for(shEntry* sectionHeader : sectionHeaderTable) delete sectionHeader;
        for(rtEntry* reloc : relocationsTable) delete reloc;
        for(stEntry* symbol : symbolTable){
            symbol->deletePatches();
            delete symbol;
        }
    }

    void assemble(string inputfile, bool debug = false);

    void write(string outputfile);

    void writeTxt(string outputFile);

    vector<rtEntry*> findRelocsFromSection(shEntry* section) {
        vector<rtEntry*> relocs;
        for (rtEntry* reloc : relocationsTable) {
            if (reloc->section == section) relocs.push_back(reloc);
        }
        return relocs;
    }


private:

    stEntry* findSymbol(string symbol) {
        for (stEntry* entry : symbolTable) {
            if (entry->name == symbol) return entry;
        }
        return nullptr;
    }

    vector<string> stringTable;
    vector<shEntry*> sectionHeaderTable;
    vector<stEntry*> symbolTable;
    vector<rtEntry*> relocationsTable;
    vector<unsigned char> bytes;

    unsigned short locationCounter;
    stEntry* currentSection;
    shEntry* currentSectionHeader;
    unsigned short sectionLocationCounter;
};