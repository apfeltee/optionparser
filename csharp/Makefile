
default: all cs

all:
	cl -Zi -std:c++17 -clr wrap.cpp -link -dll -out:cswrap.dll

cs:
	csc -debug+ foo.cs -r:cswrap.dll

dec:
	csdec cswrap.dll -o _src


