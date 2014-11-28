#ifndef SIMPPL_INTRUSIVE_LIST_H
#define SIMPPL_INTRUSIVE_LIST_H


#include <algorithm>
#include <cassert>


namespace simppl
{

namespace intrusive
{

template<typename DataT>
struct IteratorTraits
{
  typedef DataT& referenceType;
  typedef DataT* pointerType;
};

template<typename DataT>
struct IteratorTraits<const DataT>
{
 typedef const DataT& referenceType;
  typedef const DataT* pointerType;
};


struct ListBaseHook
{
  void* next_;
  void* prev_;
};

struct ListMemberHook
{
  void* next_;
  void* prev_;
};


template<typename DataT>
struct ListBaseHookAccessor
{
  DataT* next(const void* d) const
  {
      return (DataT*)(((DataT*)d)->next_);
  }

  DataT* prev(const void* d) const
  {
      return (DataT*)(((DataT*)d)->prev_);
  }

  void setNext(void* d, void* next)
  {
      ((DataT*)d)->next_ = next;
  }

  void setPrev(void* d, void* prev)
  {
      ((DataT*)d)->prev_ = prev;
  }
};


template<typename DataT, struct ListMemberHook DataT::* MemberPtr>
struct ListMemberHookAccessor
{
  DataT* next(const void* d) const
  {
      return (DataT*)((((DataT*)d)->*MemberPtr).next_);
  }

  DataT* prev(const void* d) const
  {
      return (DataT*)(((DataT*)d->*MemberPtr).prev_);
  }

  void setNext(void* d, void* next)
  {
      (((DataT*)d)->*MemberPtr).next_ = next;
  }

  void setPrev(void* d, void* prev)
  {
      ((DataT*)d->*MemberPtr).prev_ = prev;
  }
};


struct SizeHoldingPolicy
{
  SizeHoldingPolicy()
      : size_(0)
  {
      // NOOP
  }

  void reset()
  {
      size_ = 0;
  }

  void add()
  {
      ++size_;
  }

  void remove()
  {
      --size_;
  }

  template<typename IteratorT>
  size_t internal_size(IteratorT /*begin*/, IteratorT /*end*/) const
  {
      return size_;
  }

private:

  size_t size_;
};


struct CalculatingPolicy
{
  void reset()
  {
      // NOOP
  }

  void add()
  {
      // NOOP
  }

  void remove()
  {
      // NOOP
  }

  template<typename IteratorT>
  size_t internal_size(IteratorT begin, IteratorT end) const
  {
      size_t ret = 0;
      std::for_each(begin, end, [&ret](IteratorT){ ++ret; });            
      return ret;
  }
};


class NoopDestructionPolicy
{
public:

  inline void destruct(void*)
  {
      // NOOP
  }

  template<typename IteratorT>
  inline void destructAll(IteratorT, IteratorT)
  {
      // NOOP
  }
};


class OperatorDeleteDestructionPolicy
{
public:
  template<typename T>
  void destruct(T* ptr)
  {
      delete ptr;
  }

  template<typename IteratorT>
  void destructAll(IteratorT first, IteratorT last)
  {
      for (IteratorT iter = first; iter != last; ++iter)
      {
          delete iter.get();
      }
  }
};


/**
* need prev and next Pointers in DataT; use the hook base or member classes
* make use of empty baseclass optimisation
*/
template<typename DataT,
      typename AccessorPolicyT = ListBaseHookAccessor<DataT>,
      typename SizeCalculationPolicyT = SizeHoldingPolicy,
      typename DestructionPolicyT = NoopDestructionPolicy>
class List : protected AccessorPolicyT, protected SizeCalculationPolicyT, protected DestructionPolicyT
{
public:

  template<typename IteratorDataT>
  struct ListIterator : public IteratorTraits<IteratorDataT>, public AccessorPolicyT
  {
      friend class ListIterator<const DataT>;
      friend class ListIterator<DataT>;

      ListIterator()
      : ptr_(0)
      {
          // NOOP
      }

      ListIterator(typename IteratorTraits<IteratorDataT>::pointerType ptr)
      : ptr_(ptr)
      {
          // NOOP
      }

      ListIterator(const ListIterator<DataT>& rhs)
      : ptr_(rhs.ptr_)
      {
          // NOOP
      }

      ListIterator operator++()
      {
          if (ptr_)
              ptr_ = this->next(ptr_);
              
          return *this;
      }

      ListIterator operator--()
      {
          if (ptr_)
              ptr_ = this->prev(ptr_);
              
          return *this;
      }

      typename IteratorTraits<IteratorDataT>::pointerType operator->()
      {
          assert(ptr_);
          return ptr_;
      }

      // TODO make private with friend
      typename IteratorTraits<IteratorDataT>::pointerType get()
      {
          assert(ptr_);
          return ptr_;
      }

      typename IteratorTraits<IteratorDataT>::referenceType operator*()
      {
          assert(ptr_);
          return *ptr_;
      }

      bool operator!=(const ListIterator<IteratorDataT>& rhs) const
      {
          return ptr_ != rhs.ptr_;
      }

  private:
      typename IteratorTraits<IteratorDataT>::pointerType ptr_;
  };


  typedef ListIterator<DataT> iterator;
  typedef ListIterator<const DataT> const_iterator;

private:

  DataT* first_;
  DataT* last_;

public:

  List()
   : first_(0)
   , last_(0)
  {
      // NOOP
  }

  ~List()
  {
      this->destructAll(begin(), end());
  }

  iterator begin()
  {
      return first_;
  }

  const_iterator begin() const
  {
      return first_;
  }

  iterator end()
  {
      return 0;
  }

  const_iterator end() const
  {
      return 0;
  }

  iterator rbegin()
  {
      return last_;
  }

  const_iterator rbegin() const
  {
      return last_;
  }

  iterator rend()
  {
      return 0;
  }

  const_iterator rend() const
  {
      return 0;
  }

  void push_back(DataT& e)
  {
      this->add();

      this->setNext(&e, 0);
      this->setPrev(&e, last_);
      last_ = &e;

      if (this->prev(&e))
          this->setNext(this->prev(&e), &e);

      if (!first_)
          first_ = &e;
  }

  void pop_back()
  {
      if (last_)
      {
          void* last = last_;

          if (this->prev(last_))
          {
              this->setNext(this->prev(last_)) = 0;
          }
          else
          {
              first_ = 0;
              last_ = 0;
          }

          this->destruct(last);
          this->remove();
      }
  }

  void push_front(DataT& e)
  {
      this->add();

      setNext(&e, first_);
      setPrev(&e, 0);
      first_ = &e;

      if (next(&e))
          setPrev(next(&e)) = &e;

      if (!last_)
          last_ = &e;
  }

  void pop_front()
  {
      if (first_)
      {
          void* first = first_;

          if (next(first_))
          {
              setPrev(next(first_)) = 0;
          }
          else
          {
              first_ = 0;
              last_ = 0;
          }

          this->destruct(first);
          this->remove();
      }
  }

  const DataT& front() const
  {
      assert(first_);
      return *first_;
  }

  DataT& front()
  {
      assert(first_);
      return *first_;
  }

  const DataT& back() const
  {
      assert(last_);
      return *last_;
  }

  DataT& back()
  {
      assert(last_);
      return *last_;
  }

  inline
  bool empty() const
  {
      return first_ == 0;
  }

  void clear()
  {
      this->destructAll(begin(), end());
      this->reset();

      first_ = 0;
      last_ = 0;
  }

  iterator erase(iterator iter)
  {
      iterator ret = iter;

      if (iter != end())
      {
          if (prev(iter.get()))
              setNext(prev(iter.get()), next(iter.get()));
          else
              first_ = next(iter.get());

          if (next(iter.get()))
              setPrev(next(iter.get()), prev(iter.get()));
          else
              last_ = prev(iter.get());

          this->destruct(iter.get());
          this->remove();
      }

      return ret;
  }

   inline
   size_t size() const
   {
      return this->internal_size(begin(), end());
   }
  
   void swap(List& rhs)
   {
      this->clear();

      first_ = rhs.first_;
      last_ = rhs.last_;

      rhs.reset();

      rhs.first_ = 0;
      rhs.last_ = 0;
   }
};

}   // namespace intrusive

}   // namespace simppl


#endif   // SIMPPL_INTRUSIVE_LIST_H
