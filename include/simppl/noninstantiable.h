#ifndef SIMPPL_NONINSTANTIABLE_H
#define SIMPPL_NONINSTANTIABLE_H


struct NonInstantiable
{
protected:
   NonInstantiable() = delete;
   ~NonInstantiable() = delete;
};


#endif   // SIMPPL_NONINSTANTIABLE_H
