#ifndef SIMPPL_IF_H
#define SIMPPL_IF_H

namespace simppl
{

template<bool condition, typename ThenT, typename ElseT> 
struct if_
{
    typedef ThenT type;
};

template<typename ThenT, typename ElseT> 
struct if_<false, ThenT, ElseT>
{
    typedef ElseT type;
};

}   // namespace simppl


#endif  // SIMPPL_IF_H

