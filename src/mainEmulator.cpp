#include <iostream>
#include <sstream>
#include <string.h>
#include <string>
#include "error.h"
#include "../inc/emulator.h"

using namespace std;

int main(int argc, char** argv) {

    string entryfile = "";

    if (argc < 2) ERROR("Greska: Nije uneto ime ulaznog fajla kao opcija.");
    entryfile = argv[1];

    Emulator* emulator = new Emulator();

    emulator->run(entryfile);

    emulator->outputState();

    delete emulator;

}
