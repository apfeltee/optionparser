
#include <iostream>
#include "optionparser.hpp"

struct options
{
    int verbosity = 0;
    std::string outfile = "a.out";
};

int main(int argc, char* argv[])
{
    int i;
    options opts;
    OptionParser prs;
    prs.on("-v", "--verbose", "increase verbosity (try passing '-v' several times!)", [&]
    {
        opts.verbosity++;
        std::cout << "** verbosity is now " << opts.verbosity << std::endl;
    });
    prs.on({"-d", "--debug", "--toggledebug"}, "toggle debug mose", [&]
    {
        std::cout << "** toggling debug mode" << std::endl;
    });
    prs.on({"-o?", "/out:?", "--outputfile=?"}, "set outputfile", [&](const std::string& str)
    {
        std::cout << "** outfile = '" << str << "'" << std::endl;
        opts.outfile = str;
    });
    prs.on({"-I?", "-A?", "--include=?"}, "add a path to include searchpath", [&](const std::string& str)
    {
        std::cout << "** include: '" << str << "'" << std::endl;
    });
    try
    {
        prs.parse(argc, argv);
        auto pos = prs.positional();
        if((pos.size() == 0) && (argc == 1))
        {
            prs.help(std::cout);
            return 1;
        }
        else
        {
            std::cout << "** positional:" << std::endl;
            for(i=0; i<pos.size(); i++)
            {
                std::cout << "  [" << i << "] " << std::quoted(pos[i]) << std::endl;
            }
        }
    }
    catch(std::runtime_error& ex)
    {
        std::cerr << "error: " << ex.what() << std::endl;
    }
    return 0;
}
