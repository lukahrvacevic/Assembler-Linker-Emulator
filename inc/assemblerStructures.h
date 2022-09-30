#include <iostream>

class mainHeader{
public:

  unsigned short shoff = 0; //Offset na kom se nalazi section header table
  unsigned short shsize = 0; //Broj ulaza u section header table
  unsigned short symtaboff = 0; //Offset na kom se nalazi tabela simbola
  unsigned short symtabsize = 0; //Broj simbola u tabeli simbola
  unsigned short strtaboff = 0; //Offset na kom se nalazi tabela stringova
  unsigned short strtabsize = 0; //Broj stringova u tabeli stringova
  unsigned short reloff = 0; //Offset na kom se nalazi tabela relokacionih zapisa
  unsigned short relsize = 0; //Broj relokacionih zapisa

  void writeBinary(fstream &out) {
	  out.write((char*)&shoff, sizeof(unsigned short));
	  out.write((char*)&shsize, sizeof(unsigned short));
	  out.write((char*)&symtaboff, sizeof(unsigned short));
	  out.write((char*)&symtabsize, sizeof(unsigned short));
	  out.write((char*)&strtaboff, sizeof(unsigned short));
	  out.write((char*)&strtabsize, sizeof(unsigned short));
	  out.write((char*)&reloff, sizeof(unsigned short));
	  out.write((char*)&relsize, sizeof(unsigned short));
  }

  static int binarySize() {
	  return 8 * sizeof(unsigned short);
  }
};

class shEntry{
public:

  std::string name = ""; //Ime sekcije
  unsigned short nameIndex = 0; //Indeks stringa u tabeli stringova koji je ime sekcije
  unsigned short offset = 0; //Offset na kom se nalazi pocetak sekcije
  unsigned short size = 0; //Velicina sekcije
  unsigned short index = 0; //Indeks sekcije u tabeli simbola

  void writeBinary(fstream &out) {
	  out.write((char*)&nameIndex, sizeof(unsigned short));
	  out.write((char*)&offset, sizeof(unsigned short));
	  out.write((char*)&size, sizeof(unsigned short));
	  out.write((char*)&index, sizeof(unsigned short));
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

class stEntry{
public:

  std::string name = ""; //Ime simbola
  int nameIndex = 0; //Indeks stringa u tabeli stringova koji je ime simbola
  unsigned short value = 0; //Vrednost simbola
  bool global = false; //Da li je simbol globalan
  stEntry* section = nullptr; //Sekcija kojoj pripada ili simbol od kojeg zavisi ako je equ
  bool defined = false; //Da li je simbol definisan
  bool labelledExtern = false; //Da li je prozvan kao extern
  bool isSection = false; //Da li je simbol sekcija ili da li se stavlja minus ispred vrednosti simbola od kojeg zavisi ako je equ
	bool equ = false; //Da li je simbol definisan preko equ
  flink* patch = nullptr; //Ulancana lista mesta koja treba da se isprave
  int index = 0; //Indeks simbola u tabeli simbola

	void addRelocToPatch(rtEntry* reloc) {
		flink* node = new flink();
		node->reloc = reloc;
		node->next = patch;
		patch = node;
	}

	void deletePatches() {
		while (patch) {
			flink* node = patch->next;
			delete patch;
			patch = node;
		}
	}

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
};


class rtEntry {
public:

	unsigned short offset = 0; //Offset na kom se prepravlja
	stEntry* symbol = nullptr; //Simbol na kog se odnosi
	unsigned short addend = 0; //Addend ako postoji
	RELOC_TYPE type = REL_LE; //Tip relokacije
	shEntry* section = nullptr; //Sekcija u kojoj je relokacija
	bool minus = false; //Da li se vrednost simbola mnozi sa -1

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

	static int binarySize() {
		return 2 * sizeof(unsigned short) + 4 * sizeof(unsigned char);
	}
};