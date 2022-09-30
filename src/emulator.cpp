#include "../inc/emulator.h"

void Emulator::run(string entryfile)
{
	fstream input;
	input.open(entryfile, ios::in | ios::binary);
	if (!input.is_open()) ERROR("Greska: Fajl %s ne postoji", entryfile.c_str());

	int programSize = 0;
	input.seekg(0, ios::end);
	programSize = input.tellg();
	input.seekg(0, ios::beg);

	if (programSize > MAX_PROGRAM_SIZE) ERROR("Greska: Ulaznu datoteku nije moguce ucitati u adresni prostor emuliranog racunarskog sistema.");

	program.reserve(MAX_PROGRAM_SIZE);

	for (int i = 0; i < programSize; i++) {
		byte byte = 0;
		input.read((char*)&byte, sizeof(unsigned char));
		program.push_back(byte);
	}

	while (program.size() < MAX_PROGRAM_SIZE) program.push_back(0);

	bool on = true;

	pc = program[pc] | (program[pc + 1] << 8);

	previous_time = time(nullptr) * 1000;

	configureTerminal();

	while (on) {
		byte opcode = program[pc++];

		if (opcode == HALT) {
			on = false;
		}

		else if (opcode == INT) {
			byte regsdescr = program[pc++];
			byte reg = regsdescr >> 4;
			byte entry = r[reg] % 8;
			interrupt(entry);
		}
		
		else if (opcode == IRET) {
			pc = program[sp++] | (program[sp++] << 8);
			psw = program[sp++] | (program[sp++] << 8);
		}

		else if (opcode == CALL) {
			byte regsdescr = program[pc++];
			byte addrmode = program[pc++];

			byte addr = addrmode & 0xF;
			byte mode = addrmode >> 4;

			word newpc = 0;

			if (addr == IMMED) {
				newpc = (program[pc++] << 8) | program[pc++];
			}
			else if (addr == REGDIR) {
				byte reg = regsdescr & 0xF;

				newpc = r[reg];
			}
			else if (addr == REGDIROFF) {
				word offset = (program[pc++] << 8) | program[pc++];
				byte reg = regsdescr & 0xF;

				newpc = r[reg] + (short)offset;
			}
			else if (addr == REGIND) {
				byte reg = regsdescr & 0xF;

				if (mode == DECBEFORE) {
					r[reg] -= 2;
					newpc = program[r[reg]] | (program[r[reg] + 1] << 8);
				}
				else if (mode == DECAFTER) {
					newpc = program[r[reg]] | (program[r[reg] + 1] << 8);
					r[reg] -= 2;
				}
				else if (mode == INCBEFORE) {
					r[reg] += 2;
					newpc = program[r[reg]] | (program[r[reg] + 1] << 8);
				}
				else if (mode == INCAFTER) {
					newpc = program[r[reg]] | (program[r[reg] + 1] << 8);
					r[reg] += 2;
				}
				else newpc = program[r[reg]] | (program[r[reg] + 1] << 8);
			}
			else if (addr == REGINDOFF) {
				word offset = (program[pc++] << 8) | program[pc++];
				byte reg = regsdescr & 0xF;

				if (mode == DECBEFORE) {
					r[reg] -= 2;
					newpc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
				}
				else if (mode == DECAFTER) {
					newpc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
					r[reg] -= 2;
				}
				else if (mode == INCBEFORE) {
					r[reg] += 2;
					newpc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
				}
				else if (mode == INCAFTER) {
					newpc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
					r[reg] += 2;
				}
				else newpc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
			}
			else if (addr == MEM) {
				word address = (program[pc++] << 8) | program[pc++];
				newpc = program[address] | (program[address] << 8);
			}

			program[--sp] = (pc >> 8);
			program[--sp] = pc & 0xFF;

			pc = newpc;
		}

		else if (opcode == RET) {
			pc = program[sp++] | (program[sp++] << 8);
		}

		else if (opcode == JMP or opcode == JEQ or opcode == JNE or opcode == JGT) {
			bool jump = true;
			if (opcode == JEQ and (psw & Z) == 0) jump = false;
			if (opcode == JNE and (psw & Z)) jump = false;
			if (opcode == JGT and ((psw & Z) or (((psw & N) >> 3) ^ (psw & O >> 1) ))) jump = false;

				byte regsdescr = program[pc++];
				byte addrmode = program[pc++];

				byte addr = addrmode & 0xF;
				byte mode = addrmode >> 4;

				if (addr == IMMED) {
					if (jump) pc = (program[pc++] << 8) | program[pc++];
					else pc += 2;
				}
				else if (addr == REGDIR) {
					byte reg = regsdescr & 0xF;

					if (jump) pc = r[reg];
				}
				else if (addr == REGDIROFF) {
					word offset = (program[pc++] << 8) | program[pc++];
					byte reg = regsdescr & 0xF;

					if (jump) pc = r[reg] + (short)offset;
				}
				else if (addr == REGIND) {
					byte reg = regsdescr & 0xF;

					if (mode == DECBEFORE) {
						r[reg] -= 2;
						if (jump) pc = program[r[reg]] | (program[r[reg] + 1] << 8);
					}
					else if (mode == DECAFTER) {
						if (jump) pc = program[r[reg]] | (program[r[reg] + 1] << 8);
						r[reg] -= 2;
					}
					else if (mode == INCBEFORE) {
						r[reg] += 2;
						if (jump) pc = program[r[reg]] | (program[r[reg] + 1] << 8);
					}
					else if (mode == INCAFTER) {
						if (jump) pc = program[r[reg]] | (program[r[reg] + 1] << 8);
						r[reg] += 2;
					}
					else if (jump) pc = program[r[reg]] | (program[r[reg] + 1] << 8);
				}
				else if (addr == REGINDOFF) {
					word offset = (program[pc++] << 8) | program[pc++];
					byte reg = regsdescr & 0xF;

					if (mode == DECBEFORE) {
						r[reg] -= 2;
						if (jump) pc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
					}
					else if (mode == DECAFTER) {
						if (jump) pc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
						r[reg] -= 2;
					}
					else if (mode == INCBEFORE) {
						r[reg] += 2;
						if (jump) pc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
					}
					else if (mode == INCAFTER) {
						if (jump) pc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
						r[reg] += 2;
					}
					else if (jump) pc = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
				}
				else if (addr == MEM) {
					word address = (program[pc++] << 8) | program[pc++];
					if (jump) pc = program[address] | (program[address] << 8);;
				}
		}
	
		else if (opcode == XCHG) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			word temp = r[regd];
			r[regd] = r[regs];
			r[regs] = temp;
		}

		else if (opcode == ADD) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] += r[regs];
		}

		else if (opcode == SUB) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] -= r[regs];
		}

		else if (opcode == MUL) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] *= r[regs];
		}

		else if (opcode == DIV) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] /= r[regs];
		}

		else if (opcode == CMP) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			short result = (short)r[regd] - (short)r[regs];

			if (result == 0) { psw |= Z; }
			else psw &= (~Z);

			if (result < 0) {psw |= N;}
			else psw &= (~N);

			if ((result & 0x8000) != (r[regd] & 0x8000)) {psw |= O;}
			else psw &= (~O);

			if (r[regd] < r[regs]) {psw |= C;}
			else psw &= (~C);
		}

		else if (opcode == NOT) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4;
			r[regd] = ~r[regd];
		}

		else if (opcode == AND) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] &= r[regs];
		}

		else if (opcode == OR) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] |= r[regs];
		}

		else if (opcode == XOR) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			r[regd] ^= r[regs];
		}

		else if (opcode == TEST) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			short result = (short)r[regd] & (short)r[regs];

			if (result == 0) psw |= Z;
			else psw &= (~Z);

			if (result < 0) psw |= N;
			else psw &= (~N);
		}

		else if (opcode == SHL) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			bool carry = (r[regd] & 0x1000);
			r[regd] = r[regd] << r[regs];

			if (r[regd] == 0) psw |= Z;
			else psw &= (~Z);

			if (r[regd] < 0) psw |= N;
			else psw &= (~N);

			if (carry) psw |= C;
			else psw &= (~C);
		}

		else if (opcode == SHR) {
			byte regsdescr = program[pc++];
			byte regd = regsdescr >> 4, regs = regsdescr & 0xF;
			bool carry = (r[regd] & 1);
			r[regd] = r[regd] >> r[regs];

			if (r[regd] == 0) psw |= Z;
			else psw &= (~Z);

			if ((short)r[regd] < 0) psw |= N;
			else psw &= (~N);

			if (carry) psw |= C;
			else psw &= (~C);
		}

		else if (opcode == LDR) {
			byte regsdescr = program[pc++];
			byte addrmode = program[pc++];

			byte addr = addrmode & 0xF;
			byte mode = addrmode >> 4;

			byte regnum = regsdescr >> 4;

			if (addr == IMMED) {
				r[regnum] = (program[pc++] << 8) | program[pc++];
			}
			else if (addr == REGDIR) {
				byte reg = regsdescr & 0xF;

				r[regnum] = r[reg];
			}
			else if (addr == REGDIROFF) {
				word offset = (program[pc++] << 8) | program[pc++];
				byte reg = regsdescr & 0xF;

				r[regnum] = r[reg] + (short)offset;
			}
			else if (addr == REGIND) {
				byte reg = regsdescr & 0xF;

				if (mode == DECBEFORE) {
					r[reg] -= 2;
					r[regnum] = program[r[reg]] | (program[r[reg] + 1] << 8);
				}
				else if (mode == DECAFTER) {
					r[regnum] = program[r[reg]] | (program[r[reg] + 1] << 8);
					r[reg] -= 2;
				}
				else if (mode == INCBEFORE) {
					r[reg] += 2;
					r[regnum] = program[r[reg]] | (program[r[reg] + 1] << 8);
				}
				else if (mode == INCAFTER) {
					r[regnum] = program[r[reg]] | (program[r[reg] + 1] << 8);
					r[reg] += 2;
				}
				else r[regnum] = program[r[reg]] | (program[r[reg] + 1] << 8);
			}
			else if (addr == REGINDOFF) {
				word offset = (program[pc++] << 8) | program[pc++];
				byte reg = regsdescr & 0xF;

				if (mode == DECBEFORE) {
					r[reg] -= 2;
					r[regnum] = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
				}
				else if (mode == DECAFTER) {
					r[regnum] = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
					r[reg] -= 2;
				}
				else if (mode == INCBEFORE) {
					r[reg] += 2;
					r[regnum] = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
				}
				else if (mode == INCAFTER) {
					r[regnum] = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
					r[reg] += 2;
				}
				else r[regnum] = program[r[reg] + (short)offset] | (program[r[reg] + (short)offset + 1] << 8);
			}
			else if (addr == MEM) {
				word address = (program[pc++] << 8) | program[pc++];
				r[regnum] = program[address] | (program[address + 1] << 8);
			}
		}

		else if (opcode == STR) {
			byte regsdescr = program[pc++];
			byte addrmode = program[pc++];

			byte addr = addrmode & 0xF;
			byte mode = addrmode >> 4;

			byte regnum = regsdescr >> 4;

			if (addr == IMMED) {
				interrupt(ERROR_ENTRY);
			}
			else if (addr == REGDIR) {
				byte reg = regsdescr & 0xF;

				r[reg] = r[regnum];
			}
			else if (addr == REGDIROFF) {
				interrupt(ERROR_ENTRY);
			}
			else if (addr == REGIND) {
				byte reg = regsdescr & 0xF;

				if (mode == DECBEFORE) {
					r[reg] -= 2;
					writeWord(r[reg], r[regnum]);
				}
				else if (mode == DECAFTER) {
					writeWord(r[reg], r[regnum]);
					r[reg] -= 2;
				}
				else if (mode == INCBEFORE) {
					r[reg] += 2;
					writeWord(r[reg], r[regnum]);
				}
				else if (mode == INCAFTER) {
					writeWord(r[reg], r[regnum]);
					r[reg] += 2;
				}
				else writeWord(r[reg], r[regnum]);
			}
			else if (addr == REGINDOFF) {
				word offset = (program[pc++] << 8) | program[pc++];
				byte reg = regsdescr & 0xF;

				if (mode == DECBEFORE) {
					r[reg] -= 2;
					writeWord(r[reg] + (short)offset, r[regnum]);
				}
				else if (mode == DECAFTER) {
					writeWord(r[reg] + (short)offset, r[regnum]);
					r[reg] -= 2;
				}
				else if (mode == INCBEFORE) {
					r[reg] += 2;
					writeWord(r[reg] + (short)offset, r[regnum]);
				}
				else if (mode == INCAFTER) {
					writeWord(r[reg] + (short)offset, r[regnum]);
					r[reg] += 2;
				}
				else writeWord(r[reg] + (short)offset, r[regnum]);
			}
			else if (addr == MEM) {
				word address = (program[pc++] << 8) | program[pc++];
				writeWord(address, r[regnum]);
			}
		}

		else interrupt(ERROR_ENTRY);
		
		terminalPoll();

		updatePeriod();

		timerPoll();
	}

	resetTerminal();
}

void Emulator::outputState()
{
	cout << endl;
	for (int i = 0; i < 48; i++) cout << "-";
	cout << endl;
	cout << "Emulated proccessor executed halt instruction" << endl;
	bitset<16> p(psw);
	cout << "Emulated proccessor state: psw=0b" << p << endl;
	cout << "r0=0x" << hex << setw(4) << setfill('0') << (unsigned)r[0] << "\t";
	cout << "r1=0x" << hex << setw(4) << setfill('0') << (unsigned)r[1] << "\t";
	cout << "r2=0x" << hex << setw(4) << setfill('0') << (unsigned)r[2] << "\t";
	cout << "r3=0x" << hex << setw(4) << setfill('0') << (unsigned)r[3] << endl;
	cout << "r4=0x" << hex << setw(4) << setfill('0') << (unsigned)r[4] << "\t";
	cout << "r5=0x" << hex << setw(4) << setfill('0') << (unsigned)r[5] << "\t";
	cout << "r6=0x" << hex << setw(4) << setfill('0') << (unsigned)r[6] << "\t";
	cout << "r7=0x" << hex << setw(4) << setfill('0') << (unsigned)r[7] << endl;
}
