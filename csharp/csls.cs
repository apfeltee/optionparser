
using System;
using System.Reflection;

class TypePrinter
{
    public void printtypes_of_assembly(Assembly asm)
    {
        foreach (Type type in asm.GetTypes())
        {
            Console.WriteLine(type.FullName);
        }
    }

    public void printtypes_of_file(string filename)
    {
        var asm = Assembly.ReflectionOnlyLoadFrom(filename);
        printtypes_of_assembly(asm);
    }
}

class Program
{
    public static void Main(string[] args)
    {
        var tp = new TypePrinter();
        tp.printtypes_of_file(args[0]);
    }
}

