#include "../inc/assembler.h"
#include <iomanip>
#include <sstream>

void Assembler::assemble(string inputfile, bool debug) {
	Parser* p = new Parser(inputfile);
	fstream debugFile;
	if(debug) debugFile.open("debug.txt", ios::out);

	vector<stEntry*> equSyms;
	vector<vector<string>> expressionTokens;
	vector<string> equSymLines;

	while (true) {
		packedLine package = p->nextLine();

		if (debug) {
			debugFile << "label: " << package.label << " directive: " << package.directive << " instruction: " << package.instruction << endl;
			debugFile << "args: ";
			for (auto i : package.args) debugFile << i << " ";
			debugFile << endl;
		}

		if (package.label != "") {
			if (currentSection == nullptr) {
				ERROR("Nije definisana sekcija pre linije %s", package.line.c_str());
			}
			stEntry* symbol = findSymbol(package.label);
			if (symbol != nullptr) {
				if (symbol->defined) ERROR("Greska na liniji %s; vec je definisan simbol.", package.line.c_str());
				if (symbol->labelledExtern) ERROR("Greska na liniji %s; simbol je prozvan eksternim.", package.line.c_str());
			}
			else {
				symbol = new stEntry();
				symbol->name = package.label;
				symbol->nameIndex = stringTable.size();
				stringTable.push_back(package.label);
				symbol->index = symbolTable.size();
				symbolTable.push_back(symbol);
			}
			symbol->defined = true;
			symbol->value = sectionLocationCounter;
			symbol->section = currentSection;
		}

		if (package.directive == ".end") break;
		
		if (package.directive != "") {

			if (package.directive == ".global") {
				for (string name : package.args) {
					if (name[0] == '.') ERROR("Greska na liniji %s; nije moguce markirati sekciju globalnom.", package.line.c_str());
					stEntry* entry = findSymbol(name);
					if (entry == nullptr) {
						stEntry* entry = new stEntry();
						entry->name = name;
						entry->nameIndex = stringTable.size();
						stringTable.push_back(name);
						entry->index = symbolTable.size();
						symbolTable.push_back(entry);
					}
					entry->global = true;
				}
			}

			if (package.directive == ".extern") {
				for (string name : package.args) {
					stEntry* symbol = findSymbol(name);
					if (symbol != nullptr and symbol->defined) ERROR("Greska na liniji %s; pokusaj markiranja vec definisanog simbola kao eksternog.", package.line.c_str());
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = name;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(name);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}
					symbol->labelledExtern = true;
					symbol->global = true;
				}
			}

			if (package.directive == ".section") {
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za direktivu .section na liniji %s", package.line.c_str());
				string name = "." + package.args[0];
				if (findSymbol(name) != nullptr) ERROR("Greska na liniji %s; vec je definisana sekcija.", package.line.c_str());
				shEntry* sh = new shEntry();
				sh->name = name;
				sh->nameIndex = stringTable.size();
				stringTable.push_back(name);
				sh->offset = locationCounter;
				sh->index = symbolTable.size();
				stEntry* st = new stEntry();
				st->name = name;
				st->nameIndex = sh->nameIndex;
				st->defined = true;
				st->isSection = true;
				st->index = sh->index;
				symbolTable.push_back(st);
				sectionHeaderTable.push_back(sh);
				currentSection = st;
				currentSectionHeader = sh;
				sectionLocationCounter = 0;
			}

			if (package.directive == ".word") {
				if (currentSection == nullptr) {
					ERROR("Nije definisana sekcija pre linije %s", package.line.c_str());
				}
				for (string arg : package.args) {
					if (p->isMemLiteral(arg)) {
						unsigned short word = 0;
						if (arg.find("0x") != string::npos) {
							arg = arg.substr(2);
							stringstream ss;
							ss << hex << arg;
							ss >> word;
						}
						else word = stoi(arg);

						bytes.push_back(word & 0xFF);
						bytes.push_back(word >> 8);
					}
					else if (p->isMemSymbol(arg)) {
						stEntry* symbol = findSymbol(arg);
						if (symbol == nullptr) {
							symbol = new stEntry();
							symbol->name = arg;
							symbol->nameIndex = stringTable.size();
							stringTable.push_back(arg);
							symbol->section = currentSection;
							symbol->index = symbolTable.size();
							symbolTable.push_back(symbol);
						}

						if (symbol->equ and symbol->defined){
							bytes.push_back(symbol->value & 0xFF);
							bytes.push_back(symbol->value >> 8);
						}

						else{
							rtEntry* reloc = new rtEntry();
							reloc->offset = sectionLocationCounter;
							reloc->section = currentSectionHeader;
							if(symbol->global or symbol->equ) reloc->symbol = symbol;
							else {
								reloc->symbol = symbol->section;
								reloc->addend = symbol->value;
							}
							relocationsTable.push_back(reloc);

							if (symbol->defined == false) symbol->addRelocToPatch(reloc);

							bytes.push_back(0);
							bytes.push_back(0);
						}
					}
					else ERROR("Nevalidan argument na liniji %s", package.line.c_str());

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}
			}

			if (package.directive == ".skip") {
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za direktivu .skip na liniji %s", package.line.c_str());
				if (currentSection == nullptr) {
					ERROR("Nije definisana sekcija pre linije %s", package.line.c_str());
				}

				if (!p->isMemLiteral(package.args[0])) ERROR("Nevalidan argument na liniji %s", package.line.c_str());

				int num = stoi(package.args[0]);

				for (int i = 0; i < num; i++) bytes.push_back(0);

				locationCounter += num;
				sectionLocationCounter += num;
				currentSectionHeader->size += num;
			}

			if (package.directive == ".ascii") {
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za direktivu .ascii na liniji %s", package.line.c_str());
				if (currentSection == nullptr) {
					ERROR("Nije definisana sekcija pre linije %s", package.line.c_str());
				}
				
				string arg = package.args[0];

				if (!p->isAsciiOperand(arg)) ERROR("Nevalidan argument na liniji %s", package.line.c_str());

				arg = arg.substr(1, arg.size() - 2);

				for (int i = 0; i < arg.size(); i++) bytes.push_back((unsigned char)(arg[i]));

				locationCounter += arg.size();
				sectionLocationCounter += arg.size();
				currentSectionHeader->size += arg.size();
			}

			if (package.directive == ".equ"){
				if (package.args.size() != 2) ERROR("Nevalidan broj argumenata za direktivu .equ na liniji %s", package.line.c_str());

				string symbolName = package.args[0];
				string expression = package.args[1];

				if (!p->isMemSymbol(symbolName)) ERROR("Nevalidan argument na liniji %s", package.line.c_str());
				if (!p->isExpression(expression)) ERROR("Nevalidan argument na liniji %s", package.line.c_str());

				stEntry* symbol = findSymbol(symbolName);
				if (symbol != nullptr) {
					if (symbol->defined) ERROR("Greska na liniji %s; vec je definisan simbol.", package.line.c_str());
					if (symbol->labelledExtern) ERROR("Greska na liniji %s; simbol je prozvan eksternim.", package.line.c_str());
				}
				else{
					symbol = new stEntry();
					symbol->index = symbolTable.size();
					symbolTable.push_back(symbol);
					symbol->nameIndex = stringTable.size();
					stringTable.push_back(symbolName);
					symbol->name = symbolName;
				}

				symbol->equ = true;

				bool hasSymbols = false;
				vector<string> tokens;
				int posplus = 0, posminus = 0;
        		p->trim(expression);
				if (expression[0] == '+' or expression[0] == '-'){
					tokens.push_back(expression.substr(0,1));
					expression = expression.substr(1);
					p->trim(expression);
				}
        		posplus = expression.find("+"); posminus = expression.find("-");
        		while(posplus != string::npos or posminus != string::npos){
					int pos = 0;
					if (posminus == string::npos) pos = posplus;
					else if (posplus==string::npos) pos = posminus;
					else pos = posminus<posplus  ? posminus : posplus;
					string arg = expression.substr(0, pos);
					p->trim(arg);
								if (p->isMemSymbol(arg)) hasSymbols = true;
					tokens.push_back(arg); tokens.push_back(expression.substr(pos, 1));
					expression.erase(0, pos + 1);
					p->trim(expression);
					posplus = expression.find("+"); posminus = expression.find("-");
				}
				if (p->isMemSymbol(expression)) hasSymbols = true;
        		tokens.push_back(expression);

				if (!hasSymbols){
					bool sign = true;

					for(string token : tokens){
						if (token == "+") sign = true;
						else if (token == "-") sign = false;
						else{

							unsigned short word = 0;
							if (token.find("0x") != string::npos) {
								token = token.substr(2);
								stringstream ss;
								ss << hex << token;
								ss >> word;
							}
							else word = stoi(token);

							symbol->value += (sign ? word : -word);
						}
					}
					symbol->defined = true;
				}
			
				else{
					equSyms.push_back(symbol);
					expressionTokens.push_back(tokens);
					equSymLines.push_back(package.line);
				}
			}

		}

		if (package.instruction != "") {

			if (currentSection == nullptr) {
				ERROR("Nije definisana sekcija pre linije %s", package.line.c_str());
			}

			if (package.instruction == "halt") {
				if (package.args.size() != 0) ERROR("Nevalidan broj argumenata za instrukciju halt na liniji %s", package.line.c_str());
				
				bytes.push_back(0);

				locationCounter += 1;
				sectionLocationCounter += 1;
				currentSectionHeader->size += 1;
			}

			if (package.instruction == "int") {
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za instrukciju int na liniji %s", package.line.c_str());

				string reg = package.args[0];

				if (!p->isRegister(reg)) ERROR("Nevalidan argument za instrukciju int na liniji %s", package.line.c_str());

				int num = stoi(reg.substr(1));

				bytes.push_back(0x10);
				bytes.push_back((num << 4) | 0xF);

				locationCounter += 2;
				sectionLocationCounter += 2;
				currentSectionHeader->size += 2;
			}

			if (package.instruction == "iret") {
				if (package.args.size() != 0) ERROR("Nevalidan broj argumenata za instrukciju iret na liniji %s", package.line.c_str());

				bytes.push_back(0x20);

				locationCounter += 1;
				sectionLocationCounter += 1;
				currentSectionHeader->size += 1;
			}

			if (package.instruction == "call") {
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za instrukciju call na liniji %s", package.line.c_str());

				bytes.push_back(0x30);

				string arg = package.args[0];

				if (p->isMemLiteral(arg)) {

					bytes.push_back(0xFF);
					bytes.push_back(0);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isMemSymbol(arg)) {
					bytes.push_back(0xFF);
					bytes.push_back(0);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ or symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{
						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isPCRelSymbol(arg)) {
					arg = arg.substr(1);

					bytes.push_back(0xF7);
					bytes.push_back(0x05);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					rtEntry* reloc = new rtEntry();
					reloc->offset = sectionLocationCounter + 3;
					reloc->section = currentSectionHeader;
					if (symbol->defined and !symbol->global){
						reloc->symbol = symbol->section;
						reloc->addend = symbol->value - 2;
					}
					else{
						reloc->symbol = symbol;
						reloc->addend = -2;
					}
					reloc->type = REL_PC;
					relocationsTable.push_back(reloc);

					if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

					bytes.push_back(0);
					bytes.push_back(0);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPLiteral(arg)) {
					arg = arg.substr(1);

					bytes.push_back(0xFF);
					bytes.push_back(0x04);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPReg(arg)) {
					arg = arg.substr(2);

					int num = stoi(arg);

					bytes.push_back((0xF << 4) | num);
					bytes.push_back(0x1);
				}

				else if (p->isJMPSymbol(arg)) {
					bytes.push_back(0xFF);
					bytes.push_back(0x04);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ or symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{

						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}
				
				else if (p->isJMPRegIndirect(arg)) {
					arg = arg.substr(3);

					int num = stoi(arg);

					bytes.push_back((0xF << 4) | num);
					bytes.push_back(0x2);
				}

				else if (p->isJMPRegIndirectWithLiteral(arg)) {
					string reg = arg.substr(3, arg.find("+") - 2);
					p->trim(reg);
					string number = arg.substr(arg.find("+") + 1);
					p->trim(number);
					number.erase(number.find("]"));

					unsigned short num = stoi(number);
					int regnum = stoi(reg);

					bytes.push_back((0xF << 4) | regnum);
					bytes.push_back(0x3);
					bytes.push_back(num >> 8);
					bytes.push_back(num & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPRegIndirectWithSymbol(arg)) {
					string reg = arg.substr(3, arg.find("+") - 2);
					p->trim(reg);
					string sym = arg.substr(arg.find("+") + 1);
					p->trim(sym);
					sym.erase(sym.find("]"));

					int regnum = stoi(reg);

					bytes.push_back((0xF << 4) | regnum);
					bytes.push_back(0x3);

					stEntry* symbol = findSymbol(sym);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = sym;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(sym);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ and symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{
						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());
				
				locationCounter += 3;
				sectionLocationCounter += 3;
				currentSectionHeader->size += 3;
			}

			if (package.instruction == "ret") {
				if (package.args.size() != 0) ERROR("Nevalidan broj argumenata za instrukciju ret na liniji %s", package.line.c_str());

				bytes.push_back((0x4 << 4));

				locationCounter += 1;
				sectionLocationCounter += 1;
				currentSectionHeader->size += 1;
			}

			if (package.instruction == "jmp" or package.instruction == "jeq" or package.instruction == "jne" or package.instruction == "jgt") {
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za instrukciju %s na liniji %s", package.instruction.c_str(), package.line.c_str());

				if (package.instruction == "jmp") bytes.push_back(0x50);
				else if (package.instruction == "jeq") bytes.push_back(0x51);
				else if (package.instruction == "jne") bytes.push_back(0x52);
				else if (package.instruction == "jgt") bytes.push_back(0x53);

				string arg = package.args[0];

				if (p->isMemLiteral(arg)) {

					bytes.push_back(0xFF);
					bytes.push_back(0);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isMemSymbol(arg)) {
					bytes.push_back(0xFF);
					bytes.push_back(0);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ and symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{

						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isPCRelSymbol(arg)) {
					arg = arg.substr(1);

					bytes.push_back(0xF7);
					bytes.push_back(0x05);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					rtEntry* reloc = new rtEntry();
					reloc->offset = sectionLocationCounter + 3;
					reloc->section = currentSectionHeader;
					if (symbol->defined and !symbol->global){
						reloc->symbol = symbol->section;
						reloc->addend = symbol->value - 2;
					}
					else{
						reloc->symbol = symbol;
						reloc->addend = -2;
					}
					reloc->type = REL_PC;
					relocationsTable.push_back(reloc);

					if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

					bytes.push_back(0);
					bytes.push_back(0);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPLiteral(arg)) {
					arg = arg.substr(1);

					bytes.push_back(0xFF);
					bytes.push_back(0x04);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPSymbol(arg)) {
					bytes.push_back(0xFF);
					bytes.push_back(0x04);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ and symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{
						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPReg(arg)) {
					arg = arg.substr(2);

					int num = stoi(arg);

					bytes.push_back((0xF << 4) | num);
					bytes.push_back(0x1);
				}

				else if (p->isJMPRegIndirect(arg)) {
					arg = arg.substr(3);

					int num = stoi(arg);

					bytes.push_back((0xF << 4) | num);
					bytes.push_back(0x2);
				}

				else if (p->isJMPRegIndirectWithLiteral(arg)) {
					string reg = arg.substr(3, arg.find("+") - 2);
					p->trim(reg);
					string number = arg.substr(arg.find("+") + 1);
					p->trim(number);
					number.erase(number.find("]"));

					unsigned short num = stoi(number);
					int regnum = stoi(reg);

					bytes.push_back((0xF << 4) | regnum);
					bytes.push_back(0x3);
					bytes.push_back(num >> 8);
					bytes.push_back(num & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isJMPRegIndirectWithSymbol(arg)) {
					string reg = arg.substr(3, arg.find("+") - 2);
					p->trim(reg);
					string sym = arg.substr(arg.find("+") + 1);
					p->trim(sym);
					sym.erase(sym.find("]"));

					int regnum = stoi(reg);

					bytes.push_back((0xF << 4) | regnum);
					bytes.push_back(0x3);

					stEntry* symbol = findSymbol(sym);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = sym;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(sym);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->defined and symbol->equ){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{

						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				locationCounter += 3;
				sectionLocationCounter += 3;
				currentSectionHeader->size += 3;
			}

			if (package.instruction == "push") {
				//str regd, [r6], dekrementiranje dva put pre mem16[r6] <= regd
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za instrukciju push na liniji %s", package.line.c_str());
			
				string arg = package.args[0];

				if (!p->isRegister(arg)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				int num = stoi(arg.substr(1));

				bytes.push_back(0xb << 4);
				bytes.push_back((num << 4) | 0x6);
				bytes.push_back((0x1 << 4) | 0x2);

				locationCounter += 3;
				sectionLocationCounter += 3;
				currentSectionHeader->size += 3;
			}

			if (package.instruction == "pop") {
				//ldr regd, [r6], inkrementiranje dva put posle regd <= mem16[r6]
				if (package.args.size() != 1) ERROR("Nevalidan broj argumenata za instrukciju pop na liniji %s", package.line.c_str());

				string arg = package.args[0];

				if (!p->isRegister(arg)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				int num = stoi(arg.substr(1));

				bytes.push_back(0xa << 4);
				bytes.push_back((num << 4) | 0x6);
				bytes.push_back((0x4 << 4) | 0x2);

				locationCounter += 3;
				sectionLocationCounter += 3;
				currentSectionHeader->size += 3;
			}

			if (package.instruction == "xchg") {
				if (package.args.size() != 2) ERROR("Nevalidan broj argumenata za instrukciju xchg na liniji %s", package.line.c_str());
			
				string regd = package.args[0];
				string regs = package.args[1];

				if(!p->isRegister(regd) or !p->isRegister(regs)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());
			
				int reg1 = stoi(regd.substr(1));
				int reg2 = stoi(regs.substr(1));

				bytes.push_back(0x60);
				bytes.push_back((reg1 << 4) | reg2);

				locationCounter += 2;
				sectionLocationCounter += 2;
				currentSectionHeader->size += 2;
			}
		
			if (package.instruction == "add" or package.instruction == "sub" or package.instruction == "mul" or package.instruction == "div" or package.instruction == "cmp")  {
				if (package.args.size() != 2) ERROR("Nevalidan broj argumenata za instrukciju %s na liniji %s", package.instruction.c_str(), package.line.c_str());

				string regd = package.args[0];
				string regs = package.args[1];

				if (!p->isRegister(regd) or !p->isRegister(regs)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				int reg1 = stoi(regd.substr(1));
				int reg2 = stoi(regs.substr(1));

				if (package.instruction == "add") bytes.push_back(0x70);
				else if (package.instruction == "sub") bytes.push_back(0x71);
				else if (package.instruction == "mul") bytes.push_back(0x72);
				else if (package.instruction == "div") bytes.push_back(0x73);
				else if (package.instruction == "cmp") bytes.push_back(0x74);

				
				bytes.push_back((reg1 << 4) | reg2);

				locationCounter += 2;
				sectionLocationCounter += 2;
				currentSectionHeader->size += 2;
			}
		
			if (package.instruction == "not" or package.instruction == "and" or package.instruction == "or" or package.instruction == "xor" or package.instruction == "test") {
				if (package.instruction != "not" and package.args.size() != 2) ERROR("Nevalidan broj argumenata za instrukciju %s na liniji %s", package.instruction.c_str(), package.line.c_str());
				if (package.instruction == "not" and package.args.size() != 1) ERROR("Nevalidan broj argumenata za instrukciju not na liniji %s", package.line.c_str());

				string regd = package.args[0];
				string regs;
				if (package.instruction != "not") regs = package.args[1];

				if (!p->isRegister(regd)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());
				if (package.instruction != "not" and !p->isRegister(regs)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				int reg1 = stoi(regd.substr(1));
				int reg2 = 0;
				if (package.instruction != "not") reg2 = stoi(regs.substr(1));

				if (package.instruction == "not") bytes.push_back(0x80);
				else if (package.instruction == "and") bytes.push_back(0x81);
				else if (package.instruction == "or") bytes.push_back(0x82);
				else if (package.instruction == "xor") bytes.push_back(0x83);
				else if (package.instruction == "test") bytes.push_back(0x84);


				bytes.push_back((reg1 << 4) | reg2);

				locationCounter += 2;
				sectionLocationCounter += 2;
				currentSectionHeader->size += 2;
			}
		
			if (package.instruction == "shl" or package.instruction == "shr") {
				if (package.args.size() != 2) ERROR("Nevalidan broj argumenata za instrukciju %s na liniji %s", package.instruction.c_str(), package.line.c_str());

				string regd = package.args[0];
				string regs = package.args[1];

				if (!p->isRegister(regd) or !p->isRegister(regs)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				int reg1 = stoi(regd.substr(1));
				int reg2 = stoi(regs.substr(1));

				if (package.instruction == "shl") bytes.push_back(0x90);
				else if (package.instruction == "shr") bytes.push_back(0x91);

				bytes.push_back((reg1 << 4) | reg2);

				locationCounter += 2;
				sectionLocationCounter += 2;
				currentSectionHeader->size += 2;
			}

			if (package.instruction == "ldr") {
				if (package.args.size() != 2) ERROR("Nevalidan broj argumenata za instrukciju ldr na liniji %s", package.line.c_str());

				bytes.push_back(0xa0);

				string regd = package.args[0];
				if (!p->isRegister(regd)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());
				int reg = stoi(regd.substr(1));

				string arg = package.args[1];

				if (p->isLiteral(arg)) {
					arg = arg.substr(1);

					bytes.push_back(reg << 4 | 0xF);
					bytes.push_back(0);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isSymbol(arg)) {
					arg = arg.substr(1);

					bytes.push_back(reg << 4 | 0xF);
					bytes.push_back(0);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->defined and symbol->equ){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{

						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isMemLiteral(arg)) {

					bytes.push_back(reg << 4 | 0xF);
					bytes.push_back(0x4);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isMemSymbol(arg)) {

					bytes.push_back(reg << 4 | 0xF);
					bytes.push_back(0x4);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->defined and symbol->equ){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{
						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isPCRelSymbol(arg)) {
					arg = arg.substr(1);

					bytes.push_back((reg << 4) | 0x7);
					bytes.push_back(0x03);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					rtEntry* reloc = new rtEntry();
					reloc->offset = sectionLocationCounter + 3;
					reloc->section = currentSectionHeader;
					if (symbol->defined and !symbol->global){
						reloc->symbol = symbol->section;
						reloc->addend = symbol->value - 2;
					}
					else{
						reloc->symbol = symbol;
						reloc->addend = -2;
					}
					reloc->type = REL_PC;
					relocationsTable.push_back(reloc);

					if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

					bytes.push_back(0);
					bytes.push_back(0);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isRegister(arg)) {
					arg = arg.substr(1);

					int num = stoi(arg);

					bytes.push_back((reg << 4) | num);
					bytes.push_back(0x1);
				}

				else if (p->isRegIndirect(arg)) {
					arg = arg.substr(2);

					int num = stoi(arg);

					bytes.push_back((reg << 4) | num);
					bytes.push_back(0x2);
				}

				else if (p->isRegIndirectWithLiteral(arg)) {
					string regi = arg.substr(2, arg.find("+") - 2);
					p->trim(regi);
					string number = arg.substr(arg.find("+") + 1);
					p->trim(number);
					number.erase(number.find("]"));

					unsigned short num = stoi(number);
					int regnum = stoi(regi);

					bytes.push_back((reg << 4) | regnum);
					bytes.push_back(0x3);
					bytes.push_back(num >> 8);
					bytes.push_back(num & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isRegIndirectWithSymbol(arg)) {
					string regi = arg.substr(2, arg.find("+") - 2);
					p->trim(regi);
					string sym = arg.substr(arg.find("+") + 1);
					p->trim(sym);
					sym.erase(sym.find("]"));

					int regnum = stoi(regi);

					bytes.push_back((reg << 4) | regnum);
					bytes.push_back(0x3);

					stEntry* symbol = findSymbol(sym);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = sym;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(sym);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ and symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{

						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				locationCounter += 3;
				sectionLocationCounter += 3;
				currentSectionHeader->size += 3;
			}

			if (package.instruction == "str") {
				if (package.args.size() != 2) ERROR("Nevalidan broj argumenata za instrukciju str na liniji %s", package.line.c_str());

				bytes.push_back(0xb0);

				string regd = package.args[0];
				if (!p->isRegister(regd)) ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());
				int reg = stoi(regd.substr(1));

				string arg = package.args[1];

				if (p->isLiteral(arg) or p->isSymbol(arg)) {
					ERROR("Greska: Nije moguce neposredno adresiranje kod instrukcije str(na liniji %s)", package.line.c_str());
				}

				else if (p->isMemLiteral(arg)) {

					bytes.push_back(reg << 4 | 0xF);
					bytes.push_back(0x4);

					unsigned short word = 0;
					if (arg.find("0x") != string::npos) {
						arg = arg.substr(2);
						stringstream ss;
						ss << hex << arg;
						ss >> word;
					}
					else word = stoi(arg);

					bytes.push_back(word >> 8);
					bytes.push_back(word & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isMemSymbol(arg)) {

					bytes.push_back(reg << 4 | 0xF);
					bytes.push_back(0x4);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ and symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}

					else{

						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isPCRelSymbol(arg)) {
					arg = arg.substr(1);

					bytes.push_back((reg << 4) | 0x7);
					bytes.push_back(0x03);

					stEntry* symbol = findSymbol(arg);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = arg;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(arg);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					rtEntry* reloc = new rtEntry();
					reloc->offset = sectionLocationCounter + 3;
					reloc->section = currentSectionHeader;
					if (symbol->defined and !symbol->global){
						reloc->symbol = symbol->section;
						reloc->addend = symbol->value - 2;
					}
					else{
						reloc->symbol = symbol;
						reloc->addend = -2;
					}
					reloc->type = REL_PC;
					relocationsTable.push_back(reloc);

					if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

					bytes.push_back(0);
					bytes.push_back(0);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isRegister(arg)) {
					arg = arg.substr(1);

					int num = stoi(arg);

					bytes.push_back((reg << 4) | num);
					bytes.push_back(0x1);
				}

				else if (p->isRegIndirect(arg)) {
					arg = arg.substr(2);

					int num = stoi(arg);

					bytes.push_back((reg << 4) | num);
					bytes.push_back(0x2);
				}

				else if (p->isRegIndirectWithLiteral(arg)) {
					string regi = arg.substr(2, arg.find("+") - 2);
					p->trim(regi);
					string number = arg.substr(arg.find("+") + 1);
					p->trim(number);
					number.erase(number.find("]"));

					unsigned short num = stoi(number);
					int regnum = stoi(regi);

					bytes.push_back((reg << 4) | regnum);
					bytes.push_back(0x3);
					bytes.push_back(num >> 8);
					bytes.push_back(num & 0xFF);

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else if (p->isRegIndirectWithSymbol(arg)) {
					string regi = arg.substr(2, arg.find("+") - 2);
					p->trim(regi);
					string sym = arg.substr(arg.find("+") + 1);
					p->trim(sym);
					sym.erase(sym.find("]"));

					int regnum = stoi(regi);

					bytes.push_back((reg << 4) | regnum);
					bytes.push_back(0x3);

					stEntry* symbol = findSymbol(sym);
					if (symbol == nullptr) {
						symbol = new stEntry();
						symbol->name = sym;
						symbol->nameIndex = stringTable.size();
						stringTable.push_back(sym);
						symbol->index = symbolTable.size();
						symbolTable.push_back(symbol);
					}

					if (symbol->equ and symbol->defined){
						bytes.push_back(symbol->value >> 8);
						bytes.push_back(symbol->value & 0xFF);
					}
					else{
						rtEntry* reloc = new rtEntry();
						reloc->offset = sectionLocationCounter + 3;
						reloc->section = currentSectionHeader;
						if (symbol->global or symbol->equ) reloc->symbol = symbol;
						else {
							reloc->symbol = symbol->section;
							reloc->addend = symbol->value;
						}
						reloc->type = REL_BE;
						relocationsTable.push_back(reloc);

						if (symbol->defined == false)	symbol->addRelocToPatch(reloc);

						bytes.push_back(0);
						bytes.push_back(0);
					}

					locationCounter += 2;
					sectionLocationCounter += 2;
					currentSectionHeader->size += 2;
				}

				else ERROR("Nevalidan nacin adresiranja na liniji %s", package.line.c_str());

				locationCounter += 3;
				sectionLocationCounter += 3;
				currentSectionHeader->size += 3;
			}
		}

	}

	for (int i=0; i<equSyms.size(); i++){ //Resavanje equ simbola
		stEntry* symbol = equSyms[i];
		vector<string> tokens = expressionTokens[i];

		vector<stEntry*> symbols;
		vector<bool> signs;
		bool sign = true;

		for(string token: tokens){
			if (token == "+") sign = true;
			else if (token == "-") sign = false;
			else{
				if (p->isMemSymbol(token)){
					stEntry* tokenSym = findSymbol(token);
					if(tokenSym == nullptr) ERROR("Greska: Koriscen nepoznati simbol %s na liniji %s", token.c_str(), equSymLines[i].c_str());

					if (tokenSym->equ and tokenSym->defined){ // Ovo je u sustini konstanta a ne simbol
							symbol->value += (sign ? tokenSym->value : -tokenSym->value);
					}
					else{
						symbols.push_back(tokenSym);
						signs.push_back(sign);
					}
				}
				else{
					unsigned short word = 0;
					if (token.find("0x") != string::npos) {
					token = token.substr(2);
					stringstream ss;
					ss << hex << token;
					ss >> word;
					}
					else word = stoi(token);

					symbol->value += (sign ? word : -word);
				}
			}
		}
		
		bool possible = true;

		while(symbols.size() > 1){
			bool found = false;
			for(int p=0; p<symbols.size(); p++){
				for(int j=p+1; j<symbols.size(); j++){
					stEntry* sym1 = symbols[p], *sym2 = symbols[j];
					bool sign1 = signs[p], sign2 = signs[j];
					if(sym1->section == sym2->section and (sign1 != sign2) and sym1->section != nullptr){
						symbol->value += (sign1 ? sym1->value : -sym1->value) + (sign2 ? sym2->value : -sym2->value);
						auto it1 = symbols.begin();
						it1 += p;
						symbols.erase(it1);
						auto it2 = symbols.begin();
						it2 += (j-1);
						symbols.erase(it2);
						auto it3 = signs.begin();
						it3 += p;
						signs.erase(it3);
						auto it4 = signs.begin();
						it4 += (j-1);
						signs.erase(it4);
						found = true;
						break;
					}
				}
				if (found) break;
			}
			if (!found) {possible = false; break;}
		}
	
		if (!possible) ERROR("Greska: U direktivi equ na liniji %s ; simbol nije relokatibilan", equSymLines[i].c_str());

		if (symbols.size() == 0) symbol->defined = true;
		else {
			symbol->section = symbols[0];
			symbol->isSection = (signs[0] == false);
		}
	}

	vector<rtEntry*> relocsToDelete;

	bool changes = false;

	do{ //Neizracunljivi equ simboli
			changes = false;
			for (stEntry* symbol : symbolTable){
				if(symbol->equ and symbol->section != nullptr and symbol->section->equ and symbol->section->defined){
					changes = true;
					symbol->value += (symbol->isSection ? -symbol->section->value : symbol->section->value);
					symbol->defined = true;
					symbol->section = nullptr;
					flink *head = symbol->patch;
					while(head){
						rtEntry* reloc = head->reloc;
						relocsToDelete.push_back(reloc);
						if (reloc->type == REL_LE){
							bytes[reloc->offset + reloc->section->offset] = symbol->value & 0xFF;
							bytes[reloc->offset + 1 + reloc->section->offset] = symbol->value >> 8;
						}
						else if (reloc->type == REL_BE){
							bytes[reloc->offset + reloc->section->offset] = symbol->value >> 8;
							bytes[reloc->offset + 1 + reloc->section->offset] = symbol->value & 0xFF;
						}
						head = head->next;
					}
					symbol->deletePatches();
					symbol->patch = nullptr;
				}
			}
	} while(changes);

	for(rtEntry* reloc : relocsToDelete){
		auto it = find(relocationsTable.begin(), relocationsTable.end(), reloc);
		if (it != relocationsTable.end()) relocationsTable.erase(it);
	}

	do{
			changes = false;
			for (stEntry* symbol : symbolTable){
				if(symbol->equ and symbol->section != nullptr and symbol->section->equ and !symbol->section->defined and symbol->patch){
					changes = true;
					flink *head = symbol->patch;
					while(head){
						rtEntry* reloc = head->reloc;
						head = head->next;
						reloc->symbol = symbol->section; reloc->addend += symbol->value;
						symbol->section->addRelocToPatch(reloc);
					}
					symbol->deletePatches();
					symbol->patch = nullptr;
				}
			}
	} while(changes);

	for (stEntry* symbol : symbolTable) { //Back-patching
		if (symbol->equ){
			if (symbol->defined){
				flink* head = symbol->patch;
				while (head) {
					rtEntry* reloc = head->reloc;
					relocsToDelete.push_back(reloc);
					shEntry* section = reloc->section;
					if (reloc->type == REL_LE){
						bytes[section->offset + reloc->offset] = symbol->value & 0xFF;
						bytes[section->offset + reloc->offset + 1] = symbol->value >> 8;
					}
					else if (reloc->type == REL_BE){
						bytes[section->offset + reloc->offset] = symbol->value >> 8;
						bytes[section->offset + reloc->offset + 1] = symbol->value & 0xFF;
					}
					head = head->next;
				}
			}
			else{
				stEntry* dependent = symbol->section;
				flink* head = symbol->patch;
				while (head) {
					rtEntry* reloc = head->reloc;
					reloc->addend += symbol->value;
					if (dependent->global) reloc->symbol = dependent;
					else {reloc->symbol = dependent->section; reloc->addend += dependent->value; }
					reloc->minus = symbol->isSection;
					head = head->next;
				}
			}
		}
		else{
			if (symbol->global and !symbol->defined and !symbol->labelledExtern) ERROR("Greska: Simbol %s koji je prozvan globalnim nije definisan", symbol->name.c_str());
			if (symbol->patch != nullptr) {
				if (!symbol->labelledExtern and !symbol->defined) ERROR("Greska: Koriscen simbol %s koji nije ni definisan ni prozvan eksternim", symbol->name.c_str());
				flink* head = symbol->patch;
				while (head) {
					if (!symbol->global) {
						head->reloc->symbol = symbol->section;
						head->reloc->addend = symbol->value + (head->reloc->type == REL_PC ? -2 : 0);
					}
					else {
						head->reloc->symbol = symbol;
						head->reloc->addend = head->reloc->type == REL_PC ? -2 : 0;
					}
					head = head->next;
				}
			}
		}
	}

	for(rtEntry* reloc : relocsToDelete){
		auto it = find(relocationsTable.begin(), relocationsTable.end(), reloc);
		if (it != relocationsTable.end()) relocationsTable.erase(it);
	}
	
	if(debug) debugFile.close();

	writeTxt(inputfile.substr(0, inputfile.find(".")) + ".debug");

	delete p;

}

void Assembler::write(string outputfile) {
	fstream output;
	output.open(outputfile, ios::out | ios::binary);

	mainHeader* header = new mainHeader();

	header->shoff = mainHeader::binarySize() + bytes.size();
	header->shsize = sectionHeaderTable.size();
	header->symtaboff = header->shoff + header->shsize * shEntry::binarySize();
	header->symtabsize = symbolTable.size();
	header->strtaboff = header->symtaboff + header->symtabsize * stEntry::binarySize();
	header->strtabsize = stringTable.size();
	int strtabbinarysize = 0;
	for (string s : stringTable) strtabbinarysize += (s.size() + 1);
	header->reloff = header->strtaboff + strtabbinarysize;
	header->relsize = relocationsTable.size();

	header->writeBinary(output);

	for (unsigned char byte : bytes) output.write((char*)&byte, sizeof(unsigned char));

	for (shEntry* section : sectionHeaderTable) section->writeBinary(output);

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

void Assembler::writeTxt(string outputFile) {
	fstream output;
	output.open(outputFile, ios::out);

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

	for (shEntry* section : sectionHeaderTable) {
		output << "section " << section->name << ":";

		int i = section->offset;
		int counter = 0;

		while (i < section->offset + section->size) {
			if (counter % 8 == 0) {
				output << endl << hex << setw(4) << setfill('0') << unsigned(counter) << ": ";
			}

			output << hex << setw(2) << setfill('0') << unsigned(bytes[i]) << " ";
			i++; counter++;
		}

		output << endl;

		output << "rela" << section->name << ": " << endl;

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