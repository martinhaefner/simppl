#ifndef SIMPPL_CMDLINE_H
#define SIMPPL_CMDLINE_H


#include <limits>
#include <cstring>
#include <cstdlib>

// add support for string and vector in option handlers
#define CMDLINE_HAVE_STD_STRING 1
#define CMDLINE_HAVE_STD_VECTOR 1

// undefine this if you want to generate parser output via printf instead of STL streams
#define CMDLINE_HAVE_STL_STREAMS 1

#ifdef CMDLINE_HAVE_STD_STRING
#   include <string>
#endif

#ifdef CMDLINE_HAVE_STD_VECTOR
#   include <vector>
#endif

#ifndef NDEBUG
#   define CMDLINE_ENABLE_RUNTIME_CHECK 1
#   include <set>
#endif

#ifdef CMDLINE_HAVE_STL_STREAMS
#   include <iostream>
#   include <iomanip>

// that's how the parser help output is formatted - change the define prior to the inclusion to a value that
// fits your needs if you want to change the indentation.
#   ifndef PARSER_DESCRIPTION_INDENTATION
#     define PARSER_DESCRIPTION_INDENTATION 20
#   endif
  
#   define PARSER_STDOUT_STREAM std::cout
#   define PARSER_STDERR_STREAM std::cerr

#else
#include <cstdio>

#   define PARSER_STDOUT_STREAM stdout
#   define PARSER_STDERR_STREAM stderr
#endif

#include "tribool.h"
#include "typelist.h"
#include "noninstantiable.h"


namespace simppl
{
   
namespace cmdline
{

/*TODO move to impl file and declare extern*/
static const char NoChar = '*';
   
namespace detail
{
 
// forward decls
template<char, typename, typename, typename, bool> 
struct LongSwitch;   

template<typename, typename> 
struct HandlerComposite;

template<typename>
struct HandlerCompositeBase;
   

#ifdef CMDLINE_HAVE_STL_STREAMS
typedef std::ostream stream_type;
#else
typedef FILE* stream_type;
#endif


/*TODO move to impl file and declare extern*/
static bool longOptionSupport = false;   ///< FIXME this is a runtime variable which cannot be optimized out at compile-time
static int extraOptionCounter = 0;
   
struct NoChar;                ///< extra options make this entry into the typelist if they are not mandatory
struct MandatoryNoChar;       ///< extra options make this entry into the typelist if they are mandatory
struct MultiNoChar;           ///< extra options make this entry into the typelist if they are a multi-type

/**
 * Internal parser state.
 */
struct ParserState
{
   ParserState(int argc, char** argv)
    : argc_(argc)
    , argv_(argv)
    , current_(1)   // skip application name
    , argument_(0)
   {
      //::memset(handled_, 0 , sizeof(handled_));
   }
   
   const char* progname() const
   {
      // strip off a leading path
      char* ptr = argv_[0] + strlen(argv_[0]);
      
      while(ptr > argv_[0] && *ptr != '/')
         --ptr;
      
      if (*ptr == '/')
         ++ptr;
      
      return ptr;
   }
  
   int argc_;
   char** argv_;
   
   int current_;           ///< current position in argv
   const char* argument_;  ///< current position of argument (_very_ mutable member)
   
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK
   std::set<char> arguments_;
   std::set<const char*> longArguments_;
#endif
};


/// mandatory extra options after optional ones are not assignable
template<typename ListT>
struct CheckMandatoryAfterOptionalExtraOption
{
   enum 
   { 
      idx_first_optional = Find<NoChar, ListT>::value,
      idx_followed_mandatory = idx_first_optional != -1 ? Find<MandatoryNoChar, typename PopFrontN<idx_first_optional, ListT>::type>::value : -1,
      
      value = idx_first_optional == -1 || idx_followed_mandatory == -1 || (idx_first_optional != -1 && idx_followed_mandatory == -1)
   };
};


/// multiple optional extra options are forbidden, use the Multi-policy instead
template<typename ListT>
struct CheckMultipleOptionalExtraOptions
{
   enum { value = Count<NoChar, ListT>::value < 2 };
};


/// find an extra option in the given list and return the  (quite arbitrary) position
template<typename ListT>
struct FindExtra
{
   enum
   {
      idx_mandatory = Find<MandatoryNoChar, ListT>::value,
      idx_optional = Find<NoChar, ListT>::value,
      idx_multi = Find<MultiNoChar, ListT>::value,
      
      value = idx_mandatory + idx_optional + idx_optional + 2   // position is somewhat senseless but we are only interested in if there is one
   };
};


/// check if there are further extra options after a given MultiExtraOption which is currently not parseable
template<typename ListT>
struct CheckFurtherExtraOptionAfterMultiExtraOption
{
   enum 
   {
      idx_first_multi = Find<MultiNoChar, ListT>::value,
      idx_followed_extra = idx_first_multi != -1 ? FindExtra<typename PopFrontN<idx_first_multi, ListT>::type>::value : -1,
      
      value = idx_first_multi == -1 || idx_followed_extra == -1 || (idx_first_multi != -1 && idx_followed_extra == -1)
   };
};


template<typename T>
struct IsExtraOption
{
   enum { value = std::is_same<NoChar, T>::value || std::is_same<MandatoryNoChar, T>::value || std::is_same<MultiNoChar, T>::value };
};


// construct a complete typelist from simple type
template<typename T>
struct MakeTypeList
{
   typedef TypeList<T, NilType> type;
};

// do nothing, it is already a typelist
template<typename A, typename B>
struct MakeTypeList<TypeList<A, B> >
{
   typedef TypeList<A, B> type;
};


}   // namespace detail


// -----------------------------------------------------------------------------------------------------


/// Just ignore unknown (and therefore unhandled) options.
class IgnoreUnknown : NonInstantiable
{   
protected:
   
   static inline
   bool eval(simppl::cmdline::detail::ParserState&)
   {
      return true;
   }
};




/// Report unknown (and therefore unhandled) options. Make the parser returning a failure.
class ReportUnknown : NonInstantiable
{
protected:
   
   static inline
   bool eval(simppl::cmdline::detail::ParserState& state)
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      PARSER_STDERR_STREAM << "Unknown or multiple option '" << state.argv_[state.current_] << "' encountered." << std::endl;
#else
      fprintf(PARSER_STDERR_STREAM, "Unknown or multiple option '%s' encountered.\n", state.argv_[state.current_]);
#endif      
      return false;   
   }
};


// -----------------------------------------------------------------------------------------------------


/// Print the parsers usage string.
class DefaultUsagePrinter 
{
   template<typename> friend struct LongOptionSupport;
   template<typename> friend struct NoLongOptionSupport;
   
protected:
   
   inline
   void operator()(detail::stream_type&)
   {
      // NOOP
   }
   
   template<typename InheriterT, typename ParserT>
   static 
   void eval(InheriterT* usage, detail::stream_type& os, const ParserT& parser, const char* progname)
   {
      if (usage)
         (*usage)(os);
      
      simppl::cmdline::detail::extraOptionCounter = 0;
      
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << std::endl;
      os << "Usage: " << progname << ' ';
      parser.genCmdline(os);
      os << std::endl << std::endl;
#else
      fprintf(os, "\nUsage: %s ", progname);
      parser.genCmdline(os);
      fprintf(os, "\n\n");
#endif
      
      simppl::cmdline::detail::extraOptionCounter = 0;
      parser.doDoc(os);
   }
};


/// Do not print anything about the usage (less memory footprint and code size).
class NoopUsagePrinter
{
   template<typename> friend struct LongOptionSupport;
   template<typename> friend struct NoLongOptionSupport;
   
protected:
   
   inline
   void operator()(detail::stream_type&)
   {
      // NOOP
   }
   
   template<typename InheriterT, typename ParserT>
   static inline
   void eval(InheriterT* usage, detail::stream_type& os, const ParserT&, const char*)
   {
      if (usage)
         (*usage)(os);
   }
};


// -----------------------------------------------------------------------------------------------------


/// Enable long options.
template<typename UsagePrinterT>
struct LongOptionSupport : NonInstantiable
{
   enum { longSupport = true };
   
   template<typename ParserT>
   static
   tribool eval(ParserT& parser, simppl::cmdline::detail::ParserState& state)
   {
      tribool rc;
      
      if (state.argv_[state.current_][2] != '\0')
         rc = parser.eval(state.argv_[state.current_]+2, state);
      
      if (indeterminate(rc))
      {
         if (!strcmp(state.argv_[state.current_]+2, "help"))  
         {
            UsagePrinterT usage;
            UsagePrinterT::eval(&usage, PARSER_STDERR_STREAM, parser, state.progname());
            exit(EXIT_SUCCESS);
         }
      }
      
      return rc;
   }
};


/// Disable long options, even for those registered as long options.
template<typename UsagePrinterT>
struct NoLongOptionSupport : NonInstantiable
{
   enum { longSupport = false };
   
   template<typename ParserT>
   static inline
   tribool eval(ParserT&, simppl::cmdline::detail::ParserState&)
   {
      // never accept long-only arguments in NoLongOptionSupport policy chosen
      typedef typename Reverse<typename simppl::cmdline::detail::MakeTypeList<typename ParserT::arguments_type>::type>::type arguments_type;
      static_assert(Find<char_< simppl::cmdline::NoChar>, arguments_type>::value < 0, "no_nochar_options_allowed_with_no_long_option_support_policy");
      
      return indeterminate;
   }
};
 

// -----------------------------------------------------------------------------------------------------
   

/**
 * The commandline parser generator.
 */
template<typename UnknownPolicyT = ReportUnknown
        , template<typename> class LongOptionSupportPolicyT = LongOptionSupport
        , typename UsagePrinterT = DefaultUsagePrinter>
struct Parser : protected UnknownPolicyT, protected UsagePrinterT
{
   // TODO could be a ParserT&& argument when -std=c++0x support is enabled
   // so the const_cast could be avoided in order to remove the 'const' on all handlers
   template<typename ParserT>
   static
   bool parse(int argc, char** argv, const ParserT& parser)
   {
      typedef typename Reverse<typename simppl::cmdline::detail::MakeTypeList<typename ParserT::arguments_type>::type>::type arguments_type;

      // checks on extra options
      static_assert(simppl::cmdline::detail::CheckMandatoryAfterOptionalExtraOption<arguments_type>::value, "mandatory_after_optional_extraoption_is_nonsense");
      static_assert(simppl::cmdline::detail::CheckMultipleOptionalExtraOptions<arguments_type>::value, "multiple_optional_extraoptions_are_nonsense");
      static_assert(simppl::cmdline::detail::CheckFurtherExtraOptionAfterMultiExtraOption<arguments_type>::value, "no_extra_option_after_multi_extra_option_allowed");
      
      ParserT& _parser = const_cast<ParserT&>(parser);
      return doParse(argc, argv, _parser);
   }
   
 
private:
   
   static 
   bool evaluateReturnValue(tribool rc, simppl::cmdline::detail::ParserState& state)
   {
      if (indeterminate(rc))
      {
         if (!UnknownPolicyT::eval(state))
            return false;
      }
      else if (!rc)
      {      
#ifdef CMDLINE_HAVE_STL_STREAMS         
         PARSER_STDERR_STREAM << "Error in evaluation of option '" << state.argv_[state.current_] << "'." << std::endl;
#else
         fprintf(PARSER_STDERR_STREAM, "Error in evaluation of option '%s'.\n", state.argv_[state.current_]);
#endif
         return false;
      }
      
      return true;
   }
   
   
   template<typename ParserT>
   static
   bool doParse(int argc, char** argv, ParserT& parser)
   {
      typedef typename simppl::cmdline::detail::MakeTypeList<typename ParserT::arguments_type>::type arguments_type;
      simppl::cmdline::detail::longOptionSupport = LongOptionSupportPolicyT<UsagePrinterT>::longSupport;
      
      // don't accept -h
      static_assert(Find<char_<'h'>, arguments_type>::value < 0, "h_is_reserved_for_help");
          
      simppl::cmdline::detail::ParserState state(argc, argv);

#ifdef CMDLINE_ENABLE_RUNTIME_CHECK   
      if (!parser.plausibilityCheck(state) || state.longArguments_.find("help") != state.longArguments_.end())
      {
#   ifdef CMDLINE_HAVE_STL_STREAMS
         PARSER_STDOUT_STREAM << "parser grammar error - multiple same switch arguments or 'help' detected." << std::endl;
#   else
         fprintf(PARSER_STDOUT_STREAM, "parser grammar error - multiple same switch arguments or 'help' detected.\n");
#   endif
         ::abort();
      }
      else
      {
         state.arguments_.clear();
         state.longArguments_.clear();
      }
#endif
      
      tribool rc;
      
      for (; state.current_ < argc; ++state.current_)
      {
         if (argv[state.current_][0] == '-')
         {
            if (argv[state.current_][1] == '-')
            {
               rc = LongOptionSupportPolicyT<UsagePrinterT>::eval(parser, state);
            }
            else if (argv[state.current_][1] == 'h' && argv[state.current_][2] == '\0')
            {
               UsagePrinterT usage;
               UsagePrinterT::eval(&usage, PARSER_STDOUT_STREAM, parser, state.progname());
               exit(EXIT_SUCCESS); 
            }
            else
            {
               // enable old-style -xvf like in 'tar', 
               const char* c = argv[state.current_];
               while(*++c != '\0')
               {
                  rc = parser.eval(*c, state);
                  if (!evaluateReturnValue(rc, state))
                     return false;
               }
            }
         }
         else
            rc = parser.eval(state);
         
         if (!evaluateReturnValue(rc, state))
            return false;
      }
      
      if (!parser.ok())
      {
#ifdef CMDLINE_HAVE_STL_STREAMS
         PARSER_STDOUT_STREAM << "Missing mandatory options." << std::endl;
#else
         fprintf(PARSER_STDOUT_STREAM, "Missing mandatory options.\n");
#endif
         UsagePrinterT::eval((UsagePrinterT*)0, PARSER_STDERR_STREAM, parser, state.progname());
         return false;
      }
      
      return true;
   }
};


// ------------------------------------------------------------------------------------


namespace detail
{
   
template<char Argument>
struct DocumentationHelper
{
   static inline 
   void eval(detail::stream_type& os, const char* longopt, const char* doc)
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << '-' << Argument << "|--" << std::left << std::setw(PARSER_DESCRIPTION_INDENTATION-4) << longopt << doc << std::endl;  
#else
      fprintf(os, "-%c|--%-16s %s\n", Argument, longopt, doc);
#endif         
   }
};

template<>
struct DocumentationHelper<simppl::cmdline::NoChar>
{
   static inline 
   void eval(detail::stream_type& os, const char* longopt, const char* doc)
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << "--" << std::left << std::setw(PARSER_DESCRIPTION_INDENTATION-1) << longopt << doc << std::endl;  
#else
      fprintf(os, "--%-19s %s\n", longopt, doc);
#endif
   }
};


/// Documentation policy for command line argument.
struct Documentation
{
   inline
   Documentation(const char* doc)
    : doc_(doc)
   {
      // NOOP
   }
   
   // switches
   template<char Argument>
   inline
   void doDoc_(detail::stream_type& os) const
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << '-' << std::left << std::setw(PARSER_DESCRIPTION_INDENTATION) << Argument << doc_ << std::endl;
#else
      fprintf(os, "-%-20c %s\n", Argument, doc_);
#endif
   }
  
   /// long option support
   template<char Argument>
   inline
   void doDoc_(detail::stream_type& os, const char* longopt) const
   {
      detail::DocumentationHelper<Argument>::eval(os, longopt, doc_);
   }
   
   /// extra option support
   inline
   void doDoc_(detail::stream_type& os) const
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
	  ++simppl::cmdline::detail::extraOptionCounter;
      os << "<arg" << simppl::cmdline::detail::extraOptionCounter << std::left << std::setw(PARSER_DESCRIPTION_INDENTATION-4-simppl::cmdline::detail::extraOptionCounter/10) << "> " << doc_ << std::endl;
#else
      fprintf(os, "<arg%d>%-15c %s\n", ++simppl::cmdline::detail::extraOptionCounter, ' ', doc_);
#endif
   }
   
   const char* doc_;
};


/// Documentation policy for command line argument - a NOOP.
struct NoDocumentation
{
   template<char Argument>
   inline
   void doDoc_(detail::stream_type&) const
   {
      // NOOP
   }
   
   template<char Argument>
   inline
   void doDoc_(detail::stream_type&, const char* /*longopt*/) const
   {
      // NOOP
   }
   
   inline
   void doDoc_(detail::stream_type&) const
   {
      // NOOP
   }
};

}   // end namespace detail


// ---------------------------------------------------------------------------


/**
 * Argument policy. An argument using this policy is optional and must not be 
 * given at the commandline.
 */
class Optional
{
   template<char, typename, typename, typename, bool> friend struct detail::LongSwitch;
   template<typename, typename> friend struct detail::HandlerComposite;

protected:
   
   inline
   bool ok() const
   {
      return true;
   }
   
   inline
   bool ok(bool b)
   {
      return b;
   }
   
   template<bool HaveArgument>
   inline
   void genCmdline(detail::stream_type& os, char arg) const
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << "[-" << arg << (HaveArgument?" <arg>] ":"] ");
#else
      fprintf(os, "[-%c%s] ", arg, HaveArgument?" <arg>":"");
#endif
   }
   
   template<char Argument, bool HaveArgument>
   struct Helper
   {
      static inline 
      void eval(detail::stream_type& os, const char* long__)
      {
#ifdef CMDLINE_HAVE_STL_STREAMS
         os << "[-" << Argument << (HaveArgument?" <arg>|--":"|--") << long__ << (HaveArgument?"=<arg>] ":"] ");
#else
         fprintf(os, "[-%c %s|--%s%s] ", Argument, long__, HaveArgument?" <arg>":"", HaveArgument?"=<arg>":"");
#endif
      }
   };

   template<bool HaveArgument>
   struct Helper<simppl::cmdline::NoChar, HaveArgument>
   {
      static inline 
      void eval(detail::stream_type& os, const char* long__)
      {
#ifdef CMDLINE_HAVE_STL_STREAMS
         os << "[--" << long__ << (HaveArgument?"=<arg>] ":"] ");
#else
         fprintf(os, "[--%s%s] ", long__, HaveArgument?"=<arg>":"");
#endif
      }
   };

   template<char Argument, bool HaveArgument>
   inline
   void genCmdline(detail::stream_type& os, const char* long__) const
   {
      Helper<Argument, HaveArgument>::eval(os, long__);
   }   

   inline
   void genCmdline(detail::stream_type& os, const char* ext) const
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << "[<arg" << ext << ">] ";
#else
      fprintf(os, "[<arg%s>] ", ext);
#endif
   }
};


/**
 * Argument Policy. The using argument must be given at the commandline, otherwise the parser
 * reports an error.
 */
class Mandatory
{
   template<char, typename, typename, typename, bool> friend struct detail::LongSwitch;
   template<typename, typename> friend struct detail::HandlerComposite;

protected:

   inline
   Mandatory()
    : ok_(false)
   {
      // NOOP
   }
   
   inline
   bool ok() const
   {
      return ok_;
   }
   
   inline
   bool ok(bool b)
   {
      ok_ |= b;   // set it to true one time, dont change later on
      return b;
   }
   
   template<bool HaveArgument>
   inline
   void genCmdline(detail::stream_type& os, char arg) const
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << '-' << arg << (HaveArgument?" <arg> ":" ");
#else
      fprintf(os, "-%c %s", arg, HaveArgument?"<arg> ":"");
#endif

   }

   template<char Argument, bool HaveArgument>
   struct Helper
   {
      static inline 
      void eval(detail::stream_type& os, const char* long__)
      {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << '-' << Argument << (HaveArgument?" <arg>|--":"|--") << long__ << (HaveArgument?"=<arg> ":" ");
#else
      fprintf(os, "-%c %s|--%s=%s", Argument, HaveArgument?"<arg>":"", long__, HaveArgument?"<arg> ":"");
#endif
      }
   };

   template<bool HaveArgument>
   struct Helper<simppl::cmdline::NoChar, HaveArgument>
   {
      static inline 
      void eval(detail::stream_type& os, const char* long__)
      {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << "--" << long__ << (HaveArgument?"=<arg> ":" ");
#else
      fprintf(os, "--%s=%s", long__, HaveArgument?"<arg> ":"");
#endif
      }
   };
   
   template<char Argument, bool HaveArgument>
   inline
   void genCmdline(detail::stream_type& os, const char* long__) const
   {
      Helper<Argument, HaveArgument>::eval(os, long__);
   }
 
   inline  
   void genCmdline(detail::stream_type& os, const char* ext) const
   {
#ifdef CMDLINE_HAVE_STL_STREAMS
      os << "<arg" << ++simppl::cmdline::detail::extraOptionCounter << ext << "> ";
#else
      fprintf(os, "<arg%d%s>", ++simppl::cmdline::detail::extraOptionCounter, ext);
#endif
   }
   
   bool ok_;
};


// ---------------------------------------------------------------------------


namespace detail
{

/// checker for options
template<bool>
struct OptionChecker
{
   static 
   bool eval(ParserState& state)
   {
      // make sure there is an option
      if (state.current_ < state.argc_ && *state.argv_[state.current_] != '-')
      {
         state.argument_ = state.argv_[state.current_];
         return true;
      }
      
      return false;
   }
};


/// checker for switches (trivial)
template<>
struct OptionChecker<false>
{
   static inline
   bool eval(ParserState&)
   {
      return true;
   }
};


template<char Argument>
struct EqualityComparator
{
   static inline
   bool eval(char c)
   {
      return Argument == c;
   }
};

struct InEquality
{
   static inline
   bool eval(char)
   {
      return false;
   }
};


static /*TODO move to impl file*/
bool checkOption(char theChar, char Argument, ParserState& state)
{
   const char* arg = state.argv_[state.current_];
   
   // must not be already given
   //if (::strchr(state.handled_, theChar) == 0)
   //{
      // add the option to the list of handled options to avoid multiple defined options
      //::strncat(state.handled_, &theChar, 1);
   
      // make sure it is the last in a multi commandline arg like e.g. in tar xvzf <filename>
   return arg[2] == '\0' || arg[strlen(arg)-1] == Argument;
   //}
   
   //return false;
}


template<bool>
struct LongOptionChecker
{
   static
   bool eval(const char* option, const char* current, ParserState& state)
   {
      int len = strlen(option);
      if (!::strncmp(option, current, len) && current[len] == '=')
      {
         state.argument_ = current + len + 1;
         return true;
      }
      return false;
   }
};


template<>
struct LongOptionChecker<false>
{
   static inline
   bool eval(const char* option, const char* current, ParserState&)
   {
      return !::strcmp(option, current);
   }
};


struct MultiSupport
{
   inline
   bool handled() const
   {
      return false;
   }
   
   inline
   bool isHandled() const
   {
      return false;
   }
   
   inline
   bool setHandled()
   {
      return true;
   }
   
   inline
   const char* cmdlineExtension() const
   {
      return ", ...";
   }
};


struct NoMultiSupport
{
   inline
   NoMultiSupport()
    : handled_(false)
   {
      // NOOP
   }
   
   inline
   bool handled()
   {
      bool rc = handled_;
      handled_ = true;
      return rc;
   }
   
   inline
   bool isHandled() const
   {
      return handled_;
   }
   
   inline
   bool setHandled()
   {
      handled_ = true;
      return handled_;
   }
   
   inline
   const char* cmdlineExtension() const
   {
      return "";
   }
   
   bool handled_;
};


// template forward declaration
template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, bool haveArgument = false>
struct LongSwitch;

}  // namespace detail


/**
 * The baseclass for all options and switches.
 */
template<char Argument = simppl::cmdline::NoChar, typename MandatoryT = Optional, typename MultiSupportT = simppl::cmdline::detail::MultiSupport, 
         typename DocumentationT = detail::NoDocumentation, bool haveArgument = false>
struct Switch : MandatoryT, DocumentationT, MultiSupportT
{   
   template<typename, typename> friend struct detail::HandlerComposite; 
   template<typename> friend struct detail::HandlerCompositeBase;
   template<char, typename, typename, typename, bool> friend struct detail::LongSwitch;

   static_assert(haveArgument == true || std::is_same<MandatoryT, Optional>::value, "mandatory_switches_are_senseless");
   static_assert((Argument >= 48 && Argument <=57) 
             || (Argument >=65 && Argument <=90) 
             || (Argument >=97 && Argument <=122) 
             || Argument == simppl::cmdline::NoChar, 
              "switch_argument_only_valid_in_0123456789abcdefgijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");
   
   typedef char_<Argument> char_type;
   typedef typename if_<Argument== simppl::cmdline::NoChar, simppl::cmdline::detail::InEquality, simppl::cmdline::detail::EqualityComparator<Argument> >::type equality_check_type;   
   
   enum { LongSupport = false, HaveArgument = haveArgument };

   
protected:
   
   inline explicit
   Switch(const char* doc)
    : MandatoryT()
    , MultiSupportT()
    , DocumentationT(doc)
   {
      // NOOP
   }
   
   
public:
   
   inline
   Switch()
    : MandatoryT()
    , MultiSupportT()
    , DocumentationT()
   {
      // NOOP
   }
   
   inline
   Switch<Argument, MandatoryT, MultiSupportT, detail::Documentation, haveArgument>
   doc(const char* doc)
   {
      return Switch<Argument, MandatoryT, MultiSupportT, detail::Documentation, haveArgument>(doc);
   }
   
   inline
   detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument> 
   operator[](const char* long__)
   {
      return detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument>(long__);
   }


protected:
   
   bool operator()(char c, detail::ParserState& state)
   {
      if (!MultiSupportT::isHandled())
      {
         return MandatoryT::ok(equality_check_type::eval(c) && (HaveArgument == false ? true : detail::checkOption(c, Argument, state)) && MultiSupportT::setHandled());         
      }
      else
         return false;
   }
   
   inline
   void doDoc(detail::stream_type& os) const
   {
      DocumentationT::template doDoc_<Argument>(os);
   } 
   
   inline
   void genCmdline(detail::stream_type& os) const
   {
      MandatoryT::template genCmdline<HaveArgument>(os, Argument);
   }  

   inline
   void genCmdline(detail::stream_type& os, const char* long__) const
   {
      MandatoryT::template genCmdline<Argument, haveArgument>(os, long__);
   }  
   
   inline
   bool ok() const
   {
      return MandatoryT::ok();
   }
   
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK 
   // this check here is mainly interesting only if extension points are part of the grammar
   // TODO maybe we can determine whether we need to do that or not before so no code is generated if possible
   inline 
   bool plausibilityCheck(simppl::cmdline::detail::ParserState& state) const
   {
      if (Argument == simppl::cmdline::NoChar)
      {
         return true;
      }
      else
      {
         bool rc = state.arguments_.find(Argument) == state.arguments_.end();
         state.arguments_.insert(Argument);
         return rc;
      }
   }
#endif
};


/**
 * An ordinary commandline option.
 */
template<char Argument = simppl::cmdline::NoChar, typename MandatoryT = Optional, typename MultiSupportT = simppl::cmdline::detail::NoMultiSupport, typename DocumentationT = detail::NoDocumentation>
struct Option : Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>
{
   template<typename> friend struct detail::HandlerCompositeBase;
   
   inline
   Option()
    : Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>()
   {
      // NOOP
   }
};


/**
 * A commandline option that could appear multiple times.
 */
template<char Argument = simppl::cmdline::NoChar, typename MandatoryT = Optional, typename DocumentationT = detail::NoDocumentation>
struct MultiOption : Option<Argument, MandatoryT, simppl::cmdline::detail::MultiSupport, DocumentationT>
{
   template<typename> friend struct detail::HandlerCompositeBase;
   
   inline
   MultiOption()
    : Option<Argument, MandatoryT, simppl::cmdline::detail::MultiSupport, DocumentationT>()
   {
      // NOOP
   }
};


template<typename MandatoryT = Optional, typename MultiSupportT = simppl::cmdline::detail::NoMultiSupport, typename DocumentationT = detail::NoDocumentation>
struct ExtraOption : MandatoryT, MultiSupportT, DocumentationT
{
   template<typename, typename> friend struct detail::HandlerComposite; 
   template<typename> friend struct detail::HandlerCompositeBase;
   template<char, typename, typename, typename, bool> friend struct detail::LongSwitch;

   typedef typename if_<std::is_same<MultiSupportT, simppl::cmdline::detail::MultiSupport>::value, 
      simppl::cmdline::detail::MultiNoChar, 
      typename if_<std::is_same<MandatoryT, Mandatory>::value, simppl::cmdline::detail::MandatoryNoChar, simppl::cmdline::detail::NoChar>::type>::type char_type;
   
   inline
   ExtraOption()
    : MandatoryT()
    , MultiSupportT()
    , DocumentationT()
   {
      // NOOP
   }
      
   inline
   ExtraOption<MandatoryT, MultiSupportT, detail::Documentation>
   doc(const char* doc)
   {
      return ExtraOption<MandatoryT, MultiSupportT, detail::Documentation>(doc);
   }

   // FIXME why this must be public?
   inline explicit
   ExtraOption(const char* doc)
    : MandatoryT()
    , MultiSupportT()
    , DocumentationT(doc)
   {
      // NOOP
   }


protected:
   
   inline
   tribool operator()(simppl::cmdline::detail::ParserState& state)
   {
      if (!MultiSupportT::handled())
      {
         state.argument_ = state.argv_[state.current_];
         return MandatoryT::ok(true);
      }
      else
         return indeterminate;   // just skip it...
   }
      
   inline
   void doDoc(detail::stream_type& os) const
   {
      DocumentationT::doDoc_(os);
   }
   
   inline
   void genCmdline(detail::stream_type& os) const
   {
      MandatoryT::genCmdline(os, MultiSupportT::cmdlineExtension());
   }
   
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK 
   inline 
   bool plausibilityCheck(simppl::cmdline::detail::ParserState& /*state*/) const
   {
      return true;
   }
#endif
};


template<typename MandatoryT = Optional, typename MultiSupportT = detail::NoMultiSupport, typename DocumentationT = detail::NoDocumentation>
struct MultiExtraOption : ExtraOption<MandatoryT, detail::MultiSupport, DocumentationT>
{
   inline
   MultiExtraOption()
    : ExtraOption<MandatoryT, detail::MultiSupport, DocumentationT>()
   {
      // NOOP
   }
};


// forward declaration
struct ExtensionPoint;


namespace detail
{

template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, bool haveArgument>
struct LongSwitch
{
   typedef char_<Argument> char_type;
   
   enum { LongSupport = true, HaveArgument = haveArgument };
      
   inline
   LongSwitch(const char* long__)
    : switch_()
    , long_(long__)
   {
      // NOOP
   }
      
   inline
   LongSwitch(const char* long__, const char* doc)
    : switch_(doc)
    , long_(long__)
   {
      // NOOP
   }
      
   inline
   LongSwitch<Argument, MandatoryT, MultiSupportT, Documentation, haveArgument>
   doc(const char* doc)
   {
      return LongSwitch<Argument, MandatoryT, MultiSupportT, Documentation, haveArgument>(long_, doc);
   }

   inline
   bool operator()(char c, ParserState& state)
   {
      return switch_(c, state);
   }
   
   //inline
   bool operator()(const char* longopt, ParserState& state)
   {
      if (!switch_.isHandled())
      {
         return switch_.MandatoryT::ok(LongOptionChecker<haveArgument>::eval(long_, longopt, state)) && switch_.setHandled();
      }
      else
         return false;
   }

   //inline
   void doDoc(detail::stream_type& os) const
   {
      if (simppl::cmdline::detail::longOptionSupport)
      {
         switch_.template doDoc_<Argument>(os, long_);
      }
      else
         switch_.template doDoc_<Argument>(os);
   }

   //inline
   void genCmdline(detail::stream_type& os) const
   {
      if (simppl::cmdline::detail::longOptionSupport)
      {
         switch_.genCmdline(os, long_);
      }
      else
         switch_.genCmdline(os);
   }
   
   inline
   bool ok() const
   {
      return switch_.ok();
   }
   
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK 
   //inline 
   bool plausibilityCheck(ParserState& state) const
   {
      bool rc = switch_.plausibilityCheck(state) && state.longArguments_.find(long_) == state.longArguments_.end();
      state.longArguments_.insert(long_);
      return rc;
   }
#endif
   
   Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument> switch_;
   const char* long_;
};


// --------------------------------------------------------------------------------


// NOOP handler for const char* and std::string
template<typename T>
struct Converter
{
   static inline
   typename std::remove_reference<T>::type eval(const char* ptr, bool&)
   {
      return ptr;
   }
};


template<>
struct Converter<char*>
{
   static inline
   char* eval(const char* ptr, bool&)
   {
      return const_cast<char*>(ptr);
   }
};


int convert(const char* ptr, bool& success)
{
   int base = 10;
   
   if (*ptr == '0' && *(ptr+1) != '\0')
   {
      base = 8;
   
      if (*(ptr+1) == 'x')
         base = 16;
   }
   
   char* end;
   int rc = strtol(ptr, &end, base);
   success = (*end == '\0');
   
   return rc;
}


template<typename T>
struct IntConverterBase
{
   static inline
   T eval(const char* ptr, bool& success)
   {
      int rc = convert(ptr, success);
      success &= (rc >= std::numeric_limits<T>::min() && rc <= std::numeric_limits<T>::max());
      return rc;
   }
};


template<>
struct Converter<char> : public IntConverterBase<char>
{
};

template<>
struct Converter<short> : public IntConverterBase<short>
{
};

template<>
struct Converter<unsigned short> : public IntConverterBase<unsigned short>
{
};

template<>
struct Converter<int> : public IntConverterBase<int>
{
};

template<>
struct Converter<unsigned int> : public IntConverterBase<unsigned int>
{
};

template<>
struct Converter<signed char> : public IntConverterBase<signed char>
{
};

template<>
struct Converter<unsigned char> : public IntConverterBase<unsigned char>
{
};


// --------------------------------------------------------------------------------


template<typename ObjT>
struct FunctionArgumentTypeDeducer
{
   // must be a bound member function...
   typedef typename ObjT::arg1_type argument_type;
};


template<typename ReturnT, typename ArgumentT>
struct FunctionArgumentTypeDeducer<ReturnT(*)(ArgumentT)>
{
   // ...or an ordinary free function
   typedef ArgumentT argument_type;
};


// -------------------------------------------------------------------------------


// for switches
template<typename ActionReturnT, bool isOption> 
struct ActionCaller
{
   template<typename ActionT>
   static inline 
   bool eval(ActionT& a, ParserState&)
   {
      (void)a();
      return true;
   }
};


// for switches
template<> 
struct ActionCaller<bool, false>
{
   template<typename ActionT>
   static inline 
   bool eval(ActionT& a, ParserState&)
   {
      return a();
   }
};


// for options and extraoptions
template<typename ActionReturnT> 
struct ActionCaller<ActionReturnT, true>
{
   template<typename ActionT>
   static inline 
   bool eval(ActionT& a, ParserState& state)
   {
      typedef typename FunctionArgumentTypeDeducer<ActionT>::argument_type action_argument_type;
      
      bool success = true;
      (void)a(Converter<action_argument_type>::eval(state.argument_, success));
      return success;
   }
};


// for options and extraoptions
template<> 
struct ActionCaller<bool, true>
{
   template<typename ActionT>
   static inline 
   bool eval(ActionT& a, ParserState& state)
   {
      typedef typename FunctionArgumentTypeDeducer<ActionT>::argument_type action_argument_type;
      
      bool success = true;
      return a(Converter<action_argument_type>::eval(state.argument_, success)) && success;
   }
};


// ----------------------------------------------------------------------------------------------


template<typename ObjT>
struct FunctionReturnTypeDeducer
{
   // must be a bound member function...
   typedef typename ObjT::return_type return_type;
};


template<typename ReturnT>
struct FunctionReturnTypeDeducer<ReturnT(*)()>
{
   // ...or an ordinary free function with no argument (the switch case)
   typedef ReturnT return_type;
};


template<typename ReturnT, typename ArgumentT>
struct FunctionReturnTypeDeducer<ReturnT(*)(ArgumentT)>
{
   // ...or an ordinary free function with one argument (the option case)
   typedef ReturnT return_type;
};


// ------------------------------------------------------------------------------------------


template<bool>
struct Incrementor
{
   inline explicit
   Incrementor(int& idx)
    : idx_(++idx)
   {
      // NOOP
   }
   
   inline
   bool rollback()
   {
      --idx_;
      return false;
   }
   
   int& idx_;
};


template<>  
struct Incrementor<false>
{
   inline explicit
   Incrementor(int&)
   {
      // NOOP
   }
   
   inline
   bool rollback()
   {
      return false;
   }
};


// ------------------------------------------------------------------------------------------


template<typename AnchorT>
struct HandlerCompositeBase
{
   static_assert(!std::is_same<typename AnchorT::char_type, char_< simppl::cmdline::NoChar> >::value || AnchorT::LongSupport, "only_long_options_may_support_no_char");
   
private:
   
   struct LongSupportHelper
   {
      static inline 
      bool eval(AnchorT& anchor, const char* longopt, ParserState& state)
      {
         return anchor(longopt, state);
      }
   };
   
   struct NonLongSupportHelper
   {
      static inline 
      bool eval(const AnchorT&, const char*, ParserState& /*state*/)
      {
         return false;
      }
   };
   
public:
   
   explicit inline
   HandlerCompositeBase(AnchorT anchor)
    : anchor_(anchor)
   {
      // NOOP
   }
   
   
   template<typename ActionT>
   inline
   tribool doEval(char theChar, ParserState& state, ActionT& action)
   {
      typedef typename FunctionReturnTypeDeducer<ActionT>::return_type action_return_type;
      typedef Incrementor<AnchorT::HaveArgument> incrementor_type;
      
      if (anchor_(theChar, state))
      {
         incrementor_type inc(state.current_);
         return (OptionChecker<AnchorT::HaveArgument>::eval(state) && ActionCaller<action_return_type, AnchorT::HaveArgument>::template eval(action, state)) || inc.rollback();
      }
      return indeterminate;
   }
   
   
   template<typename ActionT>
   inline
   tribool doEval(const char* longopt, ParserState& state, ActionT& action)
   {
      typedef typename FunctionReturnTypeDeducer<ActionT>::return_type action_return_type;
      
      if (if_<AnchorT::LongSupport, LongSupportHelper, NonLongSupportHelper>::type::eval(anchor_, longopt, state))  
      {
         return ActionCaller<action_return_type, AnchorT::HaveArgument>::template eval(action, state);   
      }
      return indeterminate;
   }
   
   
   template<typename ActionT>
   inline
   tribool doEval(ParserState&, ActionT& action)
   {
      return indeterminate;
   }
   
   AnchorT anchor_;
};


template<typename MandatoryT, typename MultiSupportT, typename DocumentationT>
struct HandlerCompositeBase<ExtraOption<MandatoryT, MultiSupportT, DocumentationT> >
{
   explicit inline 
   HandlerCompositeBase(ExtraOption<MandatoryT, MultiSupportT, DocumentationT> anchor)
    : anchor_(anchor)
   {
      // NOOP
   }
   
   
   template<typename ActionT>
   inline
   tribool doEval(char theChar, ParserState& state, ActionT& action)
   {
      return indeterminate;
   }
   
   
   template<typename ActionT>
   inline
   tribool doEval(const char* longopt, ParserState& state, ActionT& action)
   {
      return indeterminate;
   }
   
   template<typename ActionT>
   inline
   tribool doEval(ParserState& state, ActionT& action)
   {
      if (anchor_(state))
      {
         typedef typename FunctionReturnTypeDeducer<ActionT>::return_type action_return_type;
         return ActionCaller<action_return_type, true>::template eval(action, state);
      }
      else
         return indeterminate;
   }
   
   ExtraOption<MandatoryT, MultiSupportT, DocumentationT> anchor_;
};


template<typename AnchorT, typename ActionT>
struct HandlerComposite : HandlerCompositeBase<AnchorT>
{   
public:
 
   typedef typename AnchorT::char_type arguments_type;
   
   inline
   HandlerComposite(AnchorT anchor, ActionT action)
    : HandlerCompositeBase<AnchorT>(anchor)
    , action_(action)
   {
      // NOOP
   }
   
   inline
   tribool eval(char theChar, ParserState& state)
   {
      return HandlerCompositeBase<AnchorT>::doEval(theChar, state, action_);
   }
   
   inline
   tribool eval(const char* longopt, ParserState& state)
   {
      return HandlerCompositeBase<AnchorT>::doEval(longopt, state, action_);
   }
   
   inline
   tribool eval(ParserState& state)
   {
      return HandlerCompositeBase<AnchorT>::doEval(state, action_);
   }
   
   inline
   void doDoc(detail::stream_type& os) const
   {
      HandlerCompositeBase<AnchorT>::anchor_.doDoc(os);
   }
   
   inline
   void genCmdline(detail::stream_type& os) const
   {
      HandlerCompositeBase<AnchorT>::anchor_.genCmdline(os);
   }
   
   inline
   bool ok() const
   {
      return HandlerCompositeBase<AnchorT>::anchor_.ok();
   }
   
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK 
   inline
   bool plausibilityCheck(ParserState& state) const
   {
      return HandlerCompositeBase<AnchorT>::anchor_.plausibilityCheck(state);
   }
#endif
   
private:
   ActionT action_;
};


// --------------------------------------------------------------------------------


template<typename HandlerT1, typename HandlerT2>
struct ArgumentComposite;

template<typename T1, typename T2>
struct CompositeTypeDiscriminator
{
   static_assert(std::is_same<typename T1::arguments_type, simppl::cmdline::ExtensionPoint>::value || IsExtraOption<typename T1::arguments_type>::value || std::is_same<typename T1::arguments_type, char_<simppl::cmdline::NoChar> >::value || std::is_same<typename T1::arguments_type, typename T2::arguments_type>::value == 0, "no_same_arguments_allowed");
   typedef TypeList<typename T1::arguments_type, TypeList<typename T2::arguments_type, NilType> > arguments_type;
};

template<typename T1, typename T2, typename T3>
struct CompositeTypeDiscriminator<ArgumentComposite<T1, T2>, T3>
{
   static_assert(std::is_same<typename T3::arguments_type, simppl::cmdline::ExtensionPoint>::value || IsExtraOption<typename T3::arguments_type>::value || std::is_same<typename T3::arguments_type, char_<simppl::cmdline::NoChar> >::value || Find<typename T3::arguments_type, typename ArgumentComposite<T1, T2>::arguments_type>::value == -1, "no_same_arguments_allowed");
   typedef TypeList<typename T3::arguments_type, typename ArgumentComposite<T1, T2>::arguments_type> arguments_type;
};


template<typename HandlerT1, typename HandlerT2>
struct ArgumentComposite
{
   typedef typename CompositeTypeDiscriminator<HandlerT1, HandlerT2>::arguments_type arguments_type;
   
   inline
   ArgumentComposite(HandlerT1 first, HandlerT2 second)
    : first_(first)
    , second_(second)
   {
      //    NOOP
   }
   
   inline
   tribool eval(char theChar, ParserState& state)
   {
      tribool rc = first_.eval(theChar, state);
      if (indeterminate(rc))
         rc = second_.eval(theChar, state);
      
      return rc;
   }
   
   inline
   tribool eval(const char* longopt, ParserState& state)
   {
      tribool rc = first_.eval(longopt, state);
      if (indeterminate(rc))
         rc = second_.eval(longopt, state);
      
      return rc;
   }
   
   inline
   tribool eval(ParserState& state)
   {
      tribool rc = first_.eval(state);
      if (indeterminate(rc))
         rc = second_.eval(state);
      
      return rc;
   }
   
   inline
   void doDoc(detail::stream_type& os) const
   {
      first_.doDoc(os);
      second_.doDoc(os);
   }
   
   inline
   void genCmdline(detail::stream_type& os) const
   {
      first_.genCmdline(os);
      second_.genCmdline(os);
   }
   
   // check mandatories
   inline
   bool ok() const
   {
      return first_.ok() && second_.ok();
   }
   
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK 
   inline
   bool plausibilityCheck(ParserState& state) const
   {
      return first_.plausibilityCheck(state) && second_.plausibilityCheck(state);
   }
#endif
   
private:
   HandlerT1 first_;
   HandlerT2 second_;
};


// --------------------------------------------------------------------------------


/**
 * A flagging handler. Takes a bool and flags it 'true' if the option was set.
 */
struct flag
{
   typedef void return_type;
   
   inline explicit
   flag(bool& t)
    : t_(t)
   {
      t_ = false;
   }
   
   inline
   void operator()()
   {
      t_ = true;
   }
   
   bool& t_;
};


/**
 * A generic setter for options.
 */
template<typename T>
struct set
{
   typedef void return_type;
   typedef T arg1_type;
   
   inline explicit
   set(T& t)
    : t_(t)
   {
      // NOOP
   }
   
   inline
   void operator()(const T& t)
   {
      t_ = t;
   }
   
   T& t_;
};


/**
 * A generic setter for options of type char* and const char*.
 */
template<typename T>
struct set<T*>
{
   typedef void return_type;
   typedef T* arg1_type;
   
   inline explicit
   set(T** t)
    : t_(t)
   {
      // NOOP
   }
   
   inline
   void operator()(T* t)
   {
      *t_ = t;
   }
   
   T** t_;
};


/**
 * A generic setter for options of type char* (make strcpy).
 * FIXME ensure the size of the buffer somehow and use strncpy
 */
template<>
struct set<char[]>
{
   typedef void return_type;
   typedef const char* arg1_type;
   
   inline explicit
   set(char* t)
    : t_(t)
   {
      // NOOP
   }
   
   inline
   void operator()(const char* t)
   {
      ::strcpy(t_, t);
   }
   
   char* t_;
};


template<typename T>
struct BackInserter
{
   typedef void return_type;
   typedef typename T::value_type arg1_type;
   
   explicit inline
   BackInserter(T& t)
    : t_(t)
   {
      // NOOP
   }

   inline
   void operator()(typename T::const_reference data)
   {
      t_.push_back(data);
   }
   
   T& t_;
};

}   // namespace detail

}   // namespace cmdline

}   // namespace simppl


template<typename T1, typename T2, typename T3>
inline
simppl::cmdline::detail::ArgumentComposite<T1, simppl::cmdline::detail::HandlerComposite<T2, T3> > 
operator<= (T1 h1, simppl::cmdline::detail::HandlerComposite<T2, T3> h2)
{
   return simppl::cmdline::detail::ArgumentComposite<T1, simppl::cmdline::detail::HandlerComposite<T2, T3> >(h1, h2);
};


// --------------------------------------------------------------------------------


// TODO make sure the handler is not called for any normal variables if no specialized variant is implemented
//      that is, we need some static_assert here.
template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, bool haveArgument, typename ActionT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument>, ActionT>
operator>> (simppl::cmdline::Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument> t1, ActionT t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument>, ActionT>(t1, t2);
}


template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, bool haveArgument, typename ActionT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument>, ActionT>
operator>> (simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument> t1, ActionT t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, haveArgument>, ActionT>(t1, t2);
}


template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, typename ActionT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, ActionT>
operator>> (simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT> t1, ActionT t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, ActionT>(t1, t2);
}


template<typename MandatoryT, typename MultiSupportT, typename DocumentationT, typename ActionT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, ActionT>
operator>> (simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT> t1, ActionT t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, ActionT>(t1, t2);
}


// for bool flagging
template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, false>, simppl::cmdline::detail::flag>
operator>> (simppl::cmdline::Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, false> t1, bool& b)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Switch<Argument, MandatoryT, MultiSupportT, DocumentationT, false>, simppl::cmdline::detail::flag>(t1, simppl::cmdline::detail::flag(b));
}


// for bool flagging
template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, false>, simppl::cmdline::detail::flag>
operator>> (simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, false> t1, bool& b)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, false>, simppl::cmdline::detail::flag>(t1, simppl::cmdline::detail::flag(b));
}


// -------------------------------------------------------------------------


namespace simppl
{

namespace cmdline
{

/**
 * A value incrementor for multi-options like e.g. -vvvvv (=verbosity level)
 */
struct inc
{
   typedef void return_type;
   
   inline explicit
   inc(int& i)
    : i_(i)
   {
      i = 0;
   }
   
   inline
   void operator()()
   {
      ++i_;
   }
   
   int& i_;
};

}   // namespace cmdline

}   // namespace simppl


// --------------------------------------------------------------------------------


#define PARSER_MAKE_SHIFT_OPERATOR(datatype) \
template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT> \
inline \
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<datatype> > \
operator>> (simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT> t1, datatype& t2) \
{ \
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<datatype> >(t1, simppl::cmdline::detail::set<datatype>(t2)); \
} \
\
\
template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT> \
inline \
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>, simppl::cmdline::detail::set<datatype> >  \
operator>> (simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true> t1, datatype& t2) \
{\
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>, simppl::cmdline::detail::set<datatype> >(t1, simppl::cmdline::detail::set<datatype>(t2)); \
} \
\
\
template<typename MandatoryT, typename MultiSupportT, typename DocumentationT> \
inline \
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<datatype> >  \
operator>> (simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT> t1, datatype& t2) \
{\
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<datatype> >(t1, simppl::cmdline::detail::set<datatype>(t2)); \
}

PARSER_MAKE_SHIFT_OPERATOR(char)
PARSER_MAKE_SHIFT_OPERATOR(signed char)
PARSER_MAKE_SHIFT_OPERATOR(unsigned char)

PARSER_MAKE_SHIFT_OPERATOR(short)
PARSER_MAKE_SHIFT_OPERATOR(unsigned short)

PARSER_MAKE_SHIFT_OPERATOR(int)
PARSER_MAKE_SHIFT_OPERATOR(unsigned int)

#ifdef CMDLINE_HAVE_STD_STRING
PARSER_MAKE_SHIFT_OPERATOR(std::string)
#endif


template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, typename T>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<T*> > 
operator>> (simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT> t1, T** t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<T*> >(t1, simppl::cmdline::detail::set<T*>(t2));
}


template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT, typename T>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>, simppl::cmdline::detail::set<T*> > 
operator>> (simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true> t1, T** t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>, simppl::cmdline::detail::set<T*> >(t1, simppl::cmdline::detail::set<T*>(t2));
}


template<typename MandatoryT, typename MultiSupportT, typename DocumentationT, typename T>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<T*> > 
operator>> (simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT> t1, T** t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<T*> >(t1, simppl::cmdline::detail::set<T*>(t2));
}


template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<char[]> > 
operator>> (simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT> t1, char t2[])
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::Option<Argument, MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<char[]> >(t1, simppl::cmdline::detail::set<char[]>(t2));
}


template<char Argument, typename MandatoryT, typename MultiSupportT, typename DocumentationT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>, simppl::cmdline::detail::set<char[]> > 
operator>> (simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true> t1, char t2[])
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::detail::LongSwitch<Argument, MandatoryT, MultiSupportT, DocumentationT, true>, simppl::cmdline::detail::set<char[]> >(t1, simppl::cmdline::detail::set<char[]>(t2));
}


template<typename MandatoryT, typename MultiSupportT, typename DocumentationT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<char[]> > 
operator>> (simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT> t1, char t2[])
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, MultiSupportT, DocumentationT>, simppl::cmdline::detail::set<char[]> >(t1, simppl::cmdline::detail::set<char[]>(t2));
}


#ifdef CMDLINE_HAVE_STD_VECTOR
template<typename MandatoryT, typename DocumentationT, typename DataT>
inline
simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, simppl::cmdline::detail::MultiSupport, DocumentationT>, simppl::cmdline::detail::BackInserter<std::vector<DataT> > > 
operator>> (simppl::cmdline::ExtraOption<MandatoryT, simppl::cmdline::detail::MultiSupport, DocumentationT> t1, std::vector<DataT>& t2)
{
   return simppl::cmdline::detail::HandlerComposite<simppl::cmdline::ExtraOption<MandatoryT, simppl::cmdline::detail::MultiSupport, DocumentationT>, 
      simppl::cmdline::detail::BackInserter<std::vector<DataT> > >(t1, simppl::cmdline::detail::BackInserter<std::vector<DataT> >(t2));
}
#endif


#endif   // SIMPPL_CMDLINE_H
