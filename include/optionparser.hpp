
#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <functional>
#include <exception>
#include <stdexcept>
#include <cctype>

/*
* OptionParser borrows the style of ruby's OptionParser, in that it
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
*   OptionParser prs;
*   prs.on({"-o?", "--out=?"}, "set output file name", [&](const std::string& val)
*   {
*       myoutputfilename = val; 
*   });
*   // or parse(argc, argv, <index>) if argc starts at a different index
*   // alternatively, parse() may also be used with an std::vector<std::string>
*   prs.parse(argc, argv);
*   // unparsed values can be retrieved through prs.positional(), which is a vector of strings.
*
* -----------------------------------------
*
* things to do / implement:
*   - LLVM style options (single dash + fullstring, i.e., "-use-whatever", "use-whatever" being the key)
*   - "conditional" parsing, a la /bin/find. stuff like, say, "-if -then" ... which is to say
*     the *order* is important, not the actual flow. the order is kept in the same order they're
*     declared, since that's how arrays (more specifically, std::vector) work ...
*     just need to figure out a halfway sane api perhaps?
*/
class OptionParser
{
    public:
        struct Error: std::runtime_error
        {
            Error(const std::string& m): std::runtime_error(m)
            {
            }
        };

        struct InvalidOptionError: Error
        {
            using Error::Error;
        };

        struct ValueNeededError: Error
        {
            using Error::Error;
        };

        using StopIfCallback   = std::function<bool(OptionParser&)>; 
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

        struct LongOption
        {
            std::string name;
            bool isgnu;
        };

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

            // the name of the short option (i.e., "o")
            std::vector<char> shortnames;

            // the name of the long option (i.e., "out")
            std::vector<LongOption> longnames;

            // the description (no special syntax!) (i.e., "set output file name")
            std::string description;

            // the lambda/function to be called when this option is seen
            Callback callback;

            // return true if $c is recognized as short option
            inline bool is(char c) const
            {
                int i;
                for(i=0; i<shortnames.size(); i++)
                {
                    if(shortnames[i] == c)
                    {
                        return true;
                    }
                }
                return false;
            }

            // return true if $s is recognized as long option
            inline bool is(const std::string& s) const
            {
                int i;
                for(i=0; i<longnames.size(); i++)
                {
                    if(longnames[i].name == s)
                    {
                        return true;
                    }
                }
                return false;
            }

            inline std::string to_short_str() const
            {
                int i;
                std::stringstream buf;
                for(i=0; i<shortnames.size(); i++)
                {
                    buf
                        << "-" << shortnames[i] << (needvalue ? "<val>" : "")
                    ;
                    if((i + 1) != shortnames.size())
                    {
                        buf << " ";
                    }
                }
                return buf.str();
            }

            inline std::string to_long_str(int padsize=50) const
            {
                int i;
                size_t pad;
                size_t tmplen;
                std::stringstream buf;
                std::stringstream tmp;
                tmp << to_short_str();
                tmp << " ";
                for(i=0; i<longnames.size(); i++)
                {
                    if(longnames[i].isgnu)
                    {
                        tmp << "--" << longnames[i].name << (needvalue ? "=<val>" : "");
                    }
                    else
                    {
                        tmp << "/" << longnames[i].name << (needvalue ? ":<val>" : "");
                    }
                    if((i + 1) != longnames.size())
                    {
                        tmp << ", ";
                    }
                }
                tmplen = tmp.tellp();
                pad = ((tmplen < padsize) ? padsize : (tmplen + 2));
                buf << "  " << tmp.str() << ":";
                while(true)
                {
                    buf << " ";
                    if(buf.tellp() >= pad)
                    {
                        break;
                    }
                }
                buf << description;
                return buf.str();
            }
        };

        // wrap around isalnum to permit '?', '!', '#', etc.
        static inline bool isalphanum(char c)
        {
            static const std::string other = "?!#";
            return (std::isalnum(int(c)) || (other.find_first_of(c) != std::string::npos));
        }

        static inline bool isvalidlongopt(const std::string& str)
        {
            return ((str[0] == '-') && (str[1] == '-'));
        }

        static inline bool isvaliddosopt(const std::string& str)
        {
            return ((str[0] == '/') && isalphanum(str[1]));
        }

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

        // true, if any DOS style options had been declared.
        // only meaningful in parse() - DOS options are usually ignored.
        bool m_dosoptsdeclared = false;

    private:
        /*
        * todo: more meaningful exception classes
        */
        template<typename ExClass>
        void throwError(const std::string& msg)
        {
            std::cerr << "throwError: " << msg << std::endl;
            throw ExClass(msg);
        }

        template<typename ExClass, typename... Args>
        void throwError(size_t bufsz, const char* fmt, Args&&... args)
        {
            size_t sz;
            char buf[bufsz+1];
            sz = snprintf(buf, bufsz, fmt, args...);
            return throwError<ExClass>(std::string(buf, sz));
        }

        /*
        * properly deparse declarations into Declaration types.
        * expected grammar must fit in either of these:
        *
        * short option (getopt style):
        *   # -v
        *   # -o?
        *   "-" <char:alnum> ("?")
        *
        * long option (GNU style):
        *   # --verbose
        *   # --outputfile=?
        *   "--" <string:alnum> ("=?")
        *
        * long option (DOS style):
        *   # /?
        *   # /v
        *   # /verbose
        *   # /out:?
        *   "/" <string:alnum> (":?")
        *
        * 'alnum' is alphanumeric, i.e., alphabet (uppercase & lowercase) + digits.
        */
        void addDeclaration(const std::vector<std::string>& strs, const std::string& desc, Callback fn)
        {
            int i;
            bool isgnu;
            bool longwantvalue;
            bool shortwantvalue;
            Declaration decl;
            std::string longstr;
            std::string shortstr;
            std::string longname;
            char shortname;
            char shortbegin;
            char shortend;
            char longbegin1;
            char longbegin2;
            char longend;
            char longeq;
            longwantvalue = false;
            shortwantvalue = false;
            decl.description = desc;
            decl.callback = fn;
            for(i=0; i<strs.size(); i++)
            {
                /*
                * grammar (pseudo): ("--" | "/") <string:alphabet> (("=" | ":") "?")
                *
                * if DOS syntax is used (i.e., "/out"), which would need a value, it
                * needs to be declared as "/out:?", but NOT as "/out=?".
                * likewise, if GNU syntax is used, the decl needs to be "--out=?", but
                * NOT "--out:?".
                * the DOS variant must be immediately followed by an alpanumeric char.
                */
                if(isvalidlongopt(strs[i]) || isvaliddosopt(strs[i]))
                {
                    /*
                    * longbegin1 is the first char in longstr
                    * longbegin2 is the second char in longstr
                    * longend is the last char in longstr
                    * longeq is the char before the last char in longstr
                    * if the syntax is correct, the grammar is as such (pseudo):
                    *   "--" <string> ("=?")
                    */
                    longstr = strs[i];
                    longbegin1 = longstr[0];
                    longbegin2 = longstr[1];
                    longend = (*(longstr.end() - 1));
                    longeq = (*(longstr.end() - 2));
                    isgnu = ((longbegin1 == '-') && (longbegin2 == '-'));
                    if(isgnu)
                    {
                        longwantvalue = ((longeq == '=') && (longend == '?'));
                        if(longwantvalue)
                        {
                            longname = longstr.substr(2).substr(0, longstr.size() - 4);
                        }
                        else
                        {
                            longname = longstr.substr(2);
                        }
                    }
                    else if(longbegin1 == '/')
                    {
                        longwantvalue = ((longeq == ':') && (longend == '?'));
                        if(longwantvalue)
                        {
                            longname = longstr.substr(1).substr(0, longstr.size() - 3);
                        }
                        else
                        {
                            longname = longstr.substr(1);
                        }
                        m_dosoptsdeclared = true;
                    }
                    else
                    {
                        throwError<Error>(120, "impossible situation: failed to parse '%s'", longstr.c_str());
                    }
                    decl.longnames.push_back(LongOption{longname, isgnu});
                }
                /* grammar (pseudo): "-" <char:alnum> ("?") */
                else if((strs[i][0] == '-') && isalphanum(strs[i][1]))
                {
                    /*
                    * shortbegin is the first char in shortstr
                    * shortname is the second char in shortstr
                    * shortend is the last char in shortstr
                    */
                    shortstr = strs[i];
                    shortbegin = shortstr[0];
                    shortname = shortstr[1];
                    shortend = *(shortstr.end() - 1);
                    // permits declaring '-?'
                    shortwantvalue = ((shortend == '?') && (shortend != shortname));
                    decl.shortnames.push_back(shortname);
                }
                else
                {
                    throwError<Error>(120, "unparseable option syntax '%s'", strs[i].c_str());
                }
            }
            /*
            * ensure parsing state: if one option requires a value, then
            * every other option must too
            */
            if(longwantvalue)
            {
                if(!shortwantvalue)
                {
                    throwError<Error>("long option ended in '=?', but short option did not");
                }
            }
            if(shortwantvalue)
            {
                if(!longwantvalue)
                {
                    throwError<Error>("short option ended in '?', but long option did not");
                }
            }
            decl.needvalue = (longwantvalue && shortwantvalue);
            m_declarations.push_back(decl);
        }

        bool find_decl_long(const std::string& name, Declaration& decldest, int& idxdest)
        {
            int i;
            for(i=0; i<m_declarations.size(); i++)
            {
                if(m_declarations[i].is(name))
                {
                    idxdest = i;
                    decldest = m_declarations[i];
                    return true;
                }
            }
            return false;
        }

        bool find_decl_short(char name, Declaration& decldest, int& idxdest)
        {
            int i;
            for(i=0; i<m_declarations.size(); i++)
            {
                if(m_declarations[i].is(name))
                {
                    idxdest = i;
                    decldest = m_declarations[i];
                    return true;
                }
            }
            return false;
        }

        /*
        * parse a short option with more than one character, OR combined options.
        */
        void parse_multishort(const std::string& str)
        {
            int i;
            int idx;
            std::string ovalue;
            Declaration decl;
            idx = 0;
            for(i=0; i<str.size(); i++)
            {
                /*
                * $idx is the index of the option in question of the multishort string.
                * i.e., if $str is "-ovd", and '-o' expects a value, then $idx-1 is the
                * index of 'o' in $str
                */
                if(find_decl_short(str[i], decl, idx))
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
                            throwError<ValueNeededError>(120, "option '-%c' expected a value", str[idx-1]);
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
                            throwError<ValueNeededError>(120, "unexpected option '-%c' requiring a value", str[idx-1]);
                        }
                        else
                        {
                            decl.callback.invoke();
                        }
                    }
                }
                else
                {
                    throwError<InvalidOptionError>(120, "multishort: unknown short option '-%c'", str[i]);
                }
            }
        }

        void parse_simpleshort(char str, int& iref)
        {
            int idx;
            Declaration decl;
            std::string value;
            if(find_decl_short(str, decl, idx))
            {
                if(decl.needvalue)
                {
                    /*
                    * decl wants a value, so grab value from the next argument, if
                    * the next arg isn't an option, and increase index
                    */
                    if((iref+1) < m_vargs.size())
                    {
                        /*
                        * make sure the next argument isn't some sort of option;
                        * even if it is just a double dash ("--"). if it starts with
                        * a dash, it's no good.
                        * otherwise, something like "-o -foo" would yield "-foo"
                        * as value!
                        */
                        if(m_vargs[iref+1][0] != '-')
                        {
                            value = m_vargs[iref + 1];
                            iref++;
                            decl.callback.invoke(value);
                            return;
                        }
                    }
                    throwError<ValueNeededError>(120, "simpleshort: option '-%c' expected a value", str);
                }
                else
                {
                    decl.callback.invoke();
                }
            }
            else
            {
                throwError<InvalidOptionError>(120, "simpleshort: unknown option '-%c'", str);
            }
        }

        /*
        * parse an argument string that matches the pattern of
        * a long option, extract its values (if any), and invoke callbacks.
        * AFAIK long options can't be combined in GNU getopt, so neither does this function.
        */
        void parse_longoption(const std::string& argstring)
        {
            int idx;
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
            if(find_decl_long(name, decl, idx))
            {
                if(decl.needvalue)
                {
                    if(eqpos == std::string::npos)
                    {
                        throwError<ValueNeededError>(120, "longoption: option '%s' expected a value", name.c_str());
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
                throwError<InvalidOptionError>(120, "longoption: unknown option '%s'", name.c_str());
            }
        }

        bool realparse()
        {
            int i;
            int j;
            bool stopparsing;
            std::string nodash;
            stopparsing = false;
            for(i=0; i<m_vargs.size(); i++)
            {

                for(auto iter=m_stopif_funcs.begin(); iter!=m_stopif_funcs.end(); iter++)
                {
                    if((*iter)(*this))
                    {
                        stopparsing = true;
                        break;
                    }
                }
                /*
                * GNU behavior feature: double-dash means to stop parsing arguments, but
                * only if it wasn't signalled already by stop_if
                */
                if((m_vargs[i] == "--") && (stopparsing == false))
                {
                    stopparsing = true;
                    continue;
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
                    /*
                    * todo: DOS style command parsing:
                    * only process if any DOS style options were actually declared, since
                    * this is going to cause all sorts of mixups with positional arguments.
                    * additionally, process invalid and/or unknown DOS options as positional
                    * arguments, since this is more or less what windows seems to do
                    */
                    /*
                    if(m_dosoptsdeclared)
                    {
                        ...
                    }
                    */
                    else
                    {
                        m_positional.push_back(m_vargs[i]);
                    }
                }
            }
            return true;
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

        void init(bool declhelp)
        {
            if(declhelp)
            {
                this->on({"-h", "-?", "--help"}, "show this help", [&]
                {
                    this->help(std::cout);
                    std::exit(0);
                });
            }
        }

    public:
        /**
        * initializes OptionParser().
        *
        * @param declhelp  if true, will define a default response to "-h", "-?" and "--help",
        *                  printing help to standard output, and calls exit(0).
        */
        OptionParser(bool declhelp=true)
        {
            init(declhelp);
        }

        /**
        * add an option declaration.
        *
        * @param strs   a vector of option syntaxes. for example:
        *               {"-o?", "--out=?"}
        *               declares a short option "-o" taking a value, such as "-ofoo", or "-o foo",
        *               and declares "--out", taking a value, like "--out=foo" (but *NOT* "--out foo")
        *
        * @param desc   the description for this option. it's used to generate the help()
        *               text.
        *
        * @param fn     the callback used to call when this option is seen.
        *               it can either be an empty lambda (i.e., [&]{ ... code ... }), or
        *               taking a string (i.e., [&](std::string s){ ... code ... }).
        *               it is only called when parsing is valid, so if "--out" was declared
        *               to take a value, but is missing a value, it is *not* called.
        *               it will also be called for *every* instance found, so
        *               for example, an option declared "-I?"  can be called multiple times
        *               to, for example, build a vector of values.
        */
        template<typename FnType>
        void on(const std::vector<std::string>& strs, const std::string& desc, FnType fn)
        {
            addDeclaration(strs, desc, Callback{fn});
        }

        /**
        * same as on(std::vector<> ...), but provides backwards compatibility to
        * a previous version of OptionParser() which did not have variable length
        * options.
        */
        template<typename FnType>
        void on(const std::string& shortstr, const std::string& longstr, const std::string& desc, FnType fn)
        {
            addDeclaration({shortstr, longstr}, desc, Callback{fn});
        }

        /**
        * reference to the help() banner stream.
        * the banner is the text shown before the help text.
        */
        inline std::stringstream& banner()
        {
            return m_helpbannerbuf;
        }

        /**
        * reference to the help() tail stream.
        * the tail is the text shown after the help text.
        */
        inline std::stringstream& tail()
        {
            return m_helptailbuf;
        }

        /**
        * returns a generated help text, based on the option declarations, and
        * their descriptions, as well as the banner stream, and the tail stream.
        *
        * @param buf   a ostream-compatible stream (that defines operator<<()) to
        *              write the text to.
        */
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

        /**
        * like help(StreamT&), but writes to a stringstream, and returns a string.
        */
        inline std::string help() const
        {
            std::stringstream buf;
            return help(buf).str();
        }

        /**
        * returns the positional (non-parsed) values.
        * to merely check the size of positional values, use size() instead!
        */
        inline std::vector<std::string> positional() const
        {
            return m_positional;
        }

        /**
        * returns the amount of positional values.
        */
        inline size_t size() const
        {
            return m_positional.size();
        }

        /**
        * like size(), but for C++-isms.
        */
        inline size_t length() const
        {
            return this->size();
        }

        /**
        * add a function that is called prior to each parsing loop, determining
        * whether or not to stop parsing.
        * for an example, look at the implementation of stopIfSawPositional().
        */
        inline void stopIf(StopIfCallback cb)
        {
            m_stopif_funcs.push_back(cb);
        }

        /**
        * adds a StopIfCallback causing the parser to stop parsing, and treat every
        * argument as a positional value IF a non-option (positional value) was seen.
        *
        * i.e., if argv is something like {"-o", "foo", "-I/opt/stuff", "things.c", "-I/something/else"}
        * then this would cause the parser to stop parsing after having seen "things.c", and the
        * positional values would yield {"things.c", "-I/something/else"}, which in turn
        * would NOT call the callback for the definition of "-I?", since it was not parsed.
        */
        inline void stopIfSawPositional()
        {
            this->stopIf([](OptionParser& p)
            {
                return (p.size() > 0);
            });
        }

        /**
        * populate m_vargs, and call the parser with argc/argv as it were passed
        * to main().
        *
        * @param argc    the argument vector count.
        * @param argv    the argument vector values.
        * @param begin   the index at which to begin collecting values.
        *                there is usually no reason to specify this explicitly.
        */
        bool parse(int argc, char** argv, int begin=1)
        {
            int i;
            for(i=begin; i<argc; i++)
            {
                m_vargs.push_back(argv[i]);
            }
            return realparse();
        }

        /**
        * like parse(int, char**, int), but with a std::vector.
        * unlike parse(int, char**, int) however, it will assume that the
        * index starts at 0.
        */
        bool parse(const std::vector<std::string> args)
        {
            m_vargs = args;
            return realparse();
        }
};

// that's all, folks!
