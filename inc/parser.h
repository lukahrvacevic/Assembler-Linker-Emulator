#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm> 
#include <cctype>
#include <locale>
#include <regex>
#include <string>
#include "error.h"

using namespace std;

struct packedLine{
  string directive = "";
  string instruction = "";
  string label = "";
  vector<string> args;
  string line = "";
};

class Parser {

public:

    Parser(string filename) {
        file.open(filename);
        if (!file.is_open()) ERROR("Fajl %s nije pronadjen", filename.c_str());
        directives.push_back(".global"); directives.push_back(".extern"); directives.push_back(".section"); directives.push_back(".word"); directives.push_back(".skip");
        directives.push_back(".ascii"); directives.push_back(".equ"); directives.push_back(".end");
        instructions.push_back("halt"); instructions.push_back("int"); instructions.push_back("iret"); instructions.push_back("call");
        instructions.push_back("ret"); instructions.push_back("jmp"); instructions.push_back("jeq"); instructions.push_back("jne");
        instructions.push_back("jgt"); instructions.push_back("push"); instructions.push_back("pop"); instructions.push_back("xchg");
        instructions.push_back("add"); instructions.push_back("sub"); instructions.push_back("mul"); instructions.push_back("div");
        instructions.push_back("cmp"); instructions.push_back("not"); instructions.push_back("and"); instructions.push_back("or");
        instructions.push_back("xor"); instructions.push_back("test"); instructions.push_back("shl"); instructions.push_back("shr");
        instructions.push_back("ldr"); instructions.push_back("str");
    }

    ~Parser() {
        if (file.is_open()) file.close();
    }

    packedLine nextLine();

    fstream file;
    vector<string> directives;
    vector<string> instructions;

    bool isRegister(string s) {
        return regex_match(s, regex("r[0-8]"));
    }

    bool isExpression(string s){
        int posplus = 0, posminus = 0;
        trim(s);
        if (s[0] == '+' or s[0] == '-') {s = s.substr(1); trim(s);}
        posplus = s.find("+"); posminus = s.find("-");
        while(posplus != string::npos or posminus != string::npos){
            int pos = 0;
            if (posminus == string::npos) pos = posplus;
            else if (posplus==string::npos) pos = posminus;
            else pos = posminus<posplus  ? posminus : posplus;
            string arg = s.substr(0, pos);
            trim(arg);
            if(!(isMemLiteral(arg) or isMemSymbol(arg))) return false;
            s.erase(0, pos + 1);
            trim(s);
            if(s[0] == '+' or s[0] == '-') return false;
            posplus = s.find("+"); posminus = s.find("-");
        }
        if(!(isMemLiteral(s) or isMemSymbol(s))) return false;
        return true;
    }

    bool isLiteral(string s) {
        return regex_match(s, regex("\\$[0-9]+")) or regex_match(s, regex("\\$0x[0-9A-Fa-f]{1,4}"));
    }

    bool isSymbol(string s) {
        return regex_match(s, regex("\\$[a-zA-Z]{1}[a-zA-Z0-9?.@_$%]*"));
    }

    bool isMemLiteral(string s) {
        return regex_match(s, regex("[0-9]+")) or regex_match(s, regex("0x[0-9A-Fa-f]{1,4}"));
    }

    bool isMemSymbol(string s) {
        return regex_match(s, regex("[a-zA-Z]{1}[a-zA-Z0-9?.@_$%]*"));
    }

    bool isPCRelSymbol(string s) {
        return regex_match(s, regex("%[a-zA-Z]{1}[a-zA-Z0-9?.@_$%]*"));
    }

    bool isRegIndirect(string s) {
        return regex_match(s, regex("\\[r[0-8]\\]"));
    }

    bool isRegIndirectWithLiteral(string s) {
        return regex_match(s, regex("\\[r[0-8]\\s*\\+\\s*[0-9]+\\]"));
    }

    bool isRegIndirectWithSymbol(string s) {
        return regex_match(s, regex("\\[r[0-8]\\s*\\+\\s*[a-zA-Z]{1}[a-zA-Z0-9?.@_$%s]*\\]"));
    }

    bool isJMPSymbol(string s) {
        return regex_match(s, regex("\\*[a-zA-Z]{1}[a-zA-Z0-9?.@_$%]*"));
    }

    bool isJMPLiteral(string s) {
        return regex_match(s, regex("\\*[0-9]+")) or regex_match(s, regex("0x[0-9A-Fa-f]{1,4}"));
    }

    bool isJMPReg(string s) {
        return regex_match(s, regex("\\*r[0-8]"));
    }

    bool checkDirective(string s) {
        return find(directives.begin(), directives.end(), s) != directives.end();
    }

    bool checkInstruction(string s) {
        return find(instructions.begin(), instructions.end(), s) != instructions.end();
    }

    bool isJMPRegIndirect(string s) {
        return regex_match(s, regex("\\*\\[r[0-8]\\]"));
    }

    bool isJMPRegIndirectWithLiteral(string s) {
        return regex_match(s, regex("\\*\\[r[0-8]\\s*\\+\\s*[0-9]+\\]"));
    }

    bool isJMPRegIndirectWithSymbol(string s) {
        return regex_match(s, regex("\\*\\[r[0-8]\\s*\\+\\s*[a-zA-Z]{1}[a-zA-Z0-9?.@_$%]*\\]"));
    }

    bool isAsciiOperand(string s) {
        return regex_match(s, regex("\"[\x20-\x7E]+\""));
    }

    bool isArgument(string s) {
        return isRegister(s) or isLiteral(s) or isSymbol(s) or isMemLiteral(s) or isMemSymbol(s)
            or isPCRelSymbol(s) or isRegIndirect(s) or isRegIndirectWithLiteral(s) or isRegIndirectWithSymbol(s)
            or isJMPLiteral(s) or isJMPSymbol(s) or isJMPReg(s) or isJMPRegIndirect(s)
            or isJMPRegIndirectWithLiteral(s) or isJMPRegIndirectWithSymbol(s) or isAsciiOperand(s)
            or isExpression(s);
    }

    void ltrim(string& s) {
        s.erase(s.begin(), find_if(s.begin(), s.end(), [](unsigned char ch) {
            return !isspace(ch);
            }));
    }

    void rtrim(string& s) {
        s.erase(find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
            return !isspace(ch);
            }).base(), s.end());
    }

    void trim(string& s) {
        ltrim(s);
        rtrim(s);
    };

    vector<string> resolveArgs(string s, string line);
};