
/*
* what i'm trying to do:
*  - replicate syntax and mannerisms of OptionParser in C#
*
* what works:
*  - defining options
*  - using values
*  - catching exceptions (only uses one exception class, because CLR restricts 'using' clause ...wtf)
*  - accessing positional parameters (i.e., what is left of args[] after parsing)
*
* and without having to re-do the algorithm. cool.
*/

using System;
using static OptionParser;

class Program
{
    public static void Main(string[] args)
    {
        OptionParser prs;
        Console.WriteLine("begin-parsing");
        // auto-calls .Dispose(). how neat.
        using(prs = new OptionParser())
        {
            /*
            * because CIL doesn't really let me properly check whether or not
            * a function takes an argument, every function takes an argument.
            * trying to wrap this in a OptionParser::Value is possible, but
            * is it worth it? C# already has stuff like int.Parse(), etc.
            */
            prs.on(new[]{"-v", "--verbose"}, "enable verbose messages", (string v) =>
            {
                Console.WriteLine("toggling verbose");
            });
            prs.on(new[]{"-o?", "--out=?"}, "write output to <val>", (string v) =>
            {
                Console.WriteLine("got value: {0}", v);
            });
            try
            {
                prs.parse(args);
            }
            catch(OptionParser.Error e)
            {
                Console.WriteLine("failed to parse: {0}", e);
            }
            var remainder = prs.positional();
            Console.WriteLine("positional: ({0}) {1}", remainder.Length, string.Join(" ", remainder));
        }
        Console.WriteLine("end-parsing");
    }
};


