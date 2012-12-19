#ifndef SIMPPL_INIFILE_EXTENSIONPOINT_H
#define SIMPPL_INIFILE_EXTENSIONPOINT_H

#include "intrusive_list.h"


namespace ini
{

namespace detail
{
   
struct ExtensionPointHolder : intrusive::ListBaseHook
{
   virtual ~ExtensionPointHolder() { /* NOOP */ }
   
   virtual Section* find(const char* name) = 0;
   
   virtual bool ok(detail::section_key_pair_type& p) const = 0;
};


// FIXME holder must always hold a composite -> makes it much easier
template<typename GrammarT>
struct ExtensionPointHolderImpl : ExtensionPointHolder
{
   ExtensionPointHolderImpl(const GrammarT& grammar)
    : grammar_(grammar)
   {
      // NOOP
   }
   
   Section* find(const char* name)
   {
      return 0;   // FIXME must do something senseful...
   }
   
   bool ok(detail::section_key_pair_type& p) const
   {
      return true;   // FIXME do something senseful
   }
   
   GrammarT grammar_;
};

}   // namespace detail


struct ExtensionPoint
{
   typedef intrusive::List<detail::ExtensionPointHolder> holder_type;  
   
   inline
   ExtensionPoint()
   {
      // NOOP
   }
   
   template<typename GrammarT>
   inline
   ExtensionPoint(const GrammarT& grammar)
   {
      holder_.push_back(*new detail::ExtensionPointHolderImpl<GrammarT>(grammar));
   }
   
   // move semantics once the parser runs - so the extensionpoint cannot be accessed later on.
   ExtensionPoint(const ExtensionPoint& rhs)
    : holder_(rhs.holder_)   // FIXME ??? why this and swap afterwards?
   {
      holder_.swap(rhs.holder_);
   }
   
   ~ExtensionPoint()
   {
      holder_.clear();
   }
   
   
   template<typename GrammarT>
   void operator+=(const GrammarT& grammar)
   {
      holder_.push_back(*new detail::ExtensionPointHolderImpl<GrammarT>(grammar));
   }
   
   Section* find(const char* name)
   {
      Section* rc = 0;
      
      for(holder_type::iterator iter = holder_.begin(); iter != holder_.end() && indeterminate(rc); ++iter)
      {
         if ((rc = iter->find(name)))
            break;
      }
      
      return rc;
   }
   
   inline
   bool ok(detail::section_key_pair_type& p) const
   {
      bool rc = true;
      
      for(holder_type::iterator iter = holder_.begin(); iter != holder_.end() && indeterminate(rc); ++iter)
      {
         if (!(rc = iter->ok(p)))
            break;
      }
      
      return rc;
   }
   
private:
   
   mutable holder_type holder_;   // FIXME mutable necessary because of 'swap'
};


namespace detail
{
   
/// extension point specialization
template<typename IncludedSectionT>
struct SectionatedIniFile<ExtensionPoint, IncludedSectionT> 
{
   SectionatedIniFile(const ExtensionPoint& ep, const IncludedSectionT& included)
    : ep_(ep)
    , included_(included)
   {
      //assert(included_.find(section_.name()) == 0);
      // FIXME iterate over sections of extensionpoint and check for occurance of name in included
   }
   
   Section* find(const char* name)
   {
      Section* rc = ep_.find(name);
      return rc ? rc : included_.find(name);
   }
   
   inline
   Section& defaultSection()
   {
      return included_.defaultSection();
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
      return ep_.ok(p) && included_.ok(p);
   }
   
   ExtensionPoint ep_;
   IncludedSectionT included_;
};


}   // end namespace detail

}   // end namespace ini


#endif   // SIMPPL_INIFILE_EXTENSIONPOINT_H
