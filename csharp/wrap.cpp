
/*
* this is only needed for creating cswrap.dll
*
* this file also acts as a how-to guide on how to make
* fairly complex C++ concepts visible to C#, and also usable in C#.
* it feels very hacky, but it appears to work. this does *not* appear
* to be valid as pure cil (-clr:pure is deprecated anyway), but shows
* how to use cli::* classes, gcroot, System::* object instantiation,
* and a few bits more.
* the most annoying thing is that C++CLR is technically C++17-compatible,
* yet some typical C++17 features cannot be used, i.e., inheriting
* constructors/destructors from a superclass, i.e.,
*
*   class Foo: public Bar
*   {
*       using Bar::Bar; // <- no can do!
*   };
*
* and lambdas cannot be used in managed classes AT ALL. they
* need to be boxed around, and it's kind of awful. it works, but still.
* speaking of lambdas, C# only understands delegates, so obviously
* if a C++ class expects a lambda, then to use it in C#, the delegate
* from C# must be boxed in a C++ class, where it can be called in a lambda.
* so that's fun.
* all in all, it feels cleaner than jni. Documentation is kind of lacking, though.
*
* note: msclr::interop::marshal_context apparently cannot be
* a class variable. why? i don't know. ask microsoft.
*/

#if defined(__cplusplus_cli)
    #include <msclr/marshal_cppstd.h>
    #include <cliext/vector>
    #include <cliext/list>
    #using <System.dll>
    #using <System.Reflection.dll>
    #using <System.Collections.dll>
#endif

#include "../optionparser.hpp"

using namespace System;
using namespace System::Reflection;
using namespace System::Runtime::InteropServices;
using namespace System::Collections::Generic;

namespace Guts
{
    template<typename ListT>
    static void cliarrayToVector(ListT^ args, size_t len, std::vector<std::string>& dest)
    {
        int i;
        msclr::interop::marshal_context ctx;
        for(i=0; i<len; i++)
        {
            dest.push_back(ctx.marshal_as<std::string>(args[i]));
        }
    }
}

class ThinOptionWrapper
{
    public:
        using BaseType = BasicOptionParser<char>;

    private:
        BaseType* m_prs;
    
    public:
        ThinOptionWrapper(BaseType* p): m_prs(p)
        {
        }

        template<typename FnType>
        void wrap_on(cli::array<System::String^>^ strs, System::String^ desc, FnType^ fn)
        {
            std::vector<std::string> vec;
            msclr::interop::marshal_context ctx;
            Guts::cliarrayToVector(strs, strs->Length, vec);
            gcroot<FnType^> captured(fn);
            GC::KeepAlive(captured);
            m_prs->on(vec, ctx.marshal_as<std::string>(desc), [captured](const BaseType::Value& v)
            {
                auto str = gcnew System::String(v.str().c_str());
                captured->Invoke(str);
            });
        }

        
};

public ref class OptionParser
{
    public:
        using BaseType = BasicOptionParser<char>;
        delegate void ClrCallback(System::String^);

        ref struct Error: public System::Exception
        {
            Error(System::String^ msg): System::Exception(msg)
            {
            }
        };

        /* TODO: implement me! */
        /*
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
        */

    private:
        BaseType* m_prs;
        ThinOptionWrapper* m_thin;

    private:
        void createme()
        {
            std::cerr << "OptionParser init" << std::endl;
            m_prs = new BaseType();
            m_thin = new ThinOptionWrapper(m_prs);
        }

        void destroyme()
        {
            std::cerr << "OptionParser fini" << std::endl;
            delete m_prs;
            delete m_thin;
        }


    public:
        OptionParser()
        {
            createme();
        }

        ~OptionParser()
        {
            destroyme();
        }

        void on(cli::array<System::String^>^ strs, System::String^ desc, ClrCallback^ fn)
        {
            m_thin->wrap_on(strs, desc, fn);
        }

        /*
        * wrap around BaseType::
        */
        bool parse(cli::array<System::String^>^ args)
        {
            int i;
            msclr::interop::marshal_context ctx;
            for(i=0; i<args->Length; i++)
            {
                m_prs->cliboilerplate_pushvarg(ctx.marshal_as<std::string>(args[i]));
            }
            try
            {
                return m_prs->cliboilerplate_realparse();
            }
            catch(BaseType::Error& e)
            {
                throw gcnew Error(gcnew System::String(e.what()));
            }
            return false;
        }
    
        /*
        * cli::array<System::String^>^ is equal to string[] in C#.
        */
        cli::array<System::String^>^ positional()
        {
            size_t i;
            size_t len;
            len = m_prs->size();
            auto pos = m_prs->positional();
            auto rt = gcnew cli::array<System::String^>(m_prs->size());
            for(i=0; i<len; i++)
            {
                rt[i] = (gcnew System::String(pos[i].c_str()));
            }
            return rt;
        }
};

