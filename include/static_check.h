#ifndef STATIC_CHECK_H
#define STATIC_CHECK_H


template<bool> 
struct CheckOK
{
    typedef bool type;
};

template<> 
struct CheckOK<false>;  // not implemented


#define STATIC_CHECK(condition, message) \
   typedef typename CheckOK<condition>::type message;
   
// template<typename T> struct message; 
// typedef CheckOK<condition>::type message;

   //int array[condition?1:-1];
#if 0
    struct message : public CheckOK<condition> \
    { \
    }; \
    //(void)sizeof(message);
#endif

#endif   // STATIC_CHECK_H
