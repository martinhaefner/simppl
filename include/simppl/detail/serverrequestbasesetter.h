#ifndef SIMPPL_DETAIL_SERVERREQUESTBASESETTER_H
#define SIMPPL_DETAIL_SERVERREQUESTBASESETTER_H


namespace simppl
{
   
namespace ipc
{
   
namespace detail
{
   

struct ServerRequestBaseSetter
{
   template<typename T>
   static inline
   void setHasResponse(T& t)
   {
      t.hasResponse_ = true;
   }
};


}   // namespace detail

}   // namespace ipc

}   // namespace simppl


#endif   // SIMPPL_DETAIL_SERVERREQUESTBASESETTER_H
