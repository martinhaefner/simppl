#include "simppl/cmdline_extensionpoint.h"
#include "simppl/bind_adapter.h"

#include <iostream>
#include <tuple>


using std::placeholders::_1;

namespace cmd = simppl::cmdline;


struct MyUsagePrinter : cmd::DefaultUsagePrinter
{
   template<typename InheriterT, typename ParserT>
   static
   void eval(InheriterT* usage, cmd::detail::stream_type& os, const ParserT& parser, const char* command)
   {
      if (usage)
         (*usage)(os);

      simppl::cmdline::detail::extraOptionCounter = 0;

      os << std::endl;
      os << "Usage: commands " << command << ' ';
      parser.genCmdline(os);
      os << std::endl << std::endl;

      simppl::cmdline::detail::extraOptionCounter = 0;
      parser.doDoc(os);
   }
};


bool parse_tuple(std::tuple<int, int>& tup, const char* value)
{
    int rc = sscanf(value, "%d,%d", &std::get<0>(tup), &std::get<0>(tup));
    return rc == 2;
}


int main(int argc, char** argv)
{
    bool rc;

    std::string command;

    bool debug = false;
    bool from_shared_memory;

    std::tuple<int, int> em;
    std::tuple<int, int> tt;
    std::string serial;

    if (argc > 1)
    {
        command = argv[1];

        --argc;
        ++argv;

        rc = true;
    }

    if (rc)
    {
        cmd::ExtensionPoint options = cmd::Switch<'d'>()["debug"] >> debug;

        if (!command.compare("temp"))
        {
            options += cmd::Switch<'s'>()["shared"] >> from_shared_memory;
        }
        else if (!command.compare("info"))
        {
            options += cmd::Switch<'s'>()["shared"] >> from_shared_memory;
        }
        else if (!command.compare("store"))
        {
            options += cmd::Option<cmd::NoChar>()["tt"]     >> simppl::bind(&parse_tuple, std::ref(tt), _1)
                    <= cmd::Option<cmd::NoChar>()["em"]     >> simppl::bind(&parse_tuple, std::ref(em), _1)
                    <= cmd::Option<cmd::NoChar>()["serial"] >> serial;
        }
        else if (!command.compare("-h") || !command.compare("--help"))
        {
            std::cout << "usage: commands <temp|info|store|sync|version> [args]" << std::endl;
        }
        else
        {
            std::cerr << "Unknown command '" << command << "'" << std::endl;
            return EXIT_FAILURE;
        }

        rc = cmd::Parser<cmd::ReportUnknown,
                     cmd::LongOptionSupport,
                     MyUsagePrinter>::parse(argc, argv, options);

        if (rc)
        {
            if (debug)
                std::cout << "debug enabled" << std::endl;
        }
    }

    return rc;
}
