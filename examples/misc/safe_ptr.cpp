#include <cassert>
#include <iostream>


static const unsigned int FLAG_PREPARE_DELETE  = 0x08000000;


#ifdef __QNX__
#   include <atomic.h>

#   define __sync_fetch_and_sub(where, count) (atomic_sub_value(where, count))
#   define __sync_sub_and_fetch(where, count) (atomic_sub_value(where, count) - count)
#   define __sync_fetch_and_add(where, count) (atomic_add_value(where, count))
#   define set_to_zero(where) (atomic_clr_value((volatile unsigned*)where, 0xFFFFFFFF))
#   define __sync_fetch_and_or(where, bits) (atomic_set_value(where, bits))
#   define __sync_synchronize() /*noop for now*/
#else
#   define set_to_zero(where) (__sync_fetch_and_and(&t_, 0))
#endif


template<typename T>
struct SafePtrAccessor;

template<typename T>
struct SafePtr
{
   friend struct SafePtrAccessor<T>;
   
private:
   
   void release()
   {      
      int rc = __sync_fetch_and_sub(&h_->accessState_, 1);
      {                   
         if (rc & FLAG_PREPARE_DELETE)   
            h_->try_destruct();              
      }
   }
   
   bool acquire(int)
   {      
      // FIXME maybe check for 0 pointer?!
      int rc = __sync_fetch_and_add(&h_->accessState_, 1);

      if (rc & FLAG_PREPARE_DELETE)
      {       
         (void)__sync_fetch_and_sub(&h_->accessState_, 1);
         h_->try_destruct();
         
         return false;
      }
      
      return true;
   }

   
public:

   SafePtr(T* t)
    : h_(new Holder(t))
   {
      // NOOP
   }
   
   ~SafePtr()
   {
      if (h_->release() == 0)
         delete h_;
   }
   
   SafePtr(const SafePtr& rhs)
    : h_(0)
   {
      if (rhs.h_)
      {
         SafePtr<T>* that = const_cast<SafePtr*>(&rhs);
            
         that->h_->acquire();
         h_ = that->h_;
      }
   }   
   
   SafePtr& operator=(const SafePtr& rhs)    
   {
      if (&rhs != this && rhs.h_ != h_)
      {      
         if (h_->release() == 0)
            delete h_;
       
         SafePtr<T>* that = const_cast<SafePtr*>(&rhs);       
         h_ = that->h_;
         
         if (h_)
            h_->acquire();
      }   

      return *this;
   }
   
   SafePtrAccessor<T> acquire();
   
   
   void destruct()
   {
      if (h_)      
         h_->prepare_destruct();      
   }
   
   void dump()
   {
      if (h_)
         std::cout << "share: " << h_->refcount_ << ", access: " << std::hex << h_->accessState_ << std::dec << std::endl;
   }
   
   
private:

   struct Holder
   {
      Holder(T* t)
       : t_(t)
       , refcount_(1)
       , accessState_(0)         
      {
         __sync_synchronize();   // make refcount_ visible to others
      }
      
      ~Holder()
      {
         delete t_;
      }
      
      void acquire()
      {
         (void)__sync_fetch_and_add(&refcount_, 1);
      }
      
      int release()
      {
         return __sync_sub_and_fetch(&refcount_, 1);
      }
      
      void prepare_destruct()
      {                      
        std::cerr << "prepare" << std::endl;      
         (void)__sync_fetch_and_or(&accessState_, FLAG_PREPARE_DELETE);         
      }
      
      void try_destruct()
      {         
       if (t_ != 0)
       {
          std::cerr << "try" << std::endl;        
            volatile T* t;
         
            t = (T*)set_to_zero(&t_);
            if (t != 0)
            {         
               delete t;            
            }
         }
      }
      
      volatile T* t_;     
      
      volatile unsigned int refcount_;      
      volatile unsigned int accessState_;    
   };
   
   Holder* h_;   
};


template<typename T>
struct SafePtrAccessor
{
   friend class SafePtr<T>;
   
private:

   SafePtrAccessor(const SafePtrAccessor&) = delete;
   SafePtrAccessor& operator=(const SafePtrAccessor&) = delete;

   SafePtrAccessor(SafePtr<T>& s)
    : s_(0)
   {
      if (s.acquire(42))
         s_ = &s;
   }
   
public:

   SafePtrAccessor()
    : s_(0)
   {
      // NOOP
   }
   
   SafePtrAccessor(SafePtrAccessor&& rhs)
    : s_(0)
   {
      s_ = rhs.s_;
      rhs.s_ = 0;
   }
   
   SafePtrAccessor& operator=(SafePtrAccessor&& rhs)
   {
      s_ = rhs.s_;
      rhs.s_ = 0;
      
      return *this;
   }
   
   ~SafePtrAccessor()
   {
      if (s_)
         s_->release();
   }
      
   operator bool()
   {
      return s_;
   }
   
   T* operator->() 
   {
      assert(s_);
      return (T*)s_->h_->t_;
   }
   
   T& operator*()
   {
      assert(s_);
      return *(T*)s_->h_->t_;
   }
   
   
private:   
   
   SafePtr<T>* s_;
};


template<typename T>
SafePtrAccessor<T> SafePtr<T>::acquire()
{
   SafePtrAccessor<T> acc(*this);
   return std::move(acc);
}
   
   
// ------------------------------------------------------------------------------------------


#include <iostream>
#include <pthread.h>


struct S
{
   S(int i)
    : i_(i)
   {
      // NOOP
   }
   
   ~S()
   {
      std::cout << "~~~" << std::endl;
   }
   
   int i_;
};


void* runner(void* p)
{
   // FIXME making the copy in a separate thread is not really safe
   // but since we join that thread we can be sure that the object
   // is not destructed in-between
   SafePtr<S> s1 = *(SafePtr<S>*)p;
   int j=0;
   
   for(int i=0; i<1000; ++i)
   {
      usleep(1);
      sched_yield();
      
      {
         auto p = s1.acquire();
         if (p)
         {
         //s1.dump();
            j += p->i_;
         }
         else
         {
            std::cout << "He's dead at " << i << std::endl;
            break;
         }
      }
      
      if (i==999)
         std::cout << "Got to the end" << std::endl;
   }
   
   usleep(300);   
   s1.destruct();   
   
   return 0;
}


int main()
{  
   SafePtr<S> ip(new S(42));   
   
   const int num_threads = 30;
   
   pthread_t tids[num_threads];
   for (int i=0; i<num_threads; ++i)
   {
      pthread_create(&tids[i], 0, runner, &ip);
   }
   
   for (int i=0; i<num_threads; ++i)
   {
      pthread_join(tids[i], 0);
   }
   
   return 0;
}
