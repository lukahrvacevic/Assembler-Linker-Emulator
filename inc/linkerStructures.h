#include <iostream>
#include <fstream>

using namespace std;

class mainHeader {
public:

	unsigned short shoff = 0; //Offset na kom se nalazi section header table
	unsigned short shsize = 0; //Broj ulaza u section header table
	unsigned short symtaboff = 0; //Offset na kom se nalazi tabela simbola
	unsigned short symtabsize = 0; //Broj simbola u tabeli simbola
	unsigned short strtaboff = 0; //Offset na kom se nalazi tabela stringova
	unsigned short strtabsize = 0; //Broj stringova u tabeli stringova
	unsigned short reloff = 0; //Offset na kom se nalazi tabela relokacionih zapisa
	unsigned short relsize = 0; //Broj relokacionih zapisa

	void writeBinary(fstream& out) {
		out.write((char*)&shoff, sizeof(unsigned short));
		out.write((char*)&shsize, sizeof(unsigned short));
		out.write((char*)&symtaboff, sizeof(unsigned short));
		out.write((char*)&symtabsize, sizeof(unsigned short));
		out.write((char*)&strtaboff, sizeof(unsigned short));
		out.write((char*)&strtabsize, sizeof(unsigned short));
		out.write((char*)&reloff, sizeof(unsigned short));
		out.write((char*)&relsize, sizeof(unsigned short));
	}

	void readBinary(fstream& in) {
		in.read((char*)&shoff, sizeof(unsigned short));
		in.read((char*)&shsize, sizeof(unsigned short));
		in.read((char*)&symtaboff, sizeof(unsigned short));
		in.read((char*)&symtabsize, sizeof(unsigned short));
		in.read((char*)&strtaboff, sizeof(unsigned short));
		in.read((char*)&strtabsize, sizeof(unsigned short));
		in.read((char*)&reloff, sizeof(unsigned short));
		in.read((char*)&relsize, sizeof(unsigned short));
	}

	static int binarySize() {
		return 8 * sizeof(unsigned short);
	}
};

class shEntry {
public:

	std::string name = ""; //Ime sekcije
	unsigned short nameIndex = 0; //Indeks stringa u tabeli stringova koji je ime sekcije
	unsigned short offset = 0; //Offset na kom se nalazi pocetak sekcije
	unsigned short size = 0; //Velicina sekcije
	unsigned short index = 0; //Indeks sekcije u tabeli simbola

	void writeBinary(fstream& out) {
		out.write((char*)&nameIndex, sizeof(unsigned short));
		out.write((char*)&offset, sizeof(unsigned short));
		out.write((char*)&size, sizeof(unsigned short));
		out.write((char*)&index, sizeof(unsigned short));
	}

	void readBinary(fstream& in) {
		in.read((char*)&nameIndex, sizeof(unsigned short));
		in.read((char*)&offset, sizeof(unsigned short));
		in.read((char*)&size, sizeof(unsigned short));
		in.read((char*)&index, sizeof(unsigned short));
	}

	static int binarySize() {
		return 4 * sizeof(unsigned short);
	}
};

enum RELOC_TYPE {
	REL_LE = 0, REL_PC = 1, REL_BE = 2
};

class rtEntry;

struct flink {
	flink* next = nullptr; //Pokazivac na sledecu ispravku u nizu
	rtEntry* reloc = nullptr; //Relokacioni zapis koji treba da se ispravi
};

class stEntry {
public:

	std::string name = ""; //Ime simbola
	int nameIndex = 0; //Indeks stringa u tabeli stringova koji je ime simbola
	unsigned short value = 0; //Vrednost simbola
	bool global = false; //Da li je simbol globalan
	stEntry* section = nullptr; //Sekcija kojoj pripada
	bool defined = false; //Da li je simbol definisans
	bool labelledExtern = false; //Da li je prozvan kao extern
	bool isSection = false; //Da li je simbol sekcija
	bool equ = false; //Da li je simbol definisan preko equ
	int index = 0; //Indeks simbola u tabeli simbola

	int sectionIndex = -1; //Sluzi za privremeno cuvanje indeksa sekcije dok se ne ucita sve

	void writeBinary(fstream& out) {
		out.write((char*)&nameIndex, sizeof(unsigned short));
		out.write((char*)&value, sizeof(unsigned short));
		unsigned char i = global;
		out.write((char*)&i, sizeof(unsigned char));
		if (labelledExtern or (equ and !section)) {
			char p = -1;
			out.write(&p, sizeof(char));
		}
		else if (isSection and !equ) {
			out.write((char*)&index, sizeof(unsigned char));
		}
		else out.write((char*)&section->index, sizeof(unsigned char));
		i = isSection;
		out.write((char*)&i, sizeof(unsigned char));
		out.write((char*)&index, sizeof(unsigned char));
		i = equ;
		out.write((char*)&i, sizeof(unsigned char));
	}

	static int binarySize() {
		return 2 * sizeof(unsigned short) + 5 * sizeof(unsigned char);
	}

	void readBinary(fstream& in) {
		in.read((char*)&nameIndex, sizeof(unsigned short));
		in.read((char*)&value, sizeof(unsigned short));
		unsigned char i = 0;
		in.read((char*)&i, sizeof(unsigned char));
		if (i) global = true;
		char p = 0;
		in.read((char*)&p, sizeof(char));
		if (p != -1) {
			defined = true;
			sectionIndex = p;
		}
		in.read((char*)&i, sizeof(unsigned char));
		if (i) isSection = true;
		in.read((char*)&index, sizeof(unsigned char));
		in.read((char*)&i, sizeof(unsigned char));
		if (i) equ = true;
	}

};


class rtEntry {
public:

	unsigned short offset = 0; //Offset na kom se prepravlja
	stEntry* symbol = nullptr; //Simbol na kog se odnosi
	unsigned short addend = 0; //Addend ako postoji
	RELOC_TYPE type = REL_LE; //Tip relokacije
	shEntry* section = nullptr; //Sekcija u kojoj je relokacija
	bool minus = false; //Da li se vrednost simbola mnozi sa -1

	int symbolIndex = 0; //Za privremeno cuvanje indeksa simbola
	int sectionIndex = 0; //Za privremeno cuvanje indeksa sekcije

	void writeBinary(fstream &out) {
		out.write((char*)&offset, sizeof(unsigned short));
		out.write((char*)&symbol->index, sizeof(unsigned char));
		out.write((char*)&addend, sizeof(unsigned short));
		unsigned char i = 0;
		if (type == REL_PC) i = 1;
		else if (type == REL_BE) i = 2;
		out.write((char*)&i, sizeof(unsigned char));
		out.write((char*)&section->index, sizeof(unsigned char));
		i = minus;
		out.write((char*)&i, sizeof(unsigned char));
	}

	void readBinary(fstream& in) {
		in.read((char*)&offset, sizeof(unsigned short));
		in.read((char*)&symbolIndex, sizeof(unsigned char));
		in.read((char*)&addend, sizeof(unsigned short));
		unsigned char i = 0;
		in.read((char*)&i, sizeof(unsigned char));
		if (i == 1) type = REL_PC;
		else if (i == 2) type = REL_BE;
		in.read((char*)&sectionIndex, sizeof(unsigned char));
		in.read((char*)&i, sizeof(unsigned char));
		if (i) minus = true;
	}

	static int binarySize() {
		return 2 * sizeof(unsigned short) + 4 * sizeof(unsigned char);
	}
};