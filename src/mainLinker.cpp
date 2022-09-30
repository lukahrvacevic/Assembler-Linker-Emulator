#include <iostream>
#include <sstream>
#include <string.h>
#include <regex>
#include "error.h"
#include "../inc/linker.h"

int main(int argc, char** argv) {
    vector<string> entryFiles;
    map<string, unsigned short> places;
    string outputFile = "";
    bool hex = false;
    bool relocateable = false;

    for (int i = 1; i < argc; i++) {

        if (strcmp(argv[i], "-o") == 0) {
            if (i == (argc - 1)) ERROR("Nije dato ime izlaznog fajla a ukljucena je opcija -o.");
            outputFile = argv[++i];
        }
        else if (strcmp(argv[i], "-hex") == 0) hex = true;
        else if (strcmp(argv[i], "-relocateable") == 0) relocateable = true;
        else if (strncmp(argv[i], "-place", 6) == 0) {
            string placeString = argv[i];
            placeString = placeString.substr(placeString.find("=") + 1);

            string section = placeString.substr(0, placeString.find("@"));

            string address = placeString.substr(placeString.find("@") + 1);

            unsigned short word = 0;
            if (address.find("0x") == 0) {

                string adnum = address.substr(2);

                if (!regex_match(adnum, regex("[0-9A-Fa-f]{4}"))) ERROR("Nevalidna sintaksa za adresu: %s", address.c_str());

                while (adnum[0] == '0') adnum = adnum.substr(1);

                stringstream ss;

                ss << std::hex << adnum;

                ss >> word;

            }
            else ERROR("Nevalidna sintaksa za adresu: %s", address.c_str());

            places.insert(pair<string, unsigned short>(section, word));
        }
        else entryFiles.push_back(argv[i]);
    }

    if (entryFiles.size() == 0) ERROR("Nije naveden nijedan ulazni fajl.");

    if (hex and relocateable) ERROR("Ne mogu biti navedene i hex i relocateable opcije.");

    Linker* linker = new Linker();

    linker->proccess(entryFiles);

    linker->write(outputFile, hex, relocateable, places);

}
