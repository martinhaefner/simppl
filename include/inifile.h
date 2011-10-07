#ifndef INIFILE_H
#define INIFILE_H

#include <cassert>
#include <ctype.h>
#include <cstring>
#include <cstdlib>

#define INIFILE_HAVE_STL_STREAMS 1
#define INIFILE_HAVE_STD_VECTOR 1

#ifdef INIFILE_HAVE_RVALUE_REFS
#   define INIFILE_RVALUE_REF &&
#else
#   define INIFILE_RVALUE_REF 
#endif

#ifdef INIFILE_HAVE_STL_STREAMS
#   include <iostream>
#   include <fstream>
#else
#   include <stdio.h>
#endif

#ifdef INIFILE_HAVE_STD_VECTOR
#   include <vector>
#endif

#include "if.h"
#include "tribool.h"
#include "static_check.h"
#include "noninstantiable.h"

#define INIFILE_FRIEND_PARSER template<typename,typename,typename,typename,bool> friend struct Parser;


namespace ini
{

// forward decls
template<typename MandatoryT>
struct Key;

namespace detail
{

/// 'first' marks the section
/// 'second' the key
typedef std::pair<const char*, const char*> section_key_pair_type;

template<typename>
struct ParserChainSection;

template<typename, typename>
struct KeyHandlerComposite;

template<typename>
struct DuplicateKeyEncountered;

template<typename, typename, typename>
struct HandlerComposite;

}   // namespace detail

// end forward decls


/**
 * Argument policy. An argument using this policy is optional and must not be 
 * given at the commandline.
 */
struct Optional
{
   template<typename,typename>
   friend struct detail::KeyHandlerComposite;
   
protected:
   
   inline
   Optional()
   {
      // NOOP
   }
   
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
};


/**
 * Argument Policy. The using argument must be given at the commandline, otherwise the parser
 * reports an error.
 */
struct Mandatory
{   
   template<typename, typename>
   friend struct detail::KeyHandlerComposite;
   
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
   
   bool ok_;
};


/// key of key-value pair
template<typename MandatoryT = Optional>
struct Key : MandatoryT
{
   template<typename, typename>
   friend struct detail::KeyHandlerComposite;
   
   template<typename>
   friend struct detail::DuplicateKeyEncountered;
   
   template<typename, typename, typename>
   friend struct detail::HandlerComposite;
   
   explicit
   Key(const char* INIFILE_RVALUE_REF key)
    : key_(key)
   {
      assert(key);
      assert(strlen(key) > 0);
   }
 
private:
   
   inline
   bool operator()(const char* key)
   {
      return MandatoryT::ok(!::strcmp(key_, key));
   }

   const char* key_;
};


enum Error 
{
   Error_OK = 0,                  ///< no error occurred during processing 
   
   Error_FILE_NOT_FOUND,
   
   Error_INVALID_FILE_HEADER,
   Error_INVALID_SECTION_HEADER,
   Error_INVALID_KEYVALUE_PAIR,   ///< general parse error due to wrong grammar
   
   Error_UNKNOWN_SECTION,
   Error_UNKNOWN_KEY,
   
   Error_MISSING_MANDATORY,       ///< missing mandatory key-value pair
   Error_ANY,                     ///< any other unknown error
   
   Error_MAX                      ///< no error, just for static check purposes
};


// -----------------------------------------------------------------------------


/// result object of a parse operation 
struct Result
{
   const char* toString() const
   {
      static const char* msg[] = {
         "ok",
         "file not found",
         "invalid header",
         "invalid section",
         "invalid key-value pair",
         "unknown section",
         "unknown key",
         "missing mandatory key",
         "unknown error"
      };
      STATIC_CHECK(Error_MAX == sizeof(msg)/sizeof(msg[0]), enum_and_string_array_do_not_match);
      
      return msg[errno_];
   }
   
   inline
   Result(Error err, int line = -1)
    : errno_(err)
    , line_no_(line)
   {
      // NOOP
   }
   
   /// boolean comparison operator
   inline
   operator const void*() const
   {
      return errno_ == Error_OK ? this : 0;
   }
   
   inline
   Error error() const
   {
      return errno_;
   }
   
   inline 
   int line() const
   {
      return line_no_;
   }
   
private:
   
   Error errno_;                   ///< type of error that occurred
   int line_no_;                   ///< where did the error occur
};


// -----------------------------------------------------------------------------


/// inifile section as in [sectionname]
struct Section
{
   friend struct File;
   
   INIFILE_FRIEND_PARSER
      
   explicit
   Section(const char* INIFILE_RVALUE_REF name)
    : name_(name)
   {
      assert(name);
   }
   
   Section()
    : name_("[default]")
   {
      // NOOP
   }
   
   template<typename ParserT>
   inline
   detail::ParserChainSection<ParserT> operator[](ParserT parser);
   
   virtual tribool eval(char* line)
   {
      return indeterminate;
   }
   
   inline
   bool ok(detail::section_key_pair_type& p) const 
   {
      return true;
   }

   inline
   const char* name() const
   {
      return name_;
   }

private:
   
   const char* name_;
};


// ----------------------------------------------------------------------------


}   // namespace ini

#include "detail/inifile.h"

namespace ini
{

   
template<typename ParserT>
inline
ini::detail::ParserChainSection<ParserT> Section::operator[](ParserT parser)
{
   return ini::detail::ParserChainSection<ParserT>(*this, parser);
}


// ----------------------------------------------------------------------------


/// Ini-File with key-value pairs grouped into sections
struct File
{
   friend struct ReportMandatoryKeys;
   friend struct CheckMandatoryKeys;
   INIFILE_FRIEND_PARSER
   
   /// Both arguments are supposed to survive the lifetime of the object, the second one should be
   /// a compiled-in value
   explicit inline
   File(const char* /*user specific*/fname, const char* INIFILE_RVALUE_REF /*system wide*/defaultname = 0)
    : fname_(fname)
    , defaultname_(defaultname)
   {
      assert(fname);
   }
   
   /// return section name -> default: no name
   inline
   const char* name() const
   {
      return 0;
   }
   
   inline
   Section* find(const char*) const
   {
      return 0;
   }
   
   inline
   const char* filename() const
   {
      return fname_;
   }

   inline
   const char* defaultFilename() const
   {
      return defaultname_;
   }
      
   template<typename ParserT>
   inline
   detail::SectionatedIniFile<detail::ParserChainSection<ParserT>, File> operator[](const ParserT& parser)
   {
      return detail::SectionatedIniFile<detail::ParserChainSection<ParserT>, File>(
         detail::ParserChainSection<ParserT>(parser), *this);
   }
   
   // only for easier stopping of the algorithm -> better would be to never call this from the parser composite
   inline
   bool ok(detail::section_key_pair_type& p) const
   {
      return true;
   }
   
private:
   
   const char* fname_;
   const char* defaultname_;
};


// ----------------------------------------------------------------------------


class IgnoreUnknownSection : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   static inline
   bool eval(const char* sectionName)
   {
      return true;
   }
};


class ReportUnknownSection : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   static inline
   bool eval(const char* sectionName)
   {
#ifdef INIFILE_HAVE_STL_STREAMS
      std::cout << "Skipping unknown section '" << sectionName << "'." << std::endl;
#else
      fprintf(stdout, "Skipping unknown section '%s'.\n", sectionName);
#endif
      return true;
   }
};


class FailOnUnknownSection : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   static inline
   bool eval(const char* sectionName)
   {
#ifdef INIFILE_HAVE_STL_STREAMS
      std::cerr << "Unknown section '" << sectionName << "' detected." << std::endl;
#else
      fprintf(stderr, "Unknown section '%s' detected.\n", sectionName);
#endif
      return false;
   }
};


// ----------------------------------------------------------------------------


class IgnoreUnknownKey : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   static inline
   bool eval(const char* line)
   {
      return true;
   }
};


class ReportUnknownKey : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   static inline
   bool eval(const char* line)
   {
#ifdef INIFILE_HAVE_STL_STREAMS
      std::cout << "Skipping unknown key '" << line << "'." << std::endl;
#else
      fprintf(stdout, "Skipping unknown key '%s'.\n", line);
#endif
      return true;
   }
};


class FailOnUnknownKey : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   static inline
   bool eval(const char* line)
   {
#ifdef INIFILE_HAVE_STL_STREAMS
      std::cerr << "Unknown key-value pair '" << line << "' detected." << std::endl;
#else
      fprintf(stderr, "Unknown key-value pair '%s' detected.\n", line);
#endif
      return false;
   }
};


// ---------------------------------------------------------------------------------------------


class IgnoreMandatoryKeys : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   template<typename FileGrammarT>
   static inline
   Result eval(const FileGrammarT& /*inifile*/)
   {
      return Error_OK;
   }
};


class ReportMandatoryKeys : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   template<typename FileGrammarT>
   static inline
   Result eval(const FileGrammarT& inifile)
   {
      detail::section_key_pair_type p;
      
      if (!inifile.ok(p))
      {
#ifdef INIFILE_HAVE_STL_STREAMS
         std::cerr << "Missing mandatory key '" << p.second << "' in section '" << p.first << "'." << std::endl;
#else
         fprintf(stderr, "Missing mandatory key '%s' in section '%s'.\n", p.second, p.first);
#endif
         return Error_MISSING_MANDATORY;
      }
      else
         return Error_OK;
   }
};


class CheckMandatoryKeys : NonInstantiable
{
   INIFILE_FRIEND_PARSER
   
   template<typename FileGrammarT>
   static inline
   Result eval(const FileGrammarT& inifile)
   {
      detail::section_key_pair_type p;
      return inifile.ok(p) ? Error_OK : Error_MISSING_MANDATORY;
   }
};


// ---------------------------------------------------------------------------


template<typename HeaderParserT = NoopHeaderParser, 
         typename UnknownSectionT = IgnoreUnknownSection, 
         typename UnknownKeyT = IgnoreUnknownKey, 
         typename CheckMandatoryT = ReportMandatoryKeys,
         bool /*some ini-file must be there*/Mandatory = false>
struct Parser : NonInstantiable
{
   template<typename FileGrammarT>
   static 
   Result parse(const FileGrammarT& inifile__)
   {
      FileGrammarT& inifile = const_cast<FileGrammarT&>(inifile__);
      int line = 0;
      
      stream_type ifs(inifile.filename());
      
      if (!ifs.is_open())
         ifs.open(inifile.defaultFilename());
      
      if (ifs.is_open())
      {
         if (!detail::HeaderlineEvaluator<HeaderParserT>::eval(ifs, line))
            return Error_INVALID_FILE_HEADER;
         
         char buf[2048];
         Section* current = &inifile.defaultSection();
      
         bool skipUntilNextSection = false;
         
         while(ifs)
         {
            ifs.getline(buf, sizeof(buf));
            ++line;
            
            if (ifs.gcount() > 0 && ifs.gcount() < (int)sizeof(buf) - 1)
            {
               char* ptr = buf;
               
               // skip blanks
               while(*ptr && ::isblank(*ptr))
                  ++ptr;
               
               // skip comment lines
               if (*ptr == '#')
         
                  continue;
               
               // skip empty lines 
               if (*ptr == 0)
                  continue;
               
               // section start
               if (*ptr == '[')
               {
                  skipUntilNextSection = false;
            
                  detail::SectionNameParser p(ptr);
                  if (p.parse().good())
                  {
                     // section exists in parser grammar?
                     Section* found = inifile.find(p.name());
                  
                     if (found)
                     {
                        current = found;
                        continue;
                     }
                     else
                     {
                        if (!UnknownSectionT::eval(p.name()))
                           return Result(Error_UNKNOWN_SECTION, line);
              
                        skipUntilNextSection = true;
                        continue;
                     }
                  }
                  
                  return Result(Error_INVALID_SECTION_HEADER, line);
               }
               else
               {
                  if (!skipUntilNextSection)
                  {
                     // must be a key/value pair or line based section entry
                     tribool ret = current->eval(ptr);
                     
                     if (!ret)
                     {  
                        return Result(Error_INVALID_KEYVALUE_PAIR, line);
                     }
                     else if (indeterminate(ret) && !UnknownKeyT::eval(ptr))
                        return Result(Error_UNKNOWN_KEY, line);
                  }
               }
            }
            else
               break;
         }
         
         if (ifs.bad())
         {
            return Error_ANY;
         }
         else
            return CheckMandatoryT::eval(inifile);
      } 

      return Mandatory ? Error_FILE_NOT_FOUND : Error_OK;
   }
};

}   // namespace ini


// ------------------------------------------------------------------------------------------


#ifdef INIFILE_HAVE_STD_VECTOR

template<typename MandatoryT, typename T>
inline
ini::detail::KeyHandlerComposite<MandatoryT, ini::detail::BackInserterContainerHandler<std::vector<T> > >
operator>>(const ini::Key<MandatoryT>& key, std::vector<T>& v)
{
   return ini::detail::KeyHandlerComposite<MandatoryT, ini::detail::BackInserterContainerHandler<std::vector<T> > >(
      key, ini::detail::BackInserterContainerHandler<std::vector<T> >(v));
}


template<typename T>
inline
ini::detail::FullHandledSection<ini::detail::BackInserterContainerHandler<std::vector<T> > > 
operator>>(const ini::Section& section, std::vector<T>& v)
{
   return ini::detail::FullHandledSection<ini::detail::BackInserterContainerHandler<std::vector<T> > >(
      section, ini::detail::BackInserterContainerHandler<std::vector<T> >(v));
}

#endif   // INIFILE_HAVE_STD_VECTOR


template<typename MandatoryT, typename HandlerT>
inline
ini::detail::KeyHandlerComposite<MandatoryT, HandlerT>
operator>>(const ini::Key<MandatoryT>& key, HandlerT handler)
{
   return ini::detail::KeyHandlerComposite<MandatoryT, HandlerT>(key, handler);
}


#define INIFILE_DECLARE_STREAM_OPERATOR(type) \
template<typename MandatoryT> \
inline \
ini::detail::KeyHandlerComposite<MandatoryT, ini::detail::set<type> > \
operator>>(const ini::Key<MandatoryT>& key, type& b) \
{ \
   return ini::detail::KeyHandlerComposite<MandatoryT, ini::detail::set<type> >(key, ini::detail::set<type>(b)); \
} \

INIFILE_DECLARE_STREAM_OPERATOR(bool)

INIFILE_DECLARE_STREAM_OPERATOR(char)
INIFILE_DECLARE_STREAM_OPERATOR(signed char)
INIFILE_DECLARE_STREAM_OPERATOR(unsigned char)

INIFILE_DECLARE_STREAM_OPERATOR(signed short)
INIFILE_DECLARE_STREAM_OPERATOR(unsigned short)

INIFILE_DECLARE_STREAM_OPERATOR(signed int)
INIFILE_DECLARE_STREAM_OPERATOR(unsigned int)

INIFILE_DECLARE_STREAM_OPERATOR(std::string)


template<typename MandatoryT> 
inline 
ini::detail::KeyHandlerComposite<MandatoryT, ini::detail::set<char[]> > 
operator>>(const ini::Key<MandatoryT>& key, char b[]) 
{ 
   return ini::detail::KeyHandlerComposite<MandatoryT, ini::detail::set<char[]> >(key, ini::detail::set<char[]>(b)); 
} 


template<typename CompositeT, typename MandatoryT1, typename HandlerT1>
inline
ini::detail::HandlerComposite<MandatoryT1, HandlerT1, CompositeT>
operator||(const CompositeT& lhs, const ini::detail::KeyHandlerComposite<MandatoryT1, HandlerT1>& rhs)
{
   // assert for duplicate keys
   return ini::detail::HandlerComposite<MandatoryT1, HandlerT1, CompositeT>(rhs, lhs);
}


// ------------------------------------------------------------------------------------------


/// function objects handler
template<typename HandlerT>
inline
ini::detail::FullHandledSection<HandlerT> operator>>(const ini::Section& section, const HandlerT& handler)
{
   return ini::detail::FullHandledSection<HandlerT>(section, handler);
}


/// normal functions handler
template<typename ReturnT, typename ArgumentT>
inline
ini::detail::FullHandledSection<ReturnT(*)(ArgumentT)> operator>>(const ini::Section& section, ReturnT(*handler)(ArgumentT))
{
   return ini::detail::FullHandledSection<ReturnT(*)(ArgumentT)>(section, handler);
}


// ------------------------------------------------------------------------------------------


template<typename T1, typename T2, typename RightT>
inline
ini::detail::SectionatedIniFile<RightT, ini::detail::SectionatedIniFile<T1, T2> > 
operator<=(const ini::detail::SectionatedIniFile<T1, T2>& left, const RightT& right)
{
   return ini::detail::SectionatedIniFile<RightT, ini::detail::SectionatedIniFile<T1, T2> >(right, left);
}


template<typename RightT>
inline
ini::detail::SectionatedIniFile<RightT, ini::detail::SectionatedIniFile<ini::detail::NoopDefaultSection, ini::File> > 
operator<=(const ini::File& f, const RightT& right)
{
   return ini::detail::SectionatedIniFile<RightT, ini::detail::SectionatedIniFile<ini::detail::NoopDefaultSection, ini::File> >(
      right, ini::detail::SectionatedIniFile<ini::detail::NoopDefaultSection, ini::File>(ini::detail::NoopDefaultSection(), f));
}


#endif   // INIFILE_H
