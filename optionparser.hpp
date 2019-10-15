/*
* Copyright 2017 apfeltee (github.com/apfeltee)
* 
* Permission is hereby granted, free of charge, to any person obtaining a copy of
* this software and associated documentation files (the "Software"), to deal in the
* Software without restriction, including without limitation the rights to use, copy, modify,
* merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
* persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
* PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
* HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <functional>
#include <exception>
#include <stdexcept>
#include <codecvt>
#include <cctype>
#if defined(__cplusplus_cli)
    #include <msclr/marshal_cppstd.h>
#endif

/* some features explicitly need minimum c++17 support */
#if ((__cplusplus != 201402L) && (__cplusplus < 201402L)) && (defined(_MSC_VER) && ((_MSC_VER != 1914) || (_MSC_VER < 1914)))
    #if !defined(_MSC_VER)
        #error "optionparser requires MINIMUM C++17!"
    #endif
#endif

/**
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
*   prs.on({"-o?", "--out=?"}, "set output file name", [&](const auto& val)
*   {
*       // 'val' is a OptionParser::Value, string value access via ::str()
*       myoutputfilename = val.str(); 
*   });
*   // without any values:
*   prs.on({"-v", "--verbose", "--setverbose"}, "toggle verbose output", [&]
*   {
*       setverbose = true;
*   });
*   // or parse(argc, argv, <index>) if argc starts at a different index
*   // alternatively, parse() may also be used with an std::vector<string>
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

/*
* note: bare-word `string` is not a typo, but typedef'd to std::basic_string!
* same with stringstream.
*/
template<typename CharT>
class BasicOptionParser
{
    public:
        struct Error: std::runtime_error
        {
            Error(const std::basic_string<CharT>& m): std::runtime_error(m)
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

        struct ValueConversionError: Error
        {
            using Error::Error;
        };

        struct IOError: Error
        {
            using Error::Error;
        };

        class Value;
        using string             = std::basic_string<CharT>;
        using stringstream       = std::basic_stringstream<CharT>;
        using StopIfCallback     = std::function<bool(BasicOptionParser&)>;
        using UnknownOptCallback = std::function<bool(const string&)>;
        using CallbackNoValue    = std::function<void()>;
        using CallbackWithValue  = std::function<void(const Value&)>;

        class Value
        {
            private:
                string m_rawvalue;

            public: // static functions - doubles as helper function(s)
                template<typename OutType>
                static OutType lexical_convert(const string& str)
                {
                    OutType dest;
                    stringstream obuf;
                    obuf << str;
                    if(!(obuf >> dest))
                    {
                        throw ValueConversionError("lexical_convert failed");
                    }
                    return dest;
                }

            public: // members
                Value(const string& raw): m_rawvalue(raw)
                {
                }

                template<typename OutType>
                OutType as() const
                {
                    return Value::lexical_convert<OutType>(m_rawvalue);
                }

                string str() const
                {
                    return m_rawvalue;
                }

                bool isEmpty() const
                {
                    return m_rawvalue.empty();
                }

                size_t size() const
                {
                    return m_rawvalue.size();
                }

                int operator[](int i) const
                {
                    return m_rawvalue[i];
                }
        };

        // some kind of bad variant type. or call it a hack.
        struct Callback
        {
            CallbackNoValue real_noval = nullptr;
            CallbackWithValue real_strval = nullptr;

            Callback()
            {
            }

            Callback(CallbackWithValue cb)
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

            void invoke(const string& s)
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
            string name;

            /*
            * this isn't really used yet - ideally, it would
            * be used to specify the type of parsing:
            * if true, it would parse GNU style options, like "--verbose", or "--prefix=foo".
            * if false, it would parse MS-DOS style options, like "/verbose" ("/v"), or "/out:whatever.txt".
            * this would also need a more thorough definition spec for short
            * options.
            *
            * tl;dr possible, but mixing these seems like a great way to needlessly
            * complicate everything
            */
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
            std::vector<CharT> shortnames;

            // the name of the long option (i.e., "out")
            std::vector<LongOption> longnames;

            // the description (no special syntax!) (i.e., "set output file name")
            string description;

            // the lambda/function to be called when this option is seen
            Callback callback;

            // a ref to OptionParser - used for alias
            BasicOptionParser* selfref;

            // return true if $c is recognized as short option
            inline bool is(CharT c) const
            {
                size_t i;
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
            inline bool is(const string& s) const
            {
                size_t i;
                for(i=0; i<longnames.size(); i++)
                {
                    if(longnames[i].name == s)
                    {
                        return true;
                    }
                }
                return false;
            }

            inline string to_short_str() const
            {
                size_t i;
                stringstream buf;
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

            inline string to_long_str(size_t padsize=35) const
            {
                size_t i;
                size_t realpad;
                size_t tmplen;
                stringstream buf;
                stringstream tmp;
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
                    if((i + 1) < longnames.size())
                    {
                        tmp << ", ";
                    }
                }
                tmplen = tmp.tellp();
                realpad = ((tmplen <= padsize) ? padsize : (tmplen + 2));
                buf << "  " << tmp.str() << ":";
                while(true)
                {
                    buf << " ";
                    if(size_t(buf.tellp()) >= realpad)
                    {
                        break;
                    }
                }
                buf << description;
                return buf.str();
            }

            Declaration& alias(const std::vector<string>& opts);
        };

        class FileParser
        {
            public:
            private:
                std::istream* m_stream;
                std::string m_filename;
                bool m_mustclose;

            private:
                std::istream* openFile(const std::string& path)
                {
                    std::fstream* strm;
                    strm = new std::fstream(path, std::ios::in | std::ios::binary);
                    return strm;
                }

                void check()
                {
                    std::stringstream ss;
                    if(!m_stream->good())
                    {
                        ss << "failed to open '" << m_filename << "' for reading";
                        throw IOError(ss.str());
                    }
                }

            public:
                FileParser(std::istream* strm, const std::string& filename, bool mustclose=false):
                    m_stream(strm), m_filename(filename), m_mustclose(mustclose)
                {
                    check();
                }

                FileParser(const std::string& path):
                    FileParser(openFile(path), true)
                {}
        };

        // wrap around isalnum to permit '?', '!', '#', etc.
        static inline bool isalphanum(CharT c)
        {
            static const string other = "?!#";
            return (std::isalnum(int(c)) || (other.find_first_of(c) != string::npos));
        }

        static inline bool isvalidlongopt(const string& str)
        {
            return ((str[0] == '-') && (str[1] == '-'));
        }

        static inline bool isvaliddosopt(const string& str)
        {
            return ((str[0] == '/') && isalphanum(str[1]));
        }

    private:
        // contains the argc/argv.
        std::vector<string> m_vargs;

        // contains unparsed, positional arguments. i.e, any non-options.
        std::vector<string> m_positional;

        // contains the option syntax declarations.
        std::vector<Declaration*> m_declarations;

        // stop_if callbacks
        std::vector<StopIfCallback> m_stopif_funcs;

        // buffer for the banner (the text printed prior to the help text)
        stringstream m_helpbannerbuf;

        // buffer for the tail (the text printed after the help text)
        stringstream m_helptailbuf;

        // true, if any DOS style options had been declared.
        // only meaningful in parse() - DOS options are usually ignored.
        bool m_dosoptsdeclared = false;

        // a handler for unknown/errornous options
        UnknownOptCallback m_on_unknownoptfn;

    private:
        /*
        * todo: more meaningful exception classes
        */
        template<typename ExClass, typename... Args>
        void throwError(Args&&... args)
        {
            string msg;
            stringstream buf;
            ((buf << args), ...);
            msg = buf.str();
            throw ExClass(msg);
        }

        inline bool invoke_on_unknown_prox(const string& optstr)
        {
            if(m_on_unknownoptfn == nullptr)
            {
                return true;
            }
            return m_on_unknownoptfn(optstr);
        }

        inline bool invoke_on_unknown(const string& str)
        {
            string ostr;
            ostr.append("--");
            ostr.append(str);
            return invoke_on_unknown_prox(ostr);
        }

        inline bool invoke_on_unknown(CharT opt)
        {
            string optstr;
            optstr.push_back('-');
            optstr.push_back(CharT(opt));
            return invoke_on_unknown_prox(optstr);
        }

        template<typename ExceptionT, typename ValType, typename... Args>
        inline void invoke_or_throw(const ValType& val, size_t& iref, size_t howmuch, Args&&... args)
        {
            size_t tmp;
            if(invoke_on_unknown(val))
            {
                throwError<ExceptionT>(args...);
            }
            tmp = (iref + howmuch);
            if((tmp + 1) != m_vargs.size())
            {
                iref = tmp;
            }
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
        inline Declaration& addDeclaration(const std::vector<string>& strs, const string& desc, Callback fn)
        {
            size_t i;
            bool isgnu;
            bool hadlongopts;
            bool hadshortopts;
            bool longwantvalue;
            bool shortwantvalue;
            Declaration* decl;
            string longstr;
            string shortstr;
            string longname;
            CharT shortname;
            CharT shortbegin;
            CharT shortend;
            CharT longbegin1;
            CharT longbegin2;
            CharT longend;
            CharT longeq;
            decl = new Declaration;
            hadlongopts = false;
            hadshortopts = false;
            longwantvalue = false;
            shortwantvalue = false;
            decl->selfref = this;
            decl->description = desc;
            decl->callback = fn;
			(void)shortbegin;
            if(strs.size() == 0)
            {
                // return, but don't actually do anything ....
                // this isn't technically an error, but it will be completely ignored
                return *decl;
            }
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
                    hadlongopts = true;
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
                    // ms-style options can be '/foo:bar', but also '-foo:bar'
                    else if((longbegin1 == '/') || (longbegin1 == '-'))
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
                        delete decl;
                        throwError<Error>("impossible situation: failed to parse '", longstr, "'");
                    }
                    decl->longnames.push_back(LongOption{longname, isgnu});
                }
                /*
                * grammar (pseudo): "-" <char:alnum> ("?")
                * also allows declaring numeric flags, i.e., "-0" (like grep, sort of)
                */
                else if((strs[i][0] == '-') && isalphanum(strs[i][1]))
                {
                    /*
                    * shortbegin is the first char in shortstr
                    * shortname is the second char in shortstr
                    * shortend is the last char in shortstr
                    */
                    hadshortopts = true;
                    shortstr = strs[i];
                    shortbegin = shortstr[0];
                    shortname = shortstr[1];
                    shortend = *(shortstr.end() - 1);
                    // permits declaring '-?'
                    shortwantvalue = ((shortend == '?') && (shortend != shortname));
                    decl->shortnames.push_back(shortname);
                }
                else
                {
                    delete decl;
                    throwError<Error>("unparseable option syntax '", strs[i],"'");
                }
            }
            /*
            if(hadlongopts || hadshortopts)
            {
                if(hadlongopts && (longwantvalue && (shortwantvalue == false)))
                {
                    shortwantvalue = true;
                }
            }
            */
            /*
            * ensure parsing state sanity: if one option requires a value, then
            * every other option must too.
            * otherwise, it would create an impossible situation, and you wouldn't
            * want to open a tear in the space-time continuum, now would you? :-)
            * but, if you do, please let me know.
            */
            if(longwantvalue == true)
            {
                if((shortwantvalue == false) && (hadshortopts == true))
                {
                    delete decl;
                    throwError<Error>("long option ended in '=?', but short option did not");
                }
            }
            if(shortwantvalue == true)
            {
                if((longwantvalue == false) && (hadlongopts == true))
                {
                    delete decl;
                    throwError<Error>("short option ended in '?', but long option did not");
                }
            }
            decl->needvalue = (longwantvalue && shortwantvalue);
            m_declarations.push_back(decl);
            return *decl;
        }

        inline bool find_decl_long(const string& name, Declaration& decldest, size_t& idxdest)
        {
            size_t i;
            for(i=0; i<m_declarations.size(); i++)
            {
                if(m_declarations[i]->is(name))
                {
                    idxdest = i;
                    decldest = *(m_declarations[i]);
                    return true;
                }
            }
            return false;
        }

        inline bool find_decl_short(CharT name, Declaration& decldest, size_t& idxdest)
        {
            size_t i;
            for(i=0; i<m_declarations.size(); i++)
            {
                if(m_declarations[i]->is(name))
                {
                    idxdest = i;
                    decldest = *(m_declarations[i]);
                    return true;
                }
            }
            return false;
        }

        /*
        * parse a short option with more than one character, OR combined options.
        */
        inline void parse_multishort(const string& str, size_t& iref)
        {
            size_t i;
            size_t idx;
            string ovalue;
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
                            throwError<ValueNeededError>("option '-", str[idx-1], "' expected a value");
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
                            throwError<ValueNeededError>("unexpected option '-", str[idx-1], "' requiring a value");
                        }
                        else
                        {
                            decl.callback.invoke();
                        }
                    }
                }
                else
                {
                    // invoke_on_unknown: multishort
                    invoke_or_throw<InvalidOptionError>(str[i], iref, 0,
                        "unknown short option '-", str[i], "'");
                    /*
                    * if we don't return here, then it will just return back to this block,
                    * unless, by chance, the string(s) happen to contain an option
                    * we can parse, and EVEN SO it would be still just a game of chance.
                    * best to go the safe way, and give up on it entirely.
                    * pessimistic, maybe, but the least likely to introduce bugs.
                    */
                    return;
                }
            }
        }

        inline void parse_simpleshort(CharT str, size_t& iref)
        {
            size_t idx;
            Declaration decl;
            string value;
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
                    throwError<ValueNeededError>("option '-", str, "' expected a value");
                }
                else
                {
                    decl.callback.invoke();
                }
            }
            else
            {
                // invoke_on_unknown: simpleshort
                invoke_or_throw<InvalidOptionError>(str, iref, 0, "unknown option '-", str, "'");
            }
        }



        /*
        * parse an argument string that matches the pattern of
        * a long option, extract its values (if any), and invoke callbacks.
        * AFAIK long options can't be combined in GNU getopt, so neither does this function.
        */
        void parse_longoption(const string& argstring, size_t& iref)
        {
            size_t idx;
			size_t eqpos;
            string name;
            string nodash;
            string value;
            Declaration decl;
            nodash = argstring.substr(2);
            eqpos = argstring.find_first_of('=');
            if(eqpos == string::npos)
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
                    if(eqpos == string::npos)
                    {
                        throwError<ValueNeededError>("longoption: option '", name, "' expected a value");
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
                // invoke_on_unknown: longoption
                invoke_or_throw<InvalidOptionError>(name, iref, 0, "unknown option '", name, "'");
            }
        }

        bool realparse()
        {
            size_t i;
            bool stopparsing;
            string nodash;
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
                            parse_longoption(m_vargs[i], i);
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
                                parse_multishort(nodash, i);
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
            size_t i;
            for(i=0; i<m_declarations.size(); i++)
            {
                buf << "[" << m_declarations[i]->to_short_str() << "]";
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
            size_t i;
            for(i=0; i<m_declarations.size(); i++)
            {
                buf << m_declarations[i]->to_long_str() << std::endl;
            }
            return buf;
        }

        void init(bool declhelp)
        {
            if(declhelp)
            {
                this->on({"-h", "--help"}, "show this help", [&]
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
        BasicOptionParser(bool declhelp=true)
        {
            init(declhelp);
        }

        virtual ~BasicOptionParser()
        {
            for(auto decl: m_declarations)
            {
                delete decl;
            }
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
        *               taking a string (i.e., [&](string s){ ... code ... }).
        *               it is only called when parsing is valid, so if "--out" was declared
        *               to take a value, but is missing a value, it is *not* called.
        *               it will also be called for *every* instance found, so
        *               for example, an option declared "-I?"  can be called multiple times
        *               to, for example, build a vector of values.
        */
        template<typename FnType>
        Declaration& on(const std::vector<string>& strs, const string& desc, FnType fn)
        {
            return addDeclaration(strs, desc, Callback{fn});
        }

        /**
        * same as on(std::vector<> ...), but provides backwards compatibility to
        * a previous version of OptionParser() which did not have variable length
        * options.
        * ##################
        * ### DEPRECATED ###
        * ##################
        */
        
        template<typename FnType>
        [[deprecated("use on<T>(const std::vector&, const string&, T)")]]
        Declaration& on(const string& shortstr, const string& longstr, const string& desc, FnType fn)
        {
            return addDeclaration(std::vector<string>{shortstr, longstr}, desc, Callback{fn});
        }

        /*
        * a specialized version of on() that passes the value as-is
        */
        /*
        Declaration& on(const std::vector<string>& strs, const string& desc, std::function<void(const std::string&)> fn)
        {
            return on(strs, desc, [&](const Value& v)
            {
                fn(v.str());
            });
        }
        */
        /* these are specialized versions of on(), that call Value::as() (except for string) */
        

        /***
        * declare a callback that is called whenever an unknown/undeclared option flag
        * is encountered.
        * the callback must match `bool(const string&)`, and must return
        * either true, or false.
        * the argument passed to the callback is the option string as it was seen
        * by the parser. note: with the exception of the amount of hyphens, you should NOT make
        * any assumptions as to what the flag might mean!
        *
        * return true:
        *   if the callback returns true, then the exception InvalidOptionError
        *   is raised, and parsing will be halted entirely.
        *   this is the default.
        *
        * return false:
        *   if it returns false, then optionparser will continue with the
        *   next option flags (if any), without raising any exceptions.
        */
        void onUnknownOption(UnknownOptCallback fn)
        {
            m_on_unknownoptfn = fn;
        }

        /**
        * reference to the help() banner stream.
        * the banner is the text shown before the help text.
        */
        inline stringstream& banner()
        {
            return m_helpbannerbuf;
        }

        /**
        * reference to the help() tail stream.
        * the tail is the text shown after the help text.
        */
        inline stringstream& tail()
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
        inline string help() const
        {
            stringstream buf;
            return help(buf).str();
        }

        /**
        * returns the positional (non-parsed) values.
        * to merely check the size of positional values, use size() instead!
        */
        inline std::vector<string> positional() const
        {
            return m_positional;
        }

        inline std::string positional(size_t idx) const
        {
            return m_positional[idx];
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
            this->stopIf([](BasicOptionParser& p)
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
        bool parse(const std::vector<string>& args)
        {
            m_vargs = args;
            return realparse();
        }

    #if defined(__cplusplus_cli)
        /*
        * this hack is only used for managed c++.
        * i've tested, and it "works" - but C++CLR guts are ***very*** poorly
        * documented, especially marshalling.
        * so, it works, but it might also not.
        */
        bool parse(cli::array<System::String^>^ args)
        {
            int i;
            msclr::interop::marshal_context ctx;
            for(i=0; i<args->Length; i++)
            {
                m_vargs.push_back(ctx.marshal_as<string>(args[i]));
            }
            return realparse();
        }
    #endif
};

using OptionParser = BasicOptionParser<char>;

// that's all, folks!
