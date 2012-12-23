#ifndef SIMPPL_TRIBOOL_H
#define SIMPPL_TRIBOOL_H


namespace simppl
{

struct tribool;
typedef bool (*tIndeterminateFunc)(tribool);


struct tribool
{
private:
   
   struct safe_bool_struct
   {
      void safe() { /*NOOP*/ }
   };

public:
   
   typedef void(safe_bool_struct::*safe_bool)();
   
   enum State { False = -1, Indeterminate = 0, True = 1 };
   
   inline
   tribool()
    : value_(Indeterminate)
   {
      // NOOP
   }

   inline
   tribool(const tribool& rvalue)
    : value_(rvalue.value_)
   {
      // NOOP
   }
   
   inline
   tribool(bool b)
    : value_(b ? True : False)
   {
      // NOOP
   }

   inline
   tribool(tIndeterminateFunc /*value*/)
    : value_(Indeterminate)
   {
      // NOOP
   }
   
   inline
   tribool& operator=(tribool rvalue)
   {
      if (&rvalue != this)
      {
         value_ = rvalue.value_;
      }
      return *this;
   }
   
   inline
   tribool& operator=(tIndeterminateFunc /*value*/)
   {
      value_ = Indeterminate;
      return *this;
   }
   
   inline
   tribool& operator=(bool b)
   {
      value_ = b ? True : False;
      return *this;
   }
   
   inline
   operator safe_bool() const
   {
      return value_ == True ? &safe_bool_struct::safe : 0;
   }
   
   inline
   safe_bool operator!() const
   {
      return value_ == False ? &safe_bool_struct::safe : 0;
   }
   
   State value_;
};


#ifndef INDETERMINATE_STATE
#   define INDETERMINATE_STATE indeterminate
#endif

inline
bool INDETERMINATE_STATE(tribool b) 
{
   return b.value_ == tribool::Indeterminate;
}

}   // namespace simppl


#endif   // SIMPPL_TRIBOOL_H
