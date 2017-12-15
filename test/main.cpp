
#include <iostream>
#include "optionparser.hpp"

struct options
{
    bool verbose = false;
    std::string outfile = "a.out";
};

int main(int argc, char* argv[])
{
    int i;
    options opts;
    optionparser prs(argc, argv);
    prs.on("-v", "--verbose", "toggle verbose", [&](auto)
    {
        std::cout << "** toggling verbose" << std::endl;
        opts.verbose = true;
    });
    prs.on("-d", "--debug", "toggle debug mose", [&](auto)
    {
        std::cout << "** toggling debug mode" << std::endl;
    });
    prs.on("-o?", "--out=?", "set outputfile", [&](const std::string& str)
    {
        std::cout << "** outfile = '" << str << "'" << std::endl;
        opts.outfile = str;
    });
    prs.on("-I?", "--include=?", "add a path to include searchpath", [&](const std::string& str)
    {
        std::cout << "** include: '" << str << "'" << std::endl;
    });
    try
    {
        prs.parse();
        auto pos = prs.positional();
        if(pos.size() == 0)
        {
            prs.help(std::cout, argv[0]);
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
