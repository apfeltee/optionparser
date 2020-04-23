
default: all cs

## first, build cswrap.dll, which contains CLR-accessible classes
all:
	cl -Zi -std:c++17 -clr wrap.cpp -link -dll -out:cswrap.dll

## build foo.exe, and use cswrap.dll
cs:
	csc -debug+ foo.cs -r:cswrap.dll

## csdec is just an alias to dnspy.
## useful if you want to see how cl generates code!
dec:
	csdec cswrap.dll -o _src


