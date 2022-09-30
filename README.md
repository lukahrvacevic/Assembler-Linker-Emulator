# Assembler-Linker-Emulator
Implementation of a toolchain consisting of an assembler, a linker and an emulator,
similar to the GNU one, made for a hypothethical 16-bit architecture explained 
in detail in the documentation, where you can find all the supported instructions
and directives, as well as a guide on how to use the implemented timer and terminal peripheries.

## Build

By running the following command:
``` bash
$ ./build.sh
```
three executable programs will be generated: **assembler**, **linker** and **emulator**.

### Assembler

This tool is used as follows
``` bash
assembler [options] <input_file_name>
```

Options:

<u>-o <output_file_name></u>
This option sets the output file name parameter for the output file that is
the result of assembling.

Example of startup:

``` bash
./assembler -o output.o input.s
```

### Linker

This tool is used as follows
``` bash
linker [options] <input_file_name> [<input_file2_name> ...]
```

Options:

<u>-o <output_file_name></u>
This option sets the output file name parameter for the output file that is
the result of linking.

<u>-place=<section_name>@<address></u>
This option sets the address for the section specified with the 
section name. 

<u>-hex</u>
This option tells the linker to try to generate an executable file that
can then be an input to the emulator. Linking is only successful if there are
no multiple definitions for the same symbol, no unresolved symbols and no
overlap between the sections which starting addresses are defined by the place option.

<u>-relocateable</u>
This option tells the linker to generate an object program, in the same format
as the output of the assembler, ignoring the place options defined by the user.

Example of startup:

``` bash
./linker -hex -place=iv_table@0x0000 -place=text@0x4000 -o mem_content.hex input1.s input2.s
```

### Emulator

This tool is used as follows
``` bash
emulator <input_file_name>
```

Example of startup:

``` bash
./emulator mem_content.hex
```

## Tests

In the tests folder, some tests written to test the functionalities of the toolchain can
be found. Example of a shell script necessary to run the timerTest:

``` shell
ASSEMBLER=...
LINKER=...
EMULATOR=...
#Here should be the paths to the executable programs

${ASSEMBLER} -o ivt.o ivt.s
${ASSEMBLER} -o main.o main.s
${ASSEMBLER} -o timer_int.o timer_int.s
${LINKER} -hex -o program.hex -place=ivt@0x0000 ivt.o main.o timer_int.o
${EMULATOR} program.hex
```

## To-do

- Code refactorization: This project was done in a hurry, so the code
is not very comprehensible, needs to be broken down into smaller parts
including more classes and more methods.
- Translating error output and comments from Serbian to English
- Find a Windows replacement for <termios.h> header: Right now this
toolchain can only be built and run on Linux

