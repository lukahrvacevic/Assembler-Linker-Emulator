#include <iostream>
#include <string.h>
#include "error.h"
#include "../inc/assembler.h"

using namespace std;

int main(int argc, char** argv){
    string entryFile = "";
    string outputFile = "output.o";

    for(int i=1; i<argc; i++){

        if(strcmp(argv[i],"-o") == 0){
            if(i == (argc - 1)) ERROR("Nije dato ime izlaznog fajla a ukljucena je opcija -o.")
            outputFile = argv[++i];
        }
        else entryFile = argv[i];
    }

    if (entryFile == "") ERROR("Nije dato ime ulaznog fajla.");

    Assembler* assembler = new Assembler();

    assembler->assemble(entryFile);

    assembler->write(outputFile);

    delete assembler;

}
