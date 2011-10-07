#include <iostream>


// FIXME rethink alignment issues
template<int Size>
struct pool
{
   struct entry
   {
      void* next_;
   };
   
   pool(int count)
    : p_((entry*)new char[count * Size])
   {
      for(int i=0; i<count; ++i)
      {
         // FIXME implement this
      }
   }
   
   void resize(int count)
   {
      // FIXME implement this
   }
   
   bool is_element_of(void* ptr)
   {
      // FIXME implement this
      return false;
   }
   
   void* get()
   {
      //void* cur = free_;
      //free_ = *free_;
      //return cur;
      return free_;
   }
   
   void release(void* ptr)
   {
   }
   
   entry* free_;
   entry* p_;
};


// -------------------------------------------------


struct base
{
   virtual ~base() {}
   virtual void hello()
   {
      std::cout << "base" << std::endl;
   }
};

struct derived : base
{
   void hello()
   {
      std::cout << "derived" << std::endl;
   }
   
   int i_;
   int j_;
};

struct another : base
{
   void hello()
   {
      std::cout << "another" << std::endl;
   }
};


int main()
{
   pool<sizeof(derived)> p_(5);
   void* ptr = p_.get();
   new(ptr) derived();
   base* b = (base*)ptr;
   b->hello();
   return 0;
};
