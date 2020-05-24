
#include <iostream>
#include <fstream>
#include "optionparser.hpp"

struct options
{
    int verbosity = 0;
    std::string outfile = "a.out";
};

int main(int argc, char* argv[])
{
    size_t i;
    options opts;
    OptionParser prs;
    try
    {
        prs.onUnknownOption([&](const auto& v)
        {
            std::cerr << "unknown option '" << v << "'!" << std::endl;
            return false;
        });
        prs.on({"-v", "--verbose"}, "increase verbosity (try passing '-v' several times!)", [&]
        {
            opts.verbosity++;
            std::cout << "** verbosity is now " << opts.verbosity << std::endl;
        });
        prs.on({"-d", "--debug", "--toggledebug"}, "toggle debug mode", [&]
        {
            std::cout << "** toggling debug mode" << std::endl;
        });
        prs.on({"-o<file>", "--outputfile=<file>"}, "set outputfile", [&](const auto& v)
        {
            std::cout << "** outfile = '" << v.str() << "'" << std::endl;
            opts.outfile = v.str();
        });
        prs.on({"-I<path>", "-A<path>", "--include=<path>"}, "add a path to include searchpath", [&](const auto& v)
        {
            std::cout << "** include: '" << v.str() << "'" << std::endl;
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
            std::cerr << "parse error: " << ex.what() << std::endl;
        }
    }
    catch(std::runtime_error& ex)
    {
        std::cerr << "setting error: " << ex.what() << std::endl;
    }
    return 0;
}
