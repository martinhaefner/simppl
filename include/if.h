#ifndef IF_H
#define IF_H


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



#endif  // IF_H

