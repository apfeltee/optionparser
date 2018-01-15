
## OptionParser

OptionParser borrows the style of ruby's OptionParser, in that it
uses lambdas to capture values.

this makes it trivial to implement invocation of
multiple values (think '-I' flag for gcc, for example), and,
other than storing declarations in a vector, does not need to
map out any values.

the only drawback is, that value conversion (strings to integers, etc) has to be done manually.

errors, if any, are thrown during parsing in OptionParser::parse(), so
a try/catch block would be necessary. This is virtually identical
to how ruby's OptionParser does it.


sample usage:

    optionparser prs;
    prs.on("-o?", "--out=?", "set output file name", [&](const std::string& val)
    {
       myoutputfilename = val; 
    });
    // or parse(argc, argv, <index>) if argc starts at a different index
    prs.parse(argc, arv);
    // unparsed values can be retrieved through prs.positional(), which is a vector of strings.


A more hands-on example is available in `test/main.cpp`.

### dependencies

A c++14 capable compiler. Recent versions of gcc, clang, and Visual Studio will work.

### licensing

optionparser is made available under the terms of the MIT/X11 License.  
A full copy of the license text is available in the file `LICENSE`.



