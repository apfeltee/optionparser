
using System;
using static OptionParser;

class Program
{
    public static void Main(string[] args)
    {
        OptionParser prs;
        //using(prs = new OptionParser())
        prs = new OptionParser();
        {
            prs.on(new[]{"-v", "--verbose"}, "enable verbose messages", (string v) =>
            {
                Console.WriteLine("toggling verbose");
            });
            prs.on(new[]{"-o?", "--out=?"}, "write output to <val>", (string v) =>
            {
                Console.WriteLine("still working on moving the value around ...");
                Console.WriteLine("got value: {0}", v);
            });
            prs.parse(args);
        }
    }
};


