
using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Linq;
using System.IO;
using System.Net;
using System.Text;
using System.Text.RegularExpressions;
using System.Globalization;
using System.Threading;

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
*   prs.On({"-o?", "--out=?"}, "set output file name", [&](string val)
*   {
*       myoutputfilename = val; 
*   });
*   prs.parse();
*   // unparsed values can be retrieved through prs.opts(), which is a vector of strings.
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


namespace Options
{
    using StopIfCallback   = Func<Parser, bool>; // bool(ref Parser) 

    // some kind of bad variant type. or call it a hack.
    class Callback
    {
        Action real_noval;
        Action<string> real_strval;

        public Callback()
        {
        }

        public Callback(Action<string> cb)
        {
            real_strval = cb;
        }

        public Callback(Action cb)
        {
            real_noval = cb;
        }

        public void Invoke()
        {
            if(real_noval == null)
            {
                throw new Exception("real_noval is NULL");
            }
            real_noval();
        }

        public void Invoke(string s)
        {
            if(real_strval == null)
            {
                throw new Exception("real_strval is NULL");
            }
            real_strval(s);
        }
    };

    class LongOption
    {
        public string name;
        public bool isgnu;

        public LongOption(string n, bool ig)
        {
            name = n;
            isgnu = ig;
        }
    }

    class Declaration
    {
        // whether or not this decl has a short option (wip)
        public bool has_short = false;

        // whether or not this decl has a long option (wip)
        public bool has_long = false;

        // whether this decl needs a value. this is declared
        // through appending '?' to the short opt syntax, and '=?' to
        // the long opt syntax.
        public bool needvalue = false;

        // the name of the short option (i.e., "o")
        public List<char> shortnames;

        // the name of the long option (i.e., "out")
        public List<LongOption> longnames;

        // the description (no special syntax!) (i.e., "set output file name")
        public string description;

        // the lambda/function to be called when this option is seen
        public Callback callback;

        public Declaration()
        {
            shortnames = new List<char>();
            longnames = new List<LongOption>();
        }

        // return true if $c is recognized as short option
        public  bool Like(char c)
        {
            int i;
            for(i=0; i<shortnames.Count; i++)
            {
                if(shortnames[i] == c)
                {
                    return true;
                }
            }
            return false;
        }

        // return true if $s is recognized as long option
        public bool Like(string s)
        {
            int i;
            for(i=0; i<longnames.Count; i++)
            {
                if(longnames[i].name == s)
                {
                    return true;
                }
            }
            return false;
        }

        public string ToShortStr()
        {
            int i;
            StringBuilder buf;
            buf = new StringBuilder();
            for(i=0; i<shortnames.Count; i++)
            {
                buf.AppendFormat("-{0}{1}", shortnames[i], (needvalue ? "<val>" : ""));
                if((i + 1) != shortnames.Count)
                {
                    buf.Append(" ");
                }
            }
            return buf.ToString();
        }

        public string ToLongStr(int padsize=50)
        {
            int i;
            int pad;
            int tmplen;
            StringBuilder buf;
            StringBuilder tmp;
            tmp = new StringBuilder();
            buf = new StringBuilder();
            tmp.Append(ToShortStr());
            tmp.Append(" ");
            for(i=0; i<longnames.Count; i++)
            {
                if(longnames[i].isgnu)
                {
                    tmp.AppendFormat("--{0}{1}", longnames[i].name, (needvalue ? "=<val>" : ""));
                }
                else
                {
                    tmp.AppendFormat("/{0}{1}", longnames[i].name, (needvalue ? ":<val>" : ""));
                }
                if((i + 1) != longnames.Count)
                {
                    tmp.Append(", ");
                }
            }
            tmplen = tmp.Length;
            pad = ((tmplen < padsize) ? padsize : (tmplen + 2));
            buf.Append("  ").Append(tmp).Append(":");
            while(true)
            {
                buf.Append(" ");
                if(buf.Length >= pad)
                {
                    break;
                }
            }
            buf.Append(description);
            return buf.ToString();
        }
    };

    class Parser
    {
        // contains the argc/argv.
        string[] m_vargs;

        // contains unparsed, positional arguments. i.e, any non-options.
        List<string> m_positional;

        // contains the option syntax declarations.
        List<Declaration> m_declarations;

        // stop_if callbacks
        List<StopIfCallback> m_stopif_funcs;

        // buffer for the banner (the text printed prior to the help text)
        StringBuilder m_helpbannerbuf;

        // buffer for the tail (the text printed after the help text)
        StringBuilder m_helptailbuf;

        /*
        * todo: more meaningful exception classes
        */
        void throwError(string msg)
        {
            throw new Exception(String.Format("throwError: {0}", msg));
        }

        // wrap around isalnum to permit '?', '!', '#', etc.
        bool isalphanum(char c)
        {
            bool isalnum;
            string other = "?!#";
            isalnum = (char.IsLetter(c) || char.IsNumber(c));
            return (isalnum || (other.IndexOf(c) > -1));
        }

        bool isvalidlongopt(string str)
        {
            return ((str[0] == '-') && (str[1] == '-'));
        }

        bool isvaliddosopt(string str)
        {
            return ((str[0] == '/') && isalphanum(str[1]));
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
        void AddDecl(string[] strs, string desc, Callback fn)
        {
            int i;
            bool isgnu;
            bool longwantvalue;
            bool shortwantvalue;
            Declaration decl;
            string longstr;
            string shortstr;
            string longname;
            char shortname;
            char shortbegin;
            char shortend;
            char longbegin1;
            char longbegin2;
            char longend;
            char longeq;
            longstr = null;
            shortstr = null;
            longname = null;
            longwantvalue = false;
            shortwantvalue = false;
            decl = new Declaration();
            decl.description = desc;
            decl.callback = fn;
            for(i=0; i<strs.Length; i++)
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
                    longend = longstr[longstr.Length - 1]; //(*(longstr.end() - 1));
                    longeq = longstr[longstr.Length - 2]; //(*(longstr.end() - 2));
                    isgnu = ((longbegin1 == '-') && (longbegin2 == '-'));
                    if(isgnu)
                    {
                        longwantvalue = ((longeq == '=') && (longend == '?'));
                        if(longwantvalue)
                        {
                            longname = longstr.Substring(2).Substring(0, longstr.Length - 4);
                        }
                        else
                        {
                            longname = longstr.Substring(2);
                        }
                    }
                    else if(longbegin1 == '/')
                    {
                        longwantvalue = ((longeq == ':') && (longend == '?'));
                        if(longwantvalue)
                        {
                            longname = longstr.Substring(1).Substring(0, longstr.Length - 3);
                        }
                        else
                        {
                            longname = longstr.Substring(1);
                        }
                        throwError("DOS style options are not supported");
                    }
                    else
                    {
                        throwError(String.Format("impossible situation: failed to parse '{0}'", longstr));
                    }
                    decl.longnames.Add(new LongOption(longname, isgnu));
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
                    shortend = shortstr[shortstr.Length - 1];
                    // permits declaring '-?'
                    shortwantvalue = ((shortend == '?') && (shortend != shortname));
                    decl.shortnames.Add(shortname);
                }
                else
                {
                    throwError(String.Format("unparseable option syntax '{0}'", strs[i]));
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
                    throwError("long option ended in '=?', but short option did not");
                }
            }
            if(shortwantvalue)
            {
                if(!longwantvalue)
                {
                    throwError("short option ended in '?', but long option did not");
                }
            }
            decl.needvalue = (longwantvalue && shortwantvalue);
            m_declarations.Add(decl);
        }

        bool find_decl_long(string name, ref Declaration decldest, ref int idxdest)
        {
            int i;
            for(i=0; i<m_declarations.Count; i++)
            {
                if(m_declarations[i].Like(name))
                {
                    idxdest = i;
                    decldest = m_declarations[i];
                    return true;
                }
            }
            return false;
        }

        bool find_decl_short(char name, ref Declaration decldest, ref int idxdest)
        {
            int i;
            for(i=0; i<m_declarations.Count; i++)
            {
                if(m_declarations[i].Like(name))
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
        void parse_multishort(string str)
        {
            int i;
            int idx;
            string ovalue;
            Declaration decl;
            decl = null;
            idx = 0;
            for(i=0; i<str.Length; i++)
            {
                /*
                * $idx is the index of the option in question of the multishort string.
                * i.e., if $str is "-ovd", and '-o' expects a value, then $idx-1 is the
                * index of 'o' in $str
                */
                if(find_decl_short(str[i], ref decl, ref idx))
                {
                    if(decl.needvalue && (i == 0))
                    {
                        if(str.Length > 1)
                        {
                            ovalue = str.Substring(1);
                            decl.callback.Invoke(ovalue);
                            return;
                        }
                        else
                        {
                            throwError(String.Format("option '-{0}' expected a value", str[idx-1]));
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
                            throwError(String.Format("unexpected option '-{0}', requiring a value", str[idx-1]));
                        }
                        else
                        {
                            decl.callback.Invoke();
                        }
                    }
                }
                else
                {
                    throwError(String.Format("multishort: unknown short option '{0}'", str[i]));
                }
            }
        }

        void parse_simpleshort(char str, ref int iref)
        {
            int idx;
            Declaration decl;
            string value;
            decl = null;
            idx = 0;
            if(find_decl_short(str, ref decl, ref idx))
            {
                if(decl.needvalue)
                {
                    /*
                    * decl wants a value, so grab value from the next argument, if
                    * the next arg isn't an option, and increase index
                    */
                    if((iref+1) < m_vargs.Length)
                    {
                        if(m_vargs[iref+1][0] != '-')
                        {
                            value = m_vargs[iref + 1];
                            iref++;
                            decl.callback.Invoke(value);
                            return;
                        }
                    }
                    throwError(String.Format("simpleshort: option '{0}' expected a value", str));
                }
                else
                {
                    decl.callback.Invoke();
                }
            }
            else
            {
                throwError(String.Format("simpleshort: unknown option '{0}'", str));
            }
        }

        /*
        * parse an argument string that matches the pattern of
        * a long option, extract its values (if any), and Invoke callbacks.
        * AFAIK long options can't be combined in GNU getopt, so neither does this function.
        */
        void parse_longoption(string argstring)
        {
            int idx;
            string name;
            string nodash;
            string value;
            int eqpos;
            Declaration decl;
            decl = null;
            idx = 0;
            nodash = argstring.Substring(2);
            eqpos = argstring.IndexOf('=');
            if(eqpos == -1)
            {
                name = nodash;
            }
            else
            {
                /* get name by cutting nodash until eqpos */
                name = nodash.Substring(0, eqpos - 2);
            }
            if(find_decl_long(name, ref decl, ref idx))
            {
                if(decl.needvalue)
                {
                    if(eqpos == -1)
                    {
                        throwError(String.Format("longoption: option '{0}' expected a value", name));
                    }
                    else
                    {
                        /* get value by cutting nodash after eqpos */
                        value = nodash.Substring(eqpos-1);
                        decl.callback.Invoke(value);
                    }
                }
                else
                {
                    decl.callback.Invoke();
                }
            }
            else
            {
                throwError(String.Format("longoption: unknown option '{0}'", name));
            }
        }

        void help_declarations_short(ref StringBuilder buf)
        {
            int i;
            for(i=0; i<m_declarations.Count; i++)
            {
                buf.Append("[").Append(m_declarations[i].ToShortStr()).Append("]");
                if((i + 1) != m_declarations.Count)
                {
                    buf.Append(" ");
                }
            }
        }

        void help_declarations_long(ref StringBuilder buf)
        {
            int i;
            for(i=0; i<m_declarations.Count; i++)
            {
                buf.Append(m_declarations[i].ToLongStr()).Append("\n");
            }
        }

        void init()
        {
            m_positional = new List<string>();
            m_declarations = new List<Declaration>();
            m_stopif_funcs = new List<StopIfCallback>();
            m_helpbannerbuf = new StringBuilder();
            m_helptailbuf = new StringBuilder();
            On(new[]{"-h", "-?", "--help"}, "show this help", () =>
            {
                Console.Write(Help());
                Environment.Exit(0);
                return;
            });
        }

        /**
        * additional text printed *before* option summary
        */
        public StringBuilder HelpBanner {
            get{ return m_helpbannerbuf; }
        }

        /**
        * additional text printed *after* option summary
        */
        public StringBuilder HelpTail {
            get{ return m_helptailbuf; }
        }

        /**
        * positional arguments - i.e., unparsed arguments. for example:
        *   ./foo.exe -vblah foo -ostuff bar baz
        *   # .Positional => {"foo", "bar", "baz"}
        */
        public List<string> Positional {
            get{ return m_positional; }
        }

        /**
        * returns amount of positional arguments
        */
        public int Length {
            get{ return m_positional.Count; }
        }

        /**
        * retrieve positional argument at given position
        */
        public string this[int key] {
            get{ return m_positional[key]; }
        }

        /**
        * constructor. for example, passing arguments from Main().
        * index is assumed to start at 0.
        */
        public Parser()
        {
            init();
        }

        /**
        * adds a declaration.
        * $strs is an array of option grammars. support syntaxes:
        *
        *   a short option that takes no arguments:
        *       -<char> ("-o")
        *
        *   a short option that takes one argument, such as "-ofoo", or "-o foo":
        *       -<char>? ("-o?")
        *
        *   a long option that takes no arguments:
        *       --<string> ("--outputfile")
        *
        *   a long option that takes one argument, such as "--outputfile=foo":
        *       --<string>=? ("--outputfile=?")
        *
        * $desc is the description for this option, which is used to build
        * the help summary.
        *
        * $fn is the callback delegate. Two types of delegates are supported;
        * one, taking no arguments, and two, taking one string argument.
        * Options.Parser makes no attempt to further process the strings (k-i-s-s principle :-))
        */
        public void On(string[] strs, string desc, Action fn)
        {
            AddDecl(strs, desc, new Callback(fn));
        }

        public void On(string[] strs, string desc, Action<string> fn)
        {
            AddDecl(strs, desc, new Callback(fn));
        }

        /* backwards compat */
        public void On(string shortstr, string longstr, string desc, Action fn)
        {
            AddDecl(new[]{shortstr, longstr}, desc, new Callback(fn));
        }

        public void On(string shortstr, string longstr, string desc, Action<string> fn)
        {
            AddDecl(new[]{shortstr, longstr}, desc, new Callback(fn));
        }


        public string Help()
        {
            StringBuilder buf = new StringBuilder();
            buf.Append(m_helpbannerbuf).Append("\n");
            buf.Append("usage: ");
            help_declarations_short(ref buf);
            buf.Append(" <args ...>\n\n");
            buf.Append("available options:\n");
            help_declarations_long(ref buf);
            buf.Append(m_helptailbuf).Append("\n");
            return buf.ToString();
        }

        public void StopIf(StopIfCallback cb)
        {
            m_stopif_funcs.Add(cb);
        }

        /**
        * initiates the parsing of arguments.
        */
        public bool Parse(string[] vargs)
        {
            int i;
            int j;
            bool stopparsing;
            string nodash;
            m_vargs = vargs;
            stopparsing = false;
            for(i=0; i<m_vargs.Length; i++)
            {
                for(j=0; j<m_stopif_funcs.Count; j++)
                {
                    if(m_stopif_funcs[j](this))
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
                    m_positional.Add(m_vargs[i]);
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
                            nodash = m_vargs[i].Substring(1);
                            /*
                            * arg starts with "-", but has more than one character.
                            * in this case, it could be combined options without arguments
                            * (something like '-v' for verbose, '-d' for debug, etc),
                            * but it could also be an option with argument, i.e., '-ofoo',
                            * where '-o' is the option, and 'foo' is the value.
                            */
                            if(nodash.Length > 1)
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
                                parse_simpleshort(nodash[0], ref i);
                            }
                        }
                    }
                    else
                    {
                        m_positional.Add(m_vargs[i]);
                    }
                }
            }
            return true;
        }
    }
}

namespace Test
{
    class Values
    {
        public int verbosity = 0;
        public string outfile = "a.out";
    }

    class Program
    {
        public static void Main(string[] args)
        {
            int i;
            Values opts = new Values();
            Options.Parser prs = new Options.Parser();
            prs.On("-v", "--verbose", "increase verbosity (try passing '-v' several times!)", () =>
            {
                opts.verbosity++;
                Console.WriteLine("** verbosity is now {0}", opts.verbosity);
            });
            prs.On(new[]{"-d", "--debug", "--toggledebug"}, "toggle debug mose", ()=>
            {
                Console.WriteLine("** toggling debug mode");
            });
            prs.On(new[]{"-o?", "--outputfile=?"}, "set outputfile", (string str)=>
            {
                Console.WriteLine("** outfile = '{0}'", str);
                opts.outfile = str;
            });
            prs.On(new[]{"-I?", "-A?", "--include=?"}, "add a path to include searchpath", (string str)=>
            {
                Console.WriteLine("** include: '{0}'", str);
            });
            try
            {
                prs.Parse(args);
                if((prs.Length == 0) && (args.Length == 0))
                {
                    Console.WriteLine("hlep goes here");
                    return;
                }
                else
                {
                    Console.WriteLine("** positional:");
                    for(i=0; i<prs.Length; i++)
                    {
                        Console.WriteLine("positional[{0}] = \"{1}\"", i, prs[i]);
                    }
                }
            }
            catch(Exception ex)
            {
                Console.WriteLine("Exception:\n{0}", ex);
            }
        }
    }
}

// that's all, folks!
