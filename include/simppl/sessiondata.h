#ifndef SIMPPL_SESSIONDATA_H
#define SIMPPL_SESSIONDATA_H


/// session data holder
struct SessionData
{
   SessionData();
   SessionData(int fd, void* data, void(*destructor)(void*));
   
   ~SessionData();
   
   SessionData(SessionData&& rhs);
   SessionData& operator=(SessionData&& rhs);
   
   SessionData(const SessionData&) = delete;
   SessionData& operator=(const SessionData&) = delete;
   
   int fd_;                     ///< associated file handle the session belongs to
   void* data_;                 ///< the session data pointer
   void(*destructor_)(void*);   ///< destructor function to use for destruction
};


#endif   // SIMPPL_SESSIONDATA_H
