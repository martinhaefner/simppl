#include <iostream>
#include <stdlib.h>
#include <assert.h>


struct storage_chunk
{
   friend struct memory_pool;
   
  
   storage_chunk(size_t size, size_t count)
    : free_(count)
    , capacity_(size * count)
    , freelist_(this + 1)
    , next_(0)
#ifdef NDEBUG
    , elemsize_(size)
    , elemcount_(count)
#endif
   {
      char* ptr = (char*)(this + 1);
     
      for(int i=0; i<count-1; ++i)
      {
	 *(intptr_t*)ptr = 
	    reinterpret_cast<intptr_t>(ptr + size);
	 ptr += size;
      }
      
      *ptr = 0;   // last entry
   }
   
   
   inline
   ~storage_chunk()
   {
#ifdef NDEBUG
      if (free_ != elemcount_)
      {
	 std::cerr << "Memory leak detected in storage_chunk " << this << std::endl;
	 abort();
      }
#endif

      free_ = 0;
      capacity_ = 0;
      freelist_ = 0;
      next_ = 0;
   }
   
   
   void* allocate()
   {
      void* rc = 0;
      
      if (freelist_)
      {
	 rc = freelist_;
	 freelist_ = (void*)*reinterpret_cast<intptr_t*>(freelist_);
	 --free_; 
      }
      
      return rc;
   }
   
   
   bool free(void* ptr)
   {
      bool rc = false;
      
      if (ptr >= this + 1 && ptr < (char*)(this + 1) + capacity_)
      {
	 // do a freelist check for detecting double frees when NDEBUG is set
#ifdef NDEBUG
	 void* freeptr = freelist_;
	 while(freeptr)
	 {
	    if (ptr == freeptr)
	    {
	       std::cerr << "Double free detected in storage_chunk " << this << " for " << ptr << std::endl;
	       abort();
	    }
	    
	    freeptr = (void*)*reinterpret_cast<intptr_t*>(freeptr); 
	 }
#endif   // NDEBUG 
	 
	 *(intptr_t*)ptr = 
	    reinterpret_cast<intptr_t>(freelist_);
	 freelist_ = ptr;
	 
	 ++free_;
	 
	 rc = true;
      }

      return rc;
   }
   
   
   inline
   size_t available() const
   {
      return free_;
   }
   
   
private:
   
   size_t free_;
   size_t capacity_;   ///< in bytes, not element count, only good for pointer check
   
   void* freelist_; 
   storage_chunk* next_;  

#ifdef NDEBUG
   size_t elemsize_;
   size_t elemcount_;
#endif
   
   // make sure to keep 8 byte alignment here...
};


/**
 * Simple light weight fixed-size allocation container.
 */
struct memory_pool
{
   enum {
      Initial = 100,
      Increment = 100
   };
  
   memory_pool(size_t size, size_t elements = Initial, size_t increment = Increment)
    : chunks_(0)
    , size_(size < sizeof(intptr_t) ? sizeof(intptr_t) : size)
    , increment_(increment)
   {
      assert(size);
      assert(elements);
      
      chunks_ = createNewChunk(elements);
   }
   
   
   ~memory_pool()
   {
      storage_chunk* chunk = chunks_;
      while(chunk != 0)
      {
         chunk = deleteChunk(chunk);
      }
   }


   inline
   void* allocate()
   {
      void* rc = quick_allocate();
      if (rc == 0)
	 rc = full_allocate();
      return rc;
   }
   
   
   inline
   void free(void* ptr)
   {
      if (!quick_free(ptr))
	 full_free(ptr);
   }
   
   
   /**
    * Free all unused memory chunks and sort them in a certain order (e.g. by most free 
    * chunks for fast access when allocation occurres).
    */
   void gc()
   {
      // FIXME
   }
   
   
   /**
    * Number of elements which can currently be allocated without any OS interaction.
    */
   size_t available() const
   {
      size_t rc = 0;
      for(storage_chunk* chunk = chunks_; chunk != 0; chunk = chunk->next_)
      {
	 rc += chunk->available();
      }
      return rc;
   }
   
   
private:

   inline
   void* quick_allocate()
   {
      return chunks_->allocate();
   }
   
   
   void* full_allocate()
   {
      // As precondition, the first chunk is already viewed for new memory and obviously returned 0.
      // Therefore we start with the second chunk.
      void* rc = 0;
      
      storage_chunk* chunk = chunks_->next_;
      while(chunk != 0)
      {
	 if ((rc = chunk->allocate()) != 0)
	   break;
	 
	 chunk = chunk->next_;
      }
      
      if (rc == 0 && increment_ > 0)
      {
	 storage_chunk* newchunk = createNewChunk(increment_);
	 newchunk->next_ = chunks_;
	 chunks_ = newchunk;
	 
	 rc = newchunk->allocate();
      }
      
      return rc;
   }
   
  
   inline
   bool quick_free(void* ptr)
   {
      return chunks_->free(ptr);
   }
  
  
   void full_free(void* ptr)
   {
      if (ptr)
      {
	 // As precondition, the first chunk is already viewed for new memory and obviously returned false.
         // Therefore we start with the second chunk.
	 storage_chunk* chunk = chunks_->next_;
         bool found = false;
	 
	 while(chunk != 0 && !found)
         {
	    if (chunk->free(ptr))
	       found = true;
	 
	    chunk = chunk->next_;
         }
         
#ifdef NDEBUG
         if (!found)
	 {
	    std::cerr << "Dangling pointer " << this << " encountered by segregated_storage " << this << std::endl;
	    abort();
	 }
#endif
      }      
   }
   
   
   storage_chunk* deleteChunk(storage_chunk* chunk)
   {
      storage_chunk* rc = chunk->next_;
      
      chunk->~storage_chunk();
      ::free(chunk);
      
      return rc;
   }
  
  
   storage_chunk* createNewChunk(size_t count)
   {
      storage_chunk* rc = (storage_chunk*)::malloc(sizeof(storage_chunk) + size_ * count);
      new(rc) storage_chunk(size_, count);
      return rc;
   }
   
   storage_chunk* chunks_;
   
   size_t size_;   ///< element size
   size_t increment_;   ///< increment size for new chunks
};


template<typename T> 
struct object_pool
{
    object_pool(size_t initial = memory_pool::Initial, size_t increment = memory_pool::Increment)
     : store_(sizeof(T), initial, increment)
    {
       // NOOP
    }
    
    inline
    T* allocate()
    {
       T* t = (T*)store_.allocate();
       new(t) T();
       return t;
    }
    
    template<typename T1>
    inline
    T* allocate(T1 t1)
    {
       T* t = (T*)store_.allocate();
       new(t) T(t1);
       return t;
    }
    
    template<typename T1, typename T2>
    inline
    T* allocate(T1 t1, T2 t2)
    {
       T* t = (T*)store_.allocate();
       new(t) T(t1, t2);
       return t;
    }
    
    template<typename T1, typename T2, typename T3>
    inline
    T* allocate(T1 t1, T2 t2, T3 t3)
    {
       T* t = (T*)store_.allocate();
       new(t) T(t1, t2, t3);
       return t;
    }
    
    inline
    void free(T* t)
    {
       assert(t);
       t->~T();
       store_.free(t);
    }
    
    memory_pool store_;
};


// ------------------------------------------------------------------------------------------


struct nix
{
   nix()
    : i(0)
   {
      // NOOP
   }
   
   nix(int argi)
    : i(argi)
   {
      // NOOP
   }
   
   int i;
   double d;
};


int main()
{
   memory_pool store(sizeof(nix), 4, 4);
   
   //nix* n1 = new(store.allocate()) nix;
   void* p1 = store.allocate();
   void* p2 = store.allocate();
   void* p3 = store.allocate();
   void* p4 = store.allocate();
   void* p5 = store.allocate();
   std::cout << store.available() << std::endl;
   
   std::cout << p1 << " " << p2 << " " << p3 << " " << p4 << " " << p5 << std::endl;
   
   store.free(p1);
   store.free(p2);
   store.free(p3);
   p3 = store.allocate();
   store.free(p4);
   store.free(p5);
   std::cout << store.available() << std::endl;
   
   p5 = store.allocate();
   store.free(p3);
   store.free(p5);
   std::cout << store.available() << std::endl;
   
   object_pool<nix> alloc;
   nix* n1 = alloc.allocate();
   nix* n2 = alloc.allocate(45);
   alloc.free(n1);
   alloc.free(n2);
   
   return 0;
}
