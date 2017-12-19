
#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <functional>
#include <exception>
#include <stdexcept>

/*
* optionparser borrows the style of ruby's OptionParser, in that it
* uses callbacks. this makes it trivial to implement invocation of
* multiple values (think '-I' flag for gcc, for example), and,
* other than storing declarations in a vector, does not need to
* map out any values.
*
* the only drawback is, that value conversion (strings to integers, etc)
* has to be done manually.
*
* errors, if any, are thrown during parsing in gnopt::parse(), so
* a try/catch block would be necessary. again, this is virtually identical
* to how ruby's OptionParser does it.
*
* -----------
*
* sample usage:
*
*   optionparser prs(argc, argv); // or optionparser(argc, argv, <index>) if argc starts at a different index
*   prs.on("-o?", "--out=?", "set output file name", [&](const std::string& val)
*   {
*       myoutputfilename = val; 
*   });
*   prs.parse();
*   // unparsed values can be retrieved through prs.opts(), which is a vector of strings.
*
*/
class optionparser
{
    public:
        using StopIfCallback   = std::function<bool(optionparser&)>; 
        using CallbackNoValue  = std::function<void()>;
        using CallbackStrValue = std::function<void(const std::string&)>;
        

        // some kind of bad variant type. or call it a hack.
        struct Callback
        {
            CallbackNoValue real_noval = nullptr;
            CallbackStrValue real_strval = nullptr;

            Callback()
            {
            }

            Callback(CallbackStrValue cb)
            {
                real_strval = cb;
            }

            Callback(CallbackNoValue cb)
            {
                real_noval = cb;
            }

            void invoke() const
            {
                if(real_noval == nullptr)
                {
                    throw std::runtime_error("real_noval is NULL");
                }
                return real_noval();
            }

            void invoke(const std::string& s)
            {
                if(real_strval == nullptr)
                {
                    throw std::runtime_error("real_strval is NULL");
                }
                return real_strval(s);
            }
        };

        // idea: use vectors, to support more option variations.
        // right now this can be done by sharing a lambda object.
        struct Declaration
        {
            // whether or not this decl has a short option (wip)
            bool has_short = false;

            // whether or not this decl has a long option (wip)
            bool has_long = false;

            // whether this decl needs a value. this is declared
            // through appending '?' to the short opt syntax, and '=?' to
            // the long opt syntax.
            bool needvalue = false;

            // the raw short option syntax string. (i.e., "-o?")
            std::string shortstr;

            // the raw long option syntax string. (i.e., "--out=?")
            std::string longstr;

            // the name of the short option (i.e., "o")
            char shortname;

            // the name of the long option (i.e., "out")
            std::string longname;

            // the description (no special syntax!) (i.e., "set output file name")
            std::string description;

            // the lambda/function to be called when this option is seen
            Callback callback;

            inline bool is(char c) const
            {
                return (c == shortname);
            }

            inline bool is(const std::string& s) const
            {
                return (s == longname);
            }

            inline std::string to_short_str() const
            {
                std::stringstream buf;
                buf
                    << "-" << shortname << (needvalue ? "<val>" : "")
                ;
                return buf.str();
            }

            inline std::string to_long_str(int padsize=25) const
            {
                std::stringstream tmp;
                tmp
                    << "-" << shortname << (needvalue ? "<val>" : "")
                    << ", "
                    << "--" << longname << (needvalue ? "=<val>" : "")
                ;
                std::stringstream buf;
                buf << "  " << tmp.str() << ": " << std::setw(padsize) << description;
                return buf.str();
            }
        };

    private:
        // contains the argc/argv.
        std::vector<std::string> m_vargs;

        // contains unparsed, positional arguments. i.e, any non-options.
        std::vector<std::string> m_positional;

        // contains the option syntax declarations.
        std::vector<Declaration> m_declarations;

        // stop_if callbacks
        std::vector<StopIfCallback> m_stopif_funcs;

        // buffer for the banner (the text printed prior to the help text)
        std::stringstream m_helpbannerbuf;

        // buffer for the tail (the text printed after the help text)
        std::stringstream m_helptailbuf;

    private:
        /*
        * todo: more meaningful exception classes
        */
        template<typename ExClass=std::runtime_error>
        void throwError(const std::string& msg)
        {
            throw ExClass(msg);
        }

        void addDeclaration(const std::string& shortstr, const std::string& longstr, const std::string& desc, Callback fn)
        {
            int i;
            Declaration decl;
            std::string shortname;
            std::string longname;
            char shortbegin;
            char shortend;
            char longbegin1;
            char longbegin2;
            char longend;
            char longeq;
            decl.shortstr = shortstr;
            decl.longstr = longstr;
            decl.description = desc;
            decl.callback = fn;
            /* these are to used to check the syntax of the declaration */
            shortbegin = shortstr[0];
            shortend = (*(shortstr.end() - 1));
            longbegin1 = longstr[0];
            longbegin2 = longstr[1];
            longend = (*(longstr.end() - 1));
            longeq = (*(longstr.end() - 2));
            if(shortend == '?')
            {
                decl.needvalue = true;
                /*
                * both declarations must match in their syntax.
                * so if shortstr == '-o?', then longstr must end in '=?'.
                * there are means to do this through metaprogramming, but
                * this seems pointless to me. sorry :^)
                */
                if(longend != '?')
                {
                    throwError("long option did not end in '?', but short option did");
                }
                if(longeq != '=')
                {
                    throwError("expected '=' before '?' for long option syntax");
                }
                shortname = shortstr.substr(1).substr(0, shortstr.size() - 2);
                longname = longstr.substr(2).substr(0, longstr.size() - 4);
            }
            else
            {
                shortname = shortstr.substr(1);
                longname = longstr.substr(2);
            }
            if(shortname.size() > 1)
            {
                throwError("short option can only be one character long");
            }
            // at this point, the syntax, names, etc., are fully parsed.
            decl.shortname = shortname[0];
            decl.longname = longname;
            m_declarations.push_back(decl);
        }

        /*
        * parse an argument string that matches the pattern of
        * a long option, extract its values (if any), and invoke callbacks.
        * AFAIK long options can't be combined in GNU getopt, so neither does this function.
        */
        void parse_longoption(const std::string& argstring)
        {
            std::string name;
            std::string nodash;
            std::string value;
            std::size_t eqpos;
            Declaration decl;
            nodash = argstring.substr(2);
            eqpos = argstring.find_first_of('=');
            if(eqpos == std::string::npos)
            {
                name = nodash;
            }
            else
            {
                /* get name by cutting nodash until eqpos */
                name = nodash.substr(0, eqpos - 2);
            }
            if(find_decl_long(name, decl))
            {
                if(decl.needvalue)
                {
                    if(eqpos == std::string::npos)
                    {
                        std::stringstream b;
                        b << "longoption: option '" << name << "' expected a value";
                        throwError(b.str());
                    }
                    else
                    {
                        /* get value by cutting nodash after eqpos */
                        value = nodash.substr(eqpos-1);
                        decl.callback.invoke(value);
                    }
                }
                else
                {
                    decl.callback.invoke();
                }
            }
            else
            {
                std::stringstream b;
                b << "longoption: unknown option '" << name << "'";
                throwError(b.str());
            }
        }

        /*
        * parse a short option with more than one character, OR combined options.
        */
        void parse_multishort(const std::string& str)
        {
            int i;
            std::string ovalue;
            Declaration decl;
            for(i=0; i<str.size(); i++)
            {
                if(find_decl_short(str[i], decl))
                {
                    if(decl.needvalue && (i == 0))
                    {
                        if(str.size() > 1)
                        {
                            ovalue = str.substr(1);
                            decl.callback.invoke(ovalue);
                            return;
                        }
                        else
                        {
                            std::stringstream b;
                            b << "option '-" << decl.shortname << "' expected a value";
                            throwError(b.str());
                        }
                    }
                    else
                    {
                        /*
                        * a short option combined with other opts was passed, which
                        * also expected a value. afaik, this would result in an error
                        * in GNU getopt as well
                        */
                        if(decl.needvalue)
                        {
                            std::stringstream b;
                            b << "unexpected option '-" << decl.shortname << "', requiring a value";
                            throwError(b.str());
                        }
                        else
                        {
                            decl.callback.invoke();
                        }
                    }
                }
                else
                {
                    std::stringstream b;
                    b << "multishort: unknown short option '" << str[i] << "'";
                    throwError(b.str());
                }
            }
        }

        void parse_simpleshort(char str, int& iref)
        {
            Declaration decl;
            std::string value;
            if(find_decl_short(str, decl))
            {
                if(decl.needvalue)
                {
                    if(iref < m_vargs.size())
                    {
                        /*
                        * decl wants a value, so grab value from the next argument
                        * and increase index
                        */
                        value = m_vargs[iref + 1];
                        iref++;
                        decl.callback.invoke(value);
                    }
                    else
                    {
                        std::stringstream b;
                        b << "simpleshort: option '" << str << "' expected a value";
                    }
                }
                else
                {
                    decl.callback.invoke();
                }
            }
            else
            {
                std::stringstream b;
                b << "simpleshort: unknown option '" << str << "'";
                throwError(b.str());
            }
        }

        bool find_decl_long(const std::string& name, Declaration& decldest)
        {
            int i;
            for(i=0; i<m_declarations.size(); i++)
            {
                if(m_declarations[i].is(name))
                {
                    decldest = m_declarations[i];
                    return true;
                }
            }
            return false;
        }

        bool find_decl_short(char name, Declaration& decldest)
        {
            int i;
            for(i=0; i<m_declarations.size(); i++)
            {
                if(m_declarations[i].is(name))
                {
                    decldest = m_declarations[i];
                    return true;
                }
            }
            return false;
        }

        void init()
        {
            this->on("-h", "--help", "show this help", [&]
            {
                this->help(std::cout);
                std::exit(0);
            });
        }

    public:
        optionparser(int argc, char* argv[], int begin=1)
        {
            int i;
            for(i=begin; i<argc; i++)
            {
                m_vargs.push_back(argv[i]);
            }
            init();
        }

        optionparser(const std::vector<std::string>& args)
        {
            m_vargs = args;
            init();
        }

        void on(const std::string& shortstr, const std::string& longstr, const std::string& desc, CallbackNoValue fn)
        {
            addDeclaration(shortstr, longstr, desc, Callback{fn});
        }

        void on(const std::string& shortstr, const std::string& longstr, const std::string& desc, CallbackStrValue fn)
        {
            addDeclaration(shortstr, longstr, desc, Callback{fn});
        }

        inline std::stringstream& banner()
        {
            return m_helpbannerbuf;
        }

        inline std::stringstream& tail()
        {
            return m_helptailbuf;
        }

        inline std::string help() const
        {
            std::stringstream buf;
            return help(buf).str();
        }

        template<typename StreamT>
        StreamT& help_declarations_short(StreamT& buf) const
        {
            int i;
            for(i=0; i<m_declarations.size(); i++)
            {
                buf << "[" << m_declarations[i].to_short_str() << "]";
                if((i + 1) != m_declarations.size())
                {
                    buf << " ";
                }
            }
            return buf;
        }

        template<typename StreamT>
        StreamT& help_declarations_long(StreamT& buf) const
        {
            int i;
            for(i=0; i<m_declarations.size(); i++)
            {
                buf << m_declarations[i].to_long_str() << std::endl;
            }
            return buf;
        }

        template<typename StreamT>
        StreamT& help(StreamT& buf) const
        {
            buf << m_helpbannerbuf.str() << std::endl;
            buf << "usage: ";
            help_declarations_short(buf);
            buf << " <args ...>" << std::endl << std::endl;
            buf << "available options:" << std::endl;
            help_declarations_long(buf);
            buf << m_helptailbuf.str() << std::endl;
            return buf;
        }

        inline std::vector<std::string> positional() const
        {
            return m_positional;
        }

        inline size_t positional_size() const
        {
            return m_positional.size();
        }

        inline void stop_if(StopIfCallback cb)
        {
            m_stopif_funcs.push_back(cb);
        }

        bool parse()
        {
            int i;
            int j;
            bool stopparsing;
            std::string nodash;
            stopparsing = false;
            for(i=0; i<m_vargs.size(); i++)
            {
                /*
                * GNU behavior feature: double-dash means to stop parsing arguments
                */
                if(m_vargs[i] == "--")
                {
                    stopparsing = true;
                    continue;
                }
                for(auto iter=m_stopif_funcs.begin(); iter!=m_stopif_funcs.end(); iter++)
                {
                    if((*iter)(*this))
                    {
                        stopparsing = true;
                        break;
                    }
                }
                if(stopparsing)
                {
                    m_positional.push_back(m_vargs[i]);
                }
                else
                {
                    if(m_vargs[i][0] == '-')
                    {
                        /* arg starts with "--", so it's a long option. */
                        if(m_vargs[i][1] == '-')
                        {
                            parse_longoption(m_vargs[i]);
                        }
                        else
                        {
                            nodash = m_vargs[i].substr(1);
                            /*
                            * arg starts with "-", but has more than one character.
                            * in this case, it could be combined options without arguments
                            * (something like '-v' for verbose, '-d' for debug, etc),
                            * but it could also be an option with argument, i.e., '-ofoo',
                            * where '-o' is the option, and 'foo' is the value.
                            */
                            if(nodash.size() > 1)
                            {
                                parse_multishort(nodash);
                            }
                            else
                            {
                                /*
                                * process simple short option (e.g., "-ofoo", but also "-o" "foo")
                                * by passing current index as reference.
                                * that is, parse_simpleshort may increase index if option
                                * requires a value, otherwise i remains as-is.
                                */
                                parse_simpleshort(nodash[0], i);
                            }
                        }
                    }
                    else
                    {
                        m_positional.push_back(m_vargs[i]);
                    }
                }
            }
            return true;
        }
};

// that's all, folks!
