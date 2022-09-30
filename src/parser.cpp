#include "../inc/parser.h"

vector<string> Parser::resolveArgs(string s, string line){
      trim(s);
      int pos = 0;
      vector<string> args;
      if (s == "") return args;
      while((pos = s.find(",")) != string::npos){
        string arg = s.substr(0,pos);
        if(!isArgument(arg)) ERROR("Sintaksna greska na liniji %s", line.c_str());
        args.push_back(arg);
        s.erase(0, pos + 1);
        trim(s);
      }
      if (!isArgument(s)) ERROR("Sintaksna greska na liniji %s", line.c_str());
      trim(s);
      args.push_back(s);
      return args;
}

packedLine Parser::nextLine(){
    string line;
    string lineCopy;
    packedLine package;

    getline(file, line);
    trim(line);
	
    int pos = 0;
    
    while (line == "" or line[0] == '#') {
        getline(file, line);
		while ((pos = line.find("#")) != string::npos) line.erase(pos);
		trim(line);
    }
    lineCopy = line;
	
	if((pos = line.find("#")) != string::npos) line.erase(pos);

    if ((pos = line.find(":")) != string::npos) {
        package.label = line.substr(0, pos);
        if(!isMemSymbol(package.label)) ERROR("Nevalidna labela na liniji %s", line.c_str());
        line.erase(0, pos + 1);
        trim(line);
    }

    if (line.size() > 0) {
        if (line[0] == '.') {
            string directive;
            bool args = true;
            if ((pos = line.find(" ")) == string::npos) {
                args = false;
                directive = line;
            }
            else directive = line.substr(0, line.find(" "));;
            if (!checkDirective(directive)) {
                file.close();
                ERROR("Nepoznata direktiva %s", directive.c_str());
            }
            package.directive = directive;
            if (args) {
                line.erase(0, line.find(" ") + 1);
                trim(line);
                package.args = resolveArgs(line, lineCopy);
            }
        }
        else {
            string instruction;
            bool args = true;
            if ((pos = line.find(" ")) == string::npos) {
                args = false;
                instruction = line;
            }
            else instruction = line.substr(0, line.find(" "));;
            if (!checkInstruction(instruction)) {
                file.close();
                ERROR("Nepoznata instrukcija %s", instruction.c_str());
            }
            package.instruction = instruction;
            if (args) {
                line.erase(0, line.find(" ") + 1);
                trim(line);
                package.args = resolveArgs(line, lineCopy);
            }
        }
    }

    package.line = lineCopy;
    return package;
}