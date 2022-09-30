#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <bitset>
#include "error.h"
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <ctime>

using namespace std;

using byte = unsigned char;
using word = unsigned short;

class Emulator {
public:

	int cn = 0;

	void run(string entryfile);

	void outputState();

	void writeWord(word address, word word) {
		program[address] = word & 0xFF;
		program[address + 1] = word >> 8;
		if (address == term_out) {
			cout << (char)(word & 0xFF) << flush;
		}
	}

	void writeByte(word address, byte byte) {
		program[address] = byte;
		if (address == term_out) {
			cout << (char)byte << flush;
		}
	}

	void interrupt(byte entry){
		program[--sp] = (psw >> 8);
		program[--sp] = psw & 0xFF;
		program[--sp] = (pc >> 8);
		program[--sp] = pc & 0xFF;

		pc = program[entry * 2] | (program[entry * 2 + 1] << 8);
		psw &= (~I);
	}

	void configureTerminal() {
		struct termios raw;

		if( tcgetattr(STDIN_FILENO, &original) < 0){
			ERROR("Greska: Terminalu nije omogucen pristup");
		}

		raw = original;

		raw.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN);
    raw.c_cflag &= ~(CSIZE | PARENB);
    raw.c_cflag |= CS8;
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

		if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) < 0){
			ERROR("Greska: Terminal ne moze biti podesen");
		} 
	}

	void resetTerminal() {
		if (tcsetattr(STDIN_FILENO, TCSANOW, &original) < 0){
			ERROR("Greska: Terminal ne moze biti vracen u pocetno stanje");
		}
	}

	void terminalPoll() {
		if(!(psw & I) and !(psw & Tl)){
			char c;
			if (read(STDIN_FILENO, &c, 1) == 1) {
				program[term_in] = c;
				program[term_in + 1] = 0;

				interrupt(TERMINAL_ENTRY);
			}
		}
	}

	void updatePeriod(){
		byte index = program[tim_cfg];

		if(index == 0) time_period = 500;
		else if(index == 1) time_period = 1000;
		else if(index == 2) time_period = 1500;
		else if(index == 3) time_period = 2000;
		else if(index == 4) time_period = 5000;
		else if(index == 5) time_period = 10000;
		else if(index == 6) time_period = 30000;
		else if(index == 7) time_period = 60000;
	}

	void timerPoll(){
		time_t current = time(nullptr) * 1000;
		time_elapsed += (current - previous_time);
		previous_time = current;

		if(time_elapsed >= time_period and !(psw & I) and !(psw & Tr)){
			time_elapsed -= time_period;
			interrupt(TIMER_ENTRY);
		}
	}

	word r[8] = { 0 };
	word psw = 0;
	word& pc = r[7], & sp = r[6];

	static const int I = 1 << 15;
	static const int Tr = 1 << 14;
	static const int Tl = 1 << 13;
	static const int N = 1 << 3;
	static const int C = 1 << 2;
	static const int O = 1 << 1;
	static const int Z = 1;

	static const int MAX_PROGRAM_SIZE = 65535;

	vector<byte> program;

	static const word term_out = 0xFF00;
	static const word term_in = 0xFF02;

	static const word tim_cfg = 0xFF10;
	int time_period = 500;
	time_t previous_time = 0;
	time_t time_elapsed = 0;

	static const byte ERROR_ENTRY = 1;
	static const byte TIMER_ENTRY = 2;
	static const byte TERMINAL_ENTRY = 3;

	static const byte HALT = 0x00;
	static const byte INT = 0x10;
	static const byte IRET = 0x20;
	static const byte CALL = 0x30;
	static const byte RET = 0x40;
	static const byte JMP = 0x50;
	static const byte JEQ = 0x51;
	static const byte JNE = 0x52;
	static const byte JGT = 0x53;
	static const byte XCHG = 0x60;
	static const byte ADD = 0x70;
	static const byte SUB = 0x71;
	static const byte MUL = 0x72;
	static const byte DIV = 0x73;
	static const byte CMP = 0x74;
	static const byte NOT = 0x80;
	static const byte AND = 0x81;
	static const byte OR = 0x82;
	static const byte XOR = 0x83;
	static const byte TEST = 0x84;
	static const byte SHL = 0x90;
	static const byte SHR = 0x91;
	static const byte LDR = 0xA0;
	static const byte STR = 0xB0;

	static const byte IMMED = 0X00;
	static const byte REGDIR = 0x01;
	static const byte REGDIROFF = 0x05;
	static const byte REGIND = 0x02;
	static const byte REGINDOFF = 0x03;
	static const byte MEM = 0x04;

	static const byte NOUPD = 0X00;
	static const byte DECBEFORE = 0X01;
	static const byte DECAFTER = 0X03;
	static const byte INCBEFORE = 0X02;
	static const byte INCAFTER = 0X04;

	struct termios original;

};