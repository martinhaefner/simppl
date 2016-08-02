#ifndef SIMPPL_DETAIL_INIFILE_H
#define SIMPPL_DETAIL_INIFILE_H

#include <limits>
#include <type_traits>

#include "simppl/tribool.h"

namespace simppl
{

namespace inifile
{

// forward decl
struct Section;
struct File;

/// No header line in ini-file, just a marker interface for type switching
struct NoopHeaderParser;


// ----------------------------------------------------------------------------


namespace detail
{

struct IniFileSectionReturn
{
   template<typename T>
   static inline simppl::inifile::Section& eval(T& t)
   {
      return t.section_;
   }
};


struct SubCompositeSectionReturn
{
   template<typename T>
   static inline simppl::inifile::Section& eval(T& t)
   {
      return t.included_.defaultSection();
   }
};


#ifndef INIFILE_HAVE_STL_STREAMS

struct InputFileStream /* : noncopyable */
{
   explicit inline
   InputFileStream(const char* filename)
    : file_(fopen(filename, "r"))
    , last_(0)
   {
      // NOOP
   }

   inline
   ~InputFileStream()
   {
      if (file_)
         (void)fclose(file_);
   }

   inline
   bool is_open() const
   {
      return file_ != 0;
   }

   inline
   bool open(const char* filename)
   {
      if (is_open())
      {
         fclose(file_);
         file_ = 0;
      }

      file_ = fopen(filename, "r");
      last_ = 0;

      return is_open();
   }

   inline
   bool bad()
   {
      assert(file_);
      return file_ == 0 || (ferror(file_) && !feof(file_));
   }

   inline
   size_t gcount() const
   {
      return last_;
   }

   inline
   operator const void*() const
   {
      return file_ && !feof(file_) && !ferror(file_) ? this : 0;
   }

   InputFileStream& getline(char* buffer, size_t buflen)
   {
      if (file_)
      {
         if (fgets(buffer, buflen, file_) != 0)
         {
            size_t len = strlen(buffer);

            if (len == buflen - 1)
            {
               if (buffer[len-1] != '\n')
                  last_ = buflen + 1;     // indicate line-to-long
            }
            else if (len > 0)
            {
               last_ = len;

               // remove tailing linefeeds
               if (buffer[len-1] == '\n')
                  buffer[--len] = '\0';

               if (len > 0 && buffer[len-1] == '\r')
                  buffer[--len] = '\0';
            }
            else
               last_ = 0;
         }
         else
            last_ = 0;
      }

      return *this;
   }

private:

   // noncopyable
   InputFileStream(const InputFileStream&);
   InputFileStream& operator=(const InputFileStream&);

   FILE* file_;
   size_t last_;   // for gcount()
};

#endif   // INIFILE_HAVE_STL_STREAMS

}   // namespace detail


#ifdef INIFILE_HAVE_STL_STREAMS
typedef std::ifstream stream_type;
#else
typedef detail::InputFileStream stream_type;
#endif


namespace detail
{


// NOOP handler for const char* and std::string
template<typename T>
struct Converter
{
   static inline
   typename std::remove_reference<T>::type
   eval(const char* ptr, bool&)
   {
      return ptr;
   }
};


template<>
struct Converter<bool>
{
   static inline
   bool eval(const char* ptr, bool& ret)
   {
      (void)ret;
#ifndef NDEBUG
      ret = !strcasecmp(ptr, "true") || !strcasecmp(ptr, "false");
#endif
      return !strcasecmp(ptr, "true");
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


// TODO maybe we could use int for all smaller datatypes, but that's an optimization only
static
long long convert(const char* ptr, bool& success)
{
   int base = 10;

   if (*ptr == '0' && *(ptr+1) != '\0')
   {
      base = 8;

      if (*(ptr+1) == 'x')
         base = 16;
   }

   char* end;
   long long rc = strtoll(ptr, &end, base);
   success = (*end == '\0');

   return rc;
}


template<typename T>
struct IntConverterBase
{
   static inline
   T eval(const char* ptr, bool& success)
   {
      long long rc = convert(ptr, success);
      success &= (rc >= (long long)std::numeric_limits<T>::min() && rc <= (long long)std::numeric_limits<T>::max());
      return rc;
   }
};


#define INIFILE_CONVERTER(type) \
   template<> struct Converter<type> : public IntConverterBase<type> { };

INIFILE_CONVERTER(short)
INIFILE_CONVERTER(unsigned short)

INIFILE_CONVERTER(int)
INIFILE_CONVERTER(unsigned int)

INIFILE_CONVERTER(char)
INIFILE_CONVERTER(signed char)
INIFILE_CONVERTER(unsigned char)


// -------------------------------------------------------------------------------


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


template<typename ActionReturnT>
struct ActionCaller
{
   template<typename ActionT>
   static inline
   bool eval(ActionT& a, const char* value)
   {
      typedef typename FunctionArgumentTypeDeducer<ActionT>::argument_type action_argument_type;

      bool success = true;
      (void)a(Converter<action_argument_type>::eval(value, success));
      return success;
   }
};


template<>
struct ActionCaller<bool>
{
   template<typename ActionT>
   static inline
   bool eval(ActionT& a, const char* value)
   {
      typedef typename FunctionArgumentTypeDeducer<ActionT>::argument_type action_argument_type;

      bool success = true;
      return a(Converter<action_argument_type>::eval(value, success)) && success;
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


// ---------------------------------------------------------------------------------------------


/// a key-value parser with the following syntax evaluation:
///
///    key=value                  -> normal syntax
///    key="value"                -> quotes will be removed
///    key = value                -> skip spaces in both key and value
///    key = " value "            -> keep spaces in value
///    key=value # comment        -> comment will be skipped
///    key="value # with comment" -> comment is part of the value
///    key="\"quoted value\""     -> quotes are part of value
///
struct KeyValueParser
{
private:

   void strmove(char* dest, const char* src)
   {
      while(*src)
      {
         *dest++ = *src++;
      }

      *dest = '\0';
   }

   typedef void(KeyValueParser::*State)();

   // the key part
   void key__()
   {
      if (::isalnum(*cur_) || *cur_ == '_' || *cur_ == '-' || *cur_ == '.')
      {
         if (!beginkey_)
            beginkey_ = cur_;

         ++cur_;
         endkey_ = cur_;
      }
      else
         state_ = next();
   }

   // white space
   void ws()
   {
      if (::isblank(*cur_))
      {
         ++cur_;
      }
      else
         state_ = next();
   }

   // equality operator
   void equal()
   {
      state_ = (*cur_ == '=' ? next() : &KeyValueParser::fail);
      ++cur_;
   }

   // start of quotation of a value
   void beginquote()
   {
      if (*cur_ == '"')
      {
         quoted_ = true;
         ++cur_;
      }

      state_ = next();

   }

   // end of quotation of a value
   void endquote()
   {
      state_ = ((quoted_ && *cur_ == '"') || (!quoted_ && *cur_ != '"') ? next() : &KeyValueParser::fail);

      if (quoted_)
         ++cur_;
   }

   inline
   void storeAndAdvanceValuePtrs()
   {
      if (!beginvalue_)
         beginvalue_ = cur_;

      ++cur_;
      endvalue_ = cur_;
   }

   // the value part
   void value__()
   {
      if (!quoted_)
      {
         if (::isgraph(*cur_))
         {
            storeAndAdvanceValuePtrs();
         }
         else
            state_ = next();
      }
      else
      {
         if (*cur_ == '"')
         {
            if (*(cur_-1) == '\\')
            {
               strmove(cur_-1, cur_);
            }
            else
               state_ = next();
         }
         else
         {
            if (::isgraph(*cur_) || ::isblank(*cur_))
            {
               storeAndAdvanceValuePtrs();
            }
            else
               state_ = next();
         }
      }
   }

   // end state
   void success()
   {
      // NOOP
   }

   // end state
   void fail()
   {
      // NOOP
   }

   // real parser chain end state -> must be terminating 0, otherwise the parser ran into error somehow
   void finish()
   {
      state_ = (*cur_ == '\0' || *cur_ == '#' ? &KeyValueParser::success : &KeyValueParser::fail);
   }

   // return next state in parser chain (the grammar)
   inline
   State next()
   {
      // this is the grammar - how the tokens have to follow each other
      static State states[] = {
         &KeyValueParser::ws,
         &KeyValueParser::key__,
         &KeyValueParser::ws,
         &KeyValueParser::equal,
         &KeyValueParser::ws,
         &KeyValueParser::beginquote,
         &KeyValueParser::value__,
         &KeyValueParser::endquote,
         &KeyValueParser::ws,
         &KeyValueParser::finish
      };

      return states[++idx_];
   }

public:

   explicit inline
   KeyValueParser(char* line)
    : state_(0)
    , cur_(line)
    , idx_(-1)
    , quoted_(false)
    , beginkey_(0)
    , endkey_(0)
    , beginvalue_(0)
    , endvalue_(0)
   {
      // NOOP
   }

   // call this once on a parser object
   KeyValueParser& parse()
   {
      assert(idx_ == -1);

      state_ = next();

      while(state_ != &KeyValueParser::fail && state_ != &KeyValueParser::success)
      {
         (this->*state_)();
      }

      return *this;
   }

   // evaluate if parse was successful
   inline
   bool good() const
   {
      return state_ == &KeyValueParser::success;
   }

   // return the section name, only call this after good() returned 'true'.
   inline
   const char* key() const
   {
      assert(good());

      *endkey_ = '\0';   // terminate the key
      return beginkey_;
   }

   inline
   const char* value() const
   {
      assert(good());

      *endvalue_ = '\0';   // terminate the value
      return beginvalue_;
   }

   State state_;
   char* cur_;
   int idx_;
   bool quoted_;

   char* beginkey_;
   volatile char* endkey_;

   char* beginvalue_;
   volatile char* endvalue_;
};


static
char* skipComments(char* line)
{
   // skip comments in line (also skip blanks before the comment)
   char* ptr = line;
   while(*ptr && *ptr != '#')
      ++ptr;

   *ptr = '\0';
   while(::isblank(*--ptr))
      *ptr = '\0';

   return line;
}


template<typename MandatoryT, typename HandlerT>
struct KeyHandlerComposite
{
   inline
   KeyHandlerComposite(const Key<MandatoryT>& key, const HandlerT& handler)
    : key_(key)
    , handler_(handler)
   {
      // NOOP
   }

   tribool operator()(const char* key, const char* value)
   {
      if (key_(key))
      {
         typedef typename FunctionReturnTypeDeducer<HandlerT>::return_type action_return_type;
         return ActionCaller<action_return_type>::template eval(handler_, value);
      }
      else
         return indeterminate;
   }

   inline
   bool ok(section_key_pair_type& p) const
   {
      p.second = key_.key_;
      return key_.ok();
   }

   Key<MandatoryT> key_;
   HandlerT handler_;
};


// --------------------------------------------------------------------------------------------


template<typename ContainerT>
struct BackInserterContainerHandler
{
   typedef bool return_type;
   typedef const char* arg1_type;

   inline
   BackInserterContainerHandler(ContainerT& container)
    : container_(container)
   {
      // NOOP
   }

   bool operator()(const char* value)
   {
      char* v = const_cast<char*>(value);
      bool rc = true;

      char* token = strtok(v, ",");
      while(token && rc)
      {
         container_.push_back(Converter<typename ContainerT::value_type>::eval(token, rc));
         token = strtok(0, ",");
      }

      return rc;
   }

   ContainerT& container_;
};


// -------------------------------------------------------------------------------------------


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
 * A generic setter for options of type char* (make strcpy).
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


// ---------------------------------------------------------------------------------------


struct ParserChainSectionBase : simppl::inifile::Section
{
   explicit inline
   ParserChainSectionBase(const simppl::inifile::Section& section)
    : simppl::inifile::Section(section)
   {
      // NOOP
   }

   inline
   ParserChainSectionBase()
    : simppl::inifile::Section()
   {
      // NOOP
   }

   tribool eval(char* line)
   {
      KeyValueParser p(line);
      if (p.parse().good())
      {
         return doEval(p.key(), p.value());
      }

      return false;
   }


protected:

   virtual tribool doEval(const char* key, const char* value) = 0;
};


template<typename ParserT>
struct ParserChainSection : ParserChainSectionBase
{
   /// named section
   inline
   ParserChainSection(const simppl::inifile::Section& section, ParserT parser)
    : ParserChainSectionBase(section)
    , parser_(parser)
   {
      // NOOP
   }

   /// unnamed default section
   explicit inline
   ParserChainSection(ParserT parser)
    : ParserChainSectionBase()
    , parser_(parser)
   {
      // NOOP
   }

   tribool doEval(const char* key, const char* value)
   {
      return parser_(key, value);
   }

   inline
   bool ok(section_key_pair_type& p) const
   {
      p.first = name();
      return parser_.ok(p);
   }

   ParserT parser_;
};


struct NoopDefaultSection : simppl::inifile::Section
{
   explicit inline
   NoopDefaultSection()
    : simppl::inifile::Section()
   {
      // NOOP
   }
};


template<typename HandlerT>
struct FullHandledSection : simppl::inifile::Section
{
   inline
   FullHandledSection(const simppl::inifile::Section& section, HandlerT handler)
    : simppl::inifile::Section(section)
    , handler_(handler)
   {
      // NOOP
   }

   tribool eval(char* line)
   {
      typedef typename FunctionReturnTypeDeducer<HandlerT>::return_type action_return_type;
      return ActionCaller<action_return_type>::template eval(handler_, skipComments(line));
   }

   HandlerT handler_;
};


// ---------------------------------------------------------------------------------------


/// template based section-composite
template<typename SectionT, typename IncludedSectionT>
struct SectionatedIniFile
{
   SectionatedIniFile(const SectionT& section, const IncludedSectionT& included)
    : section_(section)
    , included_(included)
   {
      assert(included_.find(section_.name()) == 0);
   }

   Section* find(const char* name)
   {
      if (!::strcmp(section_.name(), name))
      {
         return &section_;
      }
      else
         return included_.find(name);
   }

   inline
   Section& defaultSection()
   {
      typedef typename std::conditional<std::is_same<IncludedSectionT, File>::value, detail::IniFileSectionReturn, detail::SubCompositeSectionReturn>::type caller_type;
      return caller_type::eval(*this);
   }

   inline
   const char* filename() const
   {
      return included_.filename();
   }

   inline
   const char* defaultFilename() const
   {
      return included_.defaultFilename();
   }

   inline
   bool ok(detail::section_key_pair_type& p) const
   {
      return section_.ok(p) && included_.ok(p);
   }

   SectionT section_;
   IncludedSectionT included_;
};


// ---------------------------------------------------------------------------------------


/// if you ever get this assertion beeing raised during runtime you should check your
/// section grammars for duplicate keys
template<typename CompositeT>
struct DuplicateKeyEncountered
{
   static inline
   bool eval(const CompositeT& c, const char* key)
   {
      return c.contains(key);
   }
};


template<typename MandatoryT, typename HandlerT>
struct DuplicateKeyEncountered<KeyHandlerComposite<MandatoryT, HandlerT> >
{
   template<typename CompositeT>
   static inline
   bool eval(const CompositeT& c, const char* key)
   {
      return !::strcmp(c.key_.key_, key);
   }
};


// ---------------------------------------------------------------------------------------


template<typename MandatoryT1, typename HandlerT1, typename CompositeT>
struct HandlerComposite
{
   inline
   HandlerComposite(const KeyHandlerComposite<MandatoryT1, HandlerT1>& lhs, const CompositeT& rhs)
    : lhs_(lhs)
    , rhs_(rhs)
   {
      assert(!DuplicateKeyEncountered<CompositeT>::eval(rhs, lhs_.key_.key_));
   }

   inline
   bool contains(const char* key) const
   {
      return !::strcmp(lhs_.key_.key_, key) || DuplicateKeyEncountered<CompositeT>::eval(rhs_, key);
   }

   tribool operator()(const char* key, const char* value)
   {
      tribool rc = lhs_(key, value);
      if (rc || !rc)
         return rc;

      return rhs_(key, value);
   }

   inline
   bool ok(section_key_pair_type& p) const
   {
      return lhs_.ok(p) && rhs_.ok(p);
   }

   KeyHandlerComposite<MandatoryT1, HandlerT1> lhs_;
   CompositeT rhs_;
};


// ----------------------------------------------------------------------------


struct SectionNameParser
{
private:

   typedef void(SectionNameParser::*State)();

   // left sectionname start bracket
   void start()
   {
      state_ = (*cur_ == '[' ? next() : &SectionNameParser::fail);
      ++cur_;
   }

   // white space
   void ws()
   {
      if (::isblank(*cur_))
      {
         ++cur_;
      }
      else
         state_ = next();
   }

   // the section name part
   void name__()
   {
      if (::isalnum(*cur_) || *cur_ == '_' || *cur_ == '-' || *cur_ == '.')
      {
         if (!start_)
            start_ = cur_;

         ++cur_;
         end_ = cur_;
      }
      else
         state_ = next();
   }

   // right sectionname stop bracket
   void end()
   {
      state_ = (*cur_ == ']' ? next() : &SectionNameParser::fail);
      ++cur_;
   }

   // end state
   void success()
   {
      // NOOP
   }

   // end state
   void fail()
   {
      // NOOP
   }

   // real parser chain end state -> must be terminating 0, otherwise the parser ran into error somehow
   // or it may stop at a comment
   void finish()
   {
      state_ = (*cur_ == '\0' || *cur_ == '#' ? &SectionNameParser::success : &SectionNameParser::fail);
   }

   // return next state in parser chain (the grammar)
   inline
   State next()
   {
      // this is the grammar - how the tokens have to follow each other
      static State states[] = {
         &SectionNameParser::start,
         &SectionNameParser::ws,
         &SectionNameParser::name__,
         &SectionNameParser::ws,
         &SectionNameParser::end,
         &SectionNameParser::ws,
         &SectionNameParser::finish
      };

      return states[++idx_];
   }

public:

   explicit inline
   SectionNameParser(char* line)
    : state_(0)
    , cur_(line)
    , idx_(-1)
    , start_(0)
    , end_(0)
   {
      // NOOP
   }

   // call this once on a parser object
   SectionNameParser& parse()
   {
      assert(idx_ == -1);

      state_ = next();

      while(state_ != &SectionNameParser::fail && state_ != &SectionNameParser::success)
      {
         (this->*state_)();
      }

      return *this;
   }

   // evaluate if parse was successful
   inline
   bool good() const
   {
      return state_ == &SectionNameParser::success;
   }

   // return the section name, only call this after good() returned 'true'.
   inline
   const char* name() const
   {
      assert(good());

      *end_ = '\0';   // terminate the section name
      return start_;
   }

   State state_;
   char* cur_;
   int idx_;

   const char* start_;
   mutable char* end_;
};


template<typename ParserT>
struct HeaderlineEvaluator
{
   static
   bool eval(stream_type& is, int& line)
   {
      char buf[128];
      is.getline(buf, sizeof(buf));
      ++line;

      if (is.gcount() > 0 && is.gcount() < sizeof(buf) - 1)
      {
         return ParserT::eval(buf);
      }
      else
         return false;
   }
};


template<>
struct HeaderlineEvaluator<NoopHeaderParser>
{
   static inline
   bool eval(stream_type&, int&)
   {
      return true;
   }
};


}   // namespace detail

}   // namespace inifile

}   // namespace simppl


#endif   // SIMPPL_DETAIL_INIFILE_H
