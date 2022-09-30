#include "linker.h"

void Linker::proccess(vector<string> entryFiles) {

	for (string file : entryFiles) {
		fstream input;
		input.open(file, ios::in | ios::binary);
		if (!input.is_open()) ERROR("Greska: Naveden nepostojeci fajl %s", file.c_str());

		mainHeader* header = new mainHeader();
		header->readBinary(input);

		int offset = header->binarySize();
		vector<unsigned char> localBytes;
		while (offset != header->shoff) {
			unsigned char i = 0;
			input.read((char*)&i, sizeof(unsigned char));
			localBytes.push_back(i);
			offset++;
		}

		vector<shEntry*> localSectionHeaders;
		for (int i = 0; i < header->shsize; i++) {
			shEntry* header = new shEntry();
			header->readBinary(input);
			localSectionHeaders.push_back(header);
		}

		vector<stEntry*> localSymbolTable;
		for (int i = 0; i < header->symtabsize; i++) {
			stEntry* symbol = new stEntry();
			symbol->readBinary(input);
			localSymbolTable.push_back(symbol);
		}

		vector<string> localStringTable;
		for (int i = 0; i < header->strtabsize; i++) {
			string s = "";
			char c = 0;
			input.read(&c, sizeof(char));
			while (c != 0) {
				s += c;
				input.read(&c, sizeof(char));
			}
			localStringTable.push_back(s);
		}

		vector<rtEntry*> localRelocsTable;
		for (int i = 0; i < header->relsize; i++) {
			rtEntry* reloc = new rtEntry();
			reloc->readBinary(input);
			localRelocsTable.push_back(reloc);
		}

		for (shEntry* sh : localSectionHeaders) sh->name = localStringTable[sh->nameIndex];

		for (stEntry* st : localSymbolTable) st->name = localStringTable[st->nameIndex];

		for (stEntry* st : localSymbolTable){
			if ((st->defined and !st->isSection) or (st->equ and st->sectionIndex > -1)) st->section = localSymbolTable[st->sectionIndex];
			if (st->equ and !st->section) st->defined = true;		
		}
		for (rtEntry* rt : localRelocsTable) {
			rt->symbol = localSymbolTable[rt->symbolIndex];
			string sectionName = localSymbolTable[rt->sectionIndex]->name;
			for (shEntry* sh : localSectionHeaders)
				if (sh->name == sectionName) { rt->section = sh; break; }
		}

		for (shEntry* sh : localSectionHeaders) {
			vector<unsigned char> sectionBytes;
			for (int i = sh->offset; i < sh->offset + sh->size; i++) sectionBytes.push_back(localBytes[i]);
			stEntry* symbolForSection = localSymbolTable[sh->index];

			shEntry* section = findHeader(sh->name);

			if (section == nullptr) {
				sections.insert(pair < string, vector<unsigned char>> (sh->name, sectionBytes));
				sectionHeaders.push_back(sh);
				sh->index = symbolTable.size();
				symbolTable.push_back(symbolForSection);
				symbolForSection->nameIndex = stringTable.size();
				sh->nameIndex = stringTable.size();
				stringTable.push_back(sh->name);

				for (stEntry* st : localSymbolTable) {
					if (st->section != nullptr and st->section == symbolForSection and st->global) {
						stEntry* symbol = findSymbol(st->name);
						if (symbol != nullptr) {
							if (symbol->defined) ERROR("Greska: Visestruka definicija simbola %s", st->name.c_str());
							symbol->defined = true;
							symbol->value = st->value;
							symbol->isSection = st->isSection;
							symbol->section = symbolForSection;
							for (rtEntry* rt : localRelocsTable)
								if (rt->symbol == st) rt->symbol = symbol;
						}
						else {
							st->index = symbolTable.size();
							symbolTable.push_back(st);
							st->nameIndex = stringTable.size();
							stringTable.push_back(st->name);
						}
					}
				}
			}

			else {
				stEntry* newSymbolForSection = symbolTable[section->index];

				for (rtEntry* rt : localRelocsTable) {
					if (rt->section == sh) {
						rt->section = section;
						rt->offset += section->size;
					}
					if (rt->symbol == symbolForSection) {
						rt->addend += section->size;
						rt->symbol = newSymbolForSection;
					}
				}

				for (stEntry* st : localSymbolTable) {
					if (st->section != nullptr and st->section == symbolForSection and st->global) {
						stEntry* symbol = findSymbol(st->name);
						if (symbol != nullptr) {
							if (symbol->defined) ERROR("Greska: Visestruka definicija simbola %s", st->name.c_str());
							symbol->defined = true;
							symbol->value = st->value + section->size;
							symbol->isSection = st->isSection;
							symbol->section = newSymbolForSection;
							for (rtEntry* rt : localRelocsTable)
								if (rt->symbol == st) rt->symbol = symbol;
						}
						else {
							st->index = symbolTable.size();
							symbolTable.push_back(st);
							st->nameIndex = stringTable.size();
							stringTable.push_back(st->name);
							st->section = newSymbolForSection;
							st->value += section->size;
						}
					}
				}

				vector<unsigned char> bytes = sections[section->name];
				for (auto byte : localBytes) bytes.push_back(byte);
				sections[section->name] = bytes;
				section->size += sh->size;

			}
		}

		for (stEntry* st : localSymbolTable)
			if (st->equ and st->global){
				if(st->section){
					stEntry* s = findSymbol(st->section->name);
					if (s) st->section = s;
				}
				else st->defined = true;

				stEntry* s = findSymbol(st->name);

				if (s == nullptr) {
					st->index = symbolTable.size();
					symbolTable.push_back(st);
					st->nameIndex = stringTable.size();
					stringTable.push_back(st->name);
				}
				else {
					if(st->section) s->section = st->section;
					else s->defined = true;
					s->value = st->value;
					s->equ = true;
				}
			}

		for (stEntry* st : localSymbolTable) {
			if (!st->defined and st->global and !st->equ) {
				stEntry* symbol = findSymbol(st->name);

				if (symbol == nullptr) {
					st->index = symbolTable.size();
					symbolTable.push_back(st);
					st->nameIndex = stringTable.size();
					stringTable.push_back(st->name);
				}
				else {
					for (rtEntry* rt : localRelocsTable)
						if (rt->symbol == st) rt->symbol = symbol;
				}
			}
		}

		for (rtEntry* rt : localRelocsTable) relocationsTable.push_back(rt);

		for (int i = 0; i < symbolTable.size(); i++) symbolTable[i]->index = i;

		input.close();
	}

}

void Linker::write(string outputfile, bool hex, bool relocateable, map<string, unsigned short> places) {
	if (!hex and !relocateable) return;

	if (outputfile == "") {
		if (hex) outputfile = "output.hex";
		else outputfile = "output.o";
	}

	string textfile = "";
	if (outputfile.find(".") != string::npos) textfile = outputfile.substr(0, outputfile.find("."));
	else textfile = outputfile;

	if (hex) textfile += ".formathex";
	else textfile += ".debug";

	fstream outputBinary, outputTxt;

	if (hex) {
		for (stEntry* st : symbolTable){
			if (st->equ and st->section) {st->value += (st->isSection ? -st->section->value : st->section->value); st->defined = true;}
			if (!st->defined) ERROR("Greska: Ostao je nerazresen simbol %s", st->name.c_str());
		}

		vector<shEntry*> sortedSections;

		int lastSection = 0;

		map<string, unsigned short>::iterator it;
		for (it = places.begin(); it != places.end(); it++)
		{
			shEntry* section = findHeader("." + it->first);
			if (section == nullptr) ERROR("Nije pronadjena sekcija %s koja je navedena u place opciji.", it->first.c_str());
			
			stEntry* symbolForSection = findSymbol(section->name);

			unsigned short place = it->second;

			if (place + section->size > lastSection) lastSection = place + section->size;

			section->offset = place;

			if (sortedSections.empty()) sortedSections.push_back(section);
			else {
				auto index = sortedSections.begin();

				bool placed = false;

				for (int i = 0; i < sortedSections.size(); i++) {
					shEntry* contender = sortedSections[i];
					if ((contender->offset <= place and (contender->offset + contender->size) > place) or (place <= contender->offset and (place + section->size) > contender->offset)) ERROR("Greska: Preklapanje izmedju sekcija %s i %s.", section->name.c_str(), contender->name.c_str());
					
					if (place < contender->offset) {
						sortedSections.insert(index, section);
						placed = true;
						break;
					}

					index++;
				}

				if (!placed) sortedSections.push_back(section);

			}

			auto iter = sectionHeaders.begin();
			while (*iter != section) iter++;
			sectionHeaders.erase(iter);

			findSymbol(section->name)->value = place;

			for (stEntry* st : symbolTable)
				if (st->section != nullptr and st->section->name == section->name) st->value += section->offset;

			for (rtEntry* rt : relocationsTable) {
				if (rt->section == section) rt->offset += section->offset; 
			}

		}

		for (shEntry* section : sectionHeaders) {
			section->offset = lastSection;
			lastSection += section->size;
			sortedSections.push_back(section);

			findSymbol(section->name)->value = section->offset;

			for (stEntry* st : symbolTable)
				if (st->section != nullptr and st->section->name == section->name) st->value += section->offset;
			for (rtEntry* rt : relocationsTable)
				if (rt->section == section) rt->offset += section->offset;
		}

		int currentLocation = 0;

		for (shEntry* section : sortedSections) {
			while (currentLocation < section->offset) {
				finalBytes.push_back(0); 
				currentLocation++;
			}

			vector<unsigned char> sectionBytes = sections[section->name];

			for (auto byte : sectionBytes) finalBytes.push_back(byte);

			currentLocation += section->size;
		}

		for (rtEntry* reloc : relocationsTable) {
			if (reloc->type == REL_LE) {
				unsigned short word = reloc->symbol->value * (reloc->minus ? -1 : 1);
				word += reloc->addend;

				finalBytes[reloc->offset] = word & 0xFF;
				finalBytes[reloc->offset + 1] = word >> 8;
			}
			else if (reloc->type == REL_BE) {
				unsigned short word = reloc->symbol->value * (reloc->minus ? -1 : 1);
				word += reloc->addend;

				finalBytes[reloc->offset] = word >> 8;
				finalBytes[reloc->offset + 1] = word & 0xFF;
			}
			else {
				unsigned short word = reloc->symbol->value;
				word += reloc->addend;
				word -= reloc->offset;

				finalBytes[reloc->offset] = word >> 8;
				finalBytes[reloc->offset + 1] = word & 0xFF;
			}
		}

		int counter = 0;

		outputBinary.open(outputfile, ios::out | ios::binary);
		outputTxt.open(textfile, ios::out);

		for (auto byte : finalBytes) {
			if (counter % 8 == 0) {
				if (counter != 0) outputTxt << endl;
				outputTxt << std::hex << setw(4) << setfill('0') << unsigned(counter) << ": ";
			}

			outputTxt << std::hex << setw(2) << setfill('0') << unsigned(byte) << " ";

			outputBinary.write((char*)&byte, sizeof(char));

			counter++;
		}
	}

	if (relocateable) {
		for (stEntry* symbol : symbolTable)
			if (symbol->global and !symbol->defined) symbol->labelledExtern = true;

		int counter = 0;

		for (shEntry* section : sectionHeaders) {
			section->offset = counter;
			counter += section->size;

			vector<unsigned char> bytes = sections[section->name];

			for (auto byte : bytes) finalBytes.push_back(byte);
		}

		outputBinary.open(outputfile, ios::out | ios::binary);
		outputTxt.open(textfile, ios::out);

		writeRelocateable(outputBinary);
		writeRelocateableTxt(outputTxt);
	}



	outputBinary.close();
	outputTxt.close();

}


void Linker::writeRelocateable(fstream& output) {

	mainHeader* header = new mainHeader();

	header->shoff = mainHeader::binarySize() + finalBytes.size();
	header->shsize = sectionHeaders.size();
	header->symtaboff = header->shoff + header->shsize * shEntry::binarySize();
	header->symtabsize = symbolTable.size();
	header->strtaboff = header->symtaboff + header->symtabsize * stEntry::binarySize();
	header->strtabsize = stringTable.size();
	int strtabbinarysize = 0;
	for (string s : stringTable) strtabbinarysize += (s.size() + 1);
	header->reloff = header->strtaboff + strtabbinarysize;
	header->relsize = relocationsTable.size();

	header->writeBinary(output);

	for (unsigned char byte : finalBytes) output.write((char*)&byte, sizeof(unsigned char));

	for (shEntry* section : sectionHeaders) section->writeBinary(output);

	for (stEntry* symbol : symbolTable) symbol->writeBinary(output);

	for (string s : stringTable) {
		for (int i = 0; i < s.size(); i++) {
			unsigned char p = s[i];
			output.write((char*)&p, sizeof(unsigned char));
		}
		unsigned char p = 0;
		output.write((char*)&p, sizeof(unsigned char));
	}

	for (rtEntry* reloc : relocationsTable) reloc->writeBinary(output);

	delete header;

	output.close();
}

void Linker::writeRelocateableTxt(fstream& output) {

	output << "Symbols: " << endl;
	output << "NUM\tNAME\tVALUE\tTYPE\tGLOB\tNDX" << endl;

	for (stEntry* symbol : symbolTable) {
		output << symbol->index + 1 << "\t" << symbol->name << "\t" << hex << setw(4) << setfill('0') << (unsigned)symbol->value << "\t" << (symbol->isSection ? "SCTN" : "NOTYP") << "\t" << symbol->global << "\t";
		if (symbol->isSection) output << symbol->index + 1;
		else if (symbol->labelledExtern or symbol->equ) output << "UND";
		else output << symbol->section->index + 1;
		output << endl;
	}

	output << endl << endl;

	output << "Sections: " << endl;

	for (shEntry* section : sectionHeaders) {
		output << "section " << section->name << ":";

		int i = section->offset;
		int counter = 0;

		while (i < section->offset + section->size) {
			if (counter % 8 == 0) {
				output << endl << hex << setw(4) << setfill('0') << unsigned(counter) << ": ";
			}

			output << hex << setw(2) << setfill('0') << unsigned(finalBytes[i]) << " ";
			i++; counter++;
		}

		output << endl;

		output << "rela." << section->name << ": " << endl;

		vector<rtEntry*> relocs = findRelocsFromSection(section);

		output << "OFFSET\tTYPE\tADDEND\tSYMBOL" << endl;

		for (rtEntry* reloc : relocs) {
			output << hex << setw(4) << setfill('0') << (unsigned)reloc->offset << "\t";
			if (reloc->type == REL_LE) output << "REL_LE\t";
			else if (reloc->type == REL_BE) output << "REL_BE\t";
			else output << "REL_PC\t";
			output << hex << setw(4) << setfill('0') << (unsigned)reloc->addend << "\t" << reloc->symbol->name << endl;
		}
	}


	output.close();
}