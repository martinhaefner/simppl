#ifndef SIMPPL_NONINSTANTIABLE_H
#define SIMPPL_NONINSTANTIABLE_H


namespace simppl
{

struct NonInstantiable
{
protected:
   NonInstantiable() = delete;
   ~NonInstantiable() = delete;
};

}   // namespace simppl


#endif   // SIMPPL_NONINSTANTIABLE_H
