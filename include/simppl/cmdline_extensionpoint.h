#ifndef SIMPPL_CMDLINE_EXTENSIONPOINT_H
#define SIMPPL_CMDLINE_EXTENSIONPOINT_H


#include "simppl/cmdline.h"
#include "simppl/intrusive_list.h"


namespace cmdline
{

namespace detail
{
   
struct ExtensionPointHolder : intrusive::ListBaseHook
{
   virtual ~ExtensionPointHolder()
   {
      // NOOP
   }
   
   virtual tribool eval(char theChar, detail::ParserState& state) = 0;
   
   virtual tribool eval(const char* longopt, detail::ParserState& state) = 0;
   
   virtual tribool eval(detail::ParserState& state) = 0;
   
   virtual void doDoc(stream_type& os) const = 0;
   
   virtual void genCmdline(stream_type& os) const = 0;
   
   // check mandatories
   virtual bool ok() const = 0;

   // be aware of object size in different compilation modes -> leave virtual function inside here
   virtual bool plausibilityCheck(ParserState& state) const = 0;
};


template<typename ParserT>
struct ExtensionPointHolderImpl : ExtensionPointHolder
{
   explicit inline
   ExtensionPointHolderImpl(const ParserT& parser)
    : parser_(parser)
   {
      typedef typename Reverse<typename detail::MakeTypeList<typename ParserT::arguments_type>::type>::type arguments_type;      
      static_assert(detail::FindExtra<arguments_type>::value < 0, "no_extra_options_allowed_in_parser_extension");
   }
  
   tribool eval(char theChar, detail::ParserState& state)
   {
      return parser_.eval(theChar, state);
   }
   
   tribool eval(const char* longopt, detail::ParserState& state)
   {
      return parser_.eval(longopt, state);
   }
   
   tribool eval(detail::ParserState& state)
   {
      return parser_.eval(state);
   }
   
   void doDoc(stream_type& os) const
   {
      return parser_.doDoc(os);
   }
   
   void genCmdline(stream_type& os) const
   {
      return parser_.genCmdline(os);
   }
   
   bool ok() const 
   {
      return parser_.ok();
   }
   
   bool plausibilityCheck(ParserState& state) const
   {
#ifdef CMDLINE_ENABLE_RUNTIME_CHECK   
      return parser_.plausibilityCheck(state);
#else
      return true;
#endif
   }

private:
   ParserT parser_;
};

}   // namespace detail


// Extension point for adding further commandline options to the parser grammar. 
// Note, that these cannot be validated by the compiler, so using the runtime check is adequate.
struct ExtensionPoint
{
private:
   
   // FIXME globally check all public and private typedefs and member functions for accessability
   typedef intrusive::List<detail::ExtensionPointHolder> holder_type;  
   
public:
   
   typedef ExtensionPoint char_type;
   typedef char_type arguments_type;
    
   inline
   ExtensionPoint()
   {
      // NOOP
   }
   
   template<typename ParserT>
   inline
   ExtensionPoint(const ParserT& parser)
   {
      holder_.push_back(*new detail::ExtensionPointHolderImpl<ParserT>(parser));
   }
   
   // move semantics once the parser runs - so the extensionpoint cannot be accessed later on.
   ExtensionPoint(const ExtensionPoint& rhs)
    : holder_(rhs.holder_)   /// FIXME??? why this and swap afterwards?
   {
      holder_.swap(rhs.holder_);
   }
   
   ~ExtensionPoint()
   {
      holder_.clear();
   }
   
   template<typename ParserT>
   void operator+=(const ParserT& parser)
   {
      holder_.push_back(*new detail::ExtensionPointHolderImpl<ParserT>(parser));
   }

// FIXME from here on private

   tribool eval(char theChar, detail::ParserState& state)
   {
      tribool rc = indeterminate;
      
      for(holder_type::iterator iter = holder_.begin(); iter != holder_.end() && indeterminate(rc); ++iter)
      {
         rc = iter->eval(theChar, state);
      }
      
      return rc;
   }
   
   tribool eval(const char* longopt, detail::ParserState& state)
   {
      tribool rc = indeterminate;
      
      for(holder_type::iterator iter = holder_.begin(); iter != holder_.end() && indeterminate(rc); ++iter)
      {
         rc = iter->eval(longopt, state);
      }
      
      return rc;
   }
   
   tribool eval(detail::ParserState& state)
   {
      tribool rc = indeterminate;
      
      for(holder_type::iterator iter = holder_.begin(); iter != holder_.end() && indeterminate(rc); ++iter)
      {
         rc = iter->eval(state);
      }
      
      return rc;
   }
   
   void doDoc(detail::stream_type& os) const
   {
      std::for_each(holder_.begin(), holder_.end(), std::bind(&detail::ExtensionPointHolder::doDoc, std::placeholders::_1, std::ref(os)));
   }
   
   void genCmdline(detail::stream_type& os) const
   {
      std::for_each(holder_.begin(), holder_.end(), std::bind(&detail::ExtensionPointHolder::genCmdline, std::placeholders::_1, std::ref(os)));
   }
   
   bool ok() const 
   {
      bool rc = true;
      
      for(holder_type::const_iterator iter = holder_.begin(); iter != holder_.end() && rc == true; ++iter)
      {
         rc = iter->ok();
      }
      
      return rc;
   }

#ifdef CMDLINE_ENABLE_RUNTIME_CHECK   
   bool plausibilityCheck(detail::ParserState& state) const
   {
      bool rc = true;
      
      for(holder_type::const_iterator iter = holder_.begin(); iter != holder_.end() && rc == true; ++iter)
      {
         rc = iter->plausibilityCheck(state);
      }
      
      return rc;
   }
#endif 
    
   mutable holder_type holder_;   // move semantics
};

}   // end namespace cmdline


// ---------------------------------------------------------------------------------------------------


template<typename T1, typename T2>
inline
cmdline::detail::ArgumentComposite<cmdline::detail::ArgumentComposite<T1, T2>, cmdline::ExtensionPoint> 
operator<= (cmdline::detail::ArgumentComposite<T1, T2> h1, cmdline::ExtensionPoint h2)
{
   return cmdline::detail::ArgumentComposite<cmdline::detail::ArgumentComposite<T1, T2>, cmdline::ExtensionPoint>(h1, h2);
};


inline
cmdline::detail::ArgumentComposite<cmdline::ExtensionPoint, cmdline::ExtensionPoint> 
operator<= (cmdline::ExtensionPoint h1, cmdline::ExtensionPoint h2)
{
   return cmdline::detail::ArgumentComposite<cmdline::ExtensionPoint, cmdline::ExtensionPoint>(h1, h2);
};


#endif   // SIMPPL_CMDLINE_EXTENSIONPOINT_H
