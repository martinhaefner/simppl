#include <if.h>
#include <typetraits.h>
#include <function.h>
#include <static_check.h>

#include <iostream>


// forward decl
class slot;


// slot lifetime tracking base class for connections
struct tracking_base
{
  friend class slot;
  
protected:
  
  tracking_base(slot* slot);
   
  void release(slot* slot);
  
  inline
  bool isValid() const
  {
    return valid_;
  }

private:
  
  tracking_base* next_;
  bool valid_;
  
  inline
  void slotDied()
  {
    valid_ = false;
  }
};


struct slot
{
  friend class tracking_base;
  
protected:

  inline
  slot()
   : cns_(0)
  {
    // NOOP
  }
  
  ~slot()
  {
    tracking_base* cn = cns_;
    while(cn)
    {
      cn->slotDied();
      cn = cn->next_;
    }
  }

private:

  inline
  void add(tracking_base* cn)
  {
    tracking_base* next = cns_;
    cns_ = cn;
    cn->next_ = next;
  }
  
  void remove(tracking_base* cn)
  {
    tracking_base* iter = cns_;
    tracking_base* last = 0;
    
    while(iter && iter != cn) 
    {
      last = iter; ++iter; 
    }
    
    if (iter == cn)
    {
      if (last) 
      {
	last->next_ = cn->next_;
      }
      else
	cns_ = cn->next_;
    }
  }

  tracking_base* cns_;  
};


inline
tracking_base::tracking_base(slot* slot)
 : next_(0)
 , valid_(true)
{
  slot->add(this);
}
  
  
inline
void tracking_base::release(slot* slot)
{
  slot->remove(this);
}


// ---------------------------------------------------------------------------

// FIXME maybe find a way to send 4-byte values within the pointer area and not reference and again reference it
//       so this should again be a bit faster...
template<typename T>
struct call_helper
{
  static inline 
  void* toVoidPtr(T& t)
  {
    return &t;
  }
  
  static inline
  T fromVoidPtr(void* p)
  {
    return *(T*)p;
  }
};


template<typename T>
struct call_helper<T*>
{
  static inline 
  void* toVoidPtr(T* t)
  {
    return t;
  }
  
  static inline
  T* fromVoidPtr(void* p)
  {
    return (T*)p;
  }
};


template<typename T>
struct call_helper<const T*>
{
  static inline 
  void* toVoidPtr(const T* t)
  {
    return const_cast<T*>(t);
  }
  
  static inline
  const T* fromVoidPtr(void* p)
  {
    return (T*)p;
  }
};


template<typename T>
struct call_helper<T&>
{
  static inline 
  void* toVoidPtr(T& t)
  {
    return &t;
  }
  
  static inline
  T& fromVoidPtr(void* p)
  {
     return *(T*)p;
  }
};


template<typename T>
struct call_helper<const T&>
{
  static inline 
  void* toVoidPtr(const T& t)
  {
    return const_cast<T*>(&t);
  }
  
  static inline
  const T& fromVoidPtr(void* p)
  {
     return *(T*)p;
  }
};


// ---------------------------------------------------------------------------


// non-slot-lifetime tracking base for connections
struct non_tracking_base
{
  inline
  non_tracking_base(...)
  {
    // do nothing, not a slot base class
  }
  
  inline
  void release(...)
  {
    // do nothing, there is no slot
  }
  
  inline
  bool isValid() const
  {
    return true;
  }
};


template<typename SlotT, typename FuncT>
struct connection_base : protected if_<is_a<SlotT, slot>::value, tracking_base, non_tracking_base>::type
{
  typedef typename if_<is_a<SlotT, slot>::value, tracking_base, non_tracking_base>::type base_type;

protected:
  
  SlotT* slot_;
  FuncT func_;
  
  connection_base(SlotT* slot, FuncT func)
   : base_type(slot)
   , slot_(slot)
   , func_(func)
  {
    // NOOP
  }
  
  ~connection_base()
  {
    base_type::release(slot_);
  }
  
  connection_base* np_;  // need this here?
};


struct abstract_connection_0
{
  template<typename T, template<typename, typename> class U> friend struct signal_base;
  friend struct signal_0;
  
protected:
  
  virtual ~abstract_connection_0()
  {
    // NOOP
  }

  abstract_connection_0* np_;
  
  virtual void call() = 0;
};


struct abstract_connection_1
{
  template<typename T, template<typename, typename> class U> friend struct signal_base;
  template<typename T> friend struct signal_1;
  
protected:
  
  virtual ~abstract_connection_1()
  {
    // NOOP
  }

  abstract_connection_1* np_;
  
  virtual void call(void*) = 0;
};


template<typename SlotT, typename FuncT>
struct connection_0 : public abstract_connection_0, private connection_base<SlotT, FuncT>
{
  template<typename T, template<typename, typename> class U> friend struct signal_base;
  friend struct signal_0;
  
  typedef connection_base<SlotT, FuncT> cbase_type;
  
private:
  
  connection_0(SlotT* slot, FuncT func)
   : connection_base<SlotT, FuncT>(slot, func)
  {
    // NOOP
  }
  
  void call()
  {
    if (cbase_type::isValid())
      ((cbase_type::slot_)->*cbase_type::func_)();
  }
};


template<typename SlotT, typename FuncT>
struct connection_1 : public abstract_connection_1, private connection_base<SlotT, FuncT>
{
  template<typename T, template<typename, typename> class U> friend struct signal_base;
  template<typename T> friend struct signal_1;
  
  typedef connection_base<SlotT, FuncT> cbase_type;
  
private:
  
  connection_1(SlotT* slot, FuncT func)
   : connection_base<SlotT, FuncT>(slot, func)
  {
    // NOOP
  }
  
  void call(void* arg)
  {
    if (cbase_type::isValid())
      ((cbase_type::slot_)->*cbase_type::func_)
        (call_helper<typename TypeAt<1, typename FunctionTraits<FuncT>::param_type>::type>::fromVoidPtr(arg));
  }
};

/*
template<typename FuncT, typename SlotT>
struct Connection2 : private connection_base<FuncT, SlotT>
{
};


template<typename FuncT, typename SlotT>
struct Connection3 : private connection_base<FuncT, SlotT>
{
};
*/


template<typename AbsConnT, template<typename, typename> class ConcreteConnT>
struct signal_base
{
protected:
    
  signal_base()
   : conns_(0)
  {
    // NOOP
  }
  
  ~signal_base()
  {
    disconnect();
  }

public:
    
  template<typename SlotT, typename FuncT>
  void connect(SlotT* slot, FuncT func)
  {
    STATIC_CHECK((is_same<SlotT, typename remove_ref<typename TypeAt<0, typename FunctionTraits<FuncT>::param_type>::type>::type>::value), object_type_mismatch);
    
    // take connections out of memory pool?
    AbsConnT* cn = conns_;
    conns_ = new ConcreteConnT<SlotT, FuncT>(slot, func);
    conns_->np_ = cn;
  }
  
  template<typename SlotT, typename FuncT>
  void disconnect(SlotT* slot, FuncT func)
  {
    // FIXME
  }
  
  void disconnect()
  {
    AbsConnT* cn = conns_;
    AbsConnT* next = 0;
    while(cn)
    {
      next = cn->np_;
      delete cn;
      cn = next;
    }
  }

protected:
  
  // FIXME make copyable
  signal_base(const signal_base&);
  signal_base& operator=(const signal_base&);
  
  AbsConnT* conns_;
};


struct signal_0 : public signal_base<abstract_connection_0, connection_0>
{
  void emit()
  {
    abstract_connection_0* cn = conns_;
    while(cn)
    {
      cn->call();
      cn = cn->np_;
    }
  }
};


template<typename T>
struct signal_1 : private signal_base<abstract_connection_1, connection_1>
{
private:
    
  typedef signal_base<abstract_connection_1, connection_1> base_type;
  
public:
    
  template<typename SlotT, typename FuncT>
  void connect(SlotT* slot, FuncT func)
  {
    STATIC_CHECK((is_same<T, typename TypeAt<1, typename FunctionTraits<FuncT>::param_type>::type>::value), type_mismatch_on_first_argument);
    STATIC_CHECK((Size<typename FunctionTraits<FuncT>::param_type>::value == 2), argument_count_mismatch);

    base_type::connect(slot, func);
  }
  
  // FIXME maybe leave the signal non-templated and only template this member function would
  //       be fine too for the compiler. But what about the user?
  void emit(T t)
  { 
    abstract_connection_1* cn = conns_;
    while(cn)
    {
      cn->call(call_helper<T>::toVoidPtr(t));
      cn = cn->np_;
    }
  }
  
  using base_type::disconnect;
  
  // FIXME make sure, an inheriter cannot access the protected connections...
};


// -----------------------------------------------------------------


template<typename T1, typename T2>
struct signal2
{
};


template<typename T1, typename T2, typename T3>
struct signal3
{
};


// -----------------------------------------------------------------------


template<typename T1, typename T2, typename T3>
struct signalchooser;

template<>
struct signalchooser<NilType, NilType, NilType>
{
  typedef signal_0 type;
};

template<typename T1>
struct signalchooser<T1, NilType, NilType>
{
  typedef signal_1<T1> type;
};

template<typename T1, typename T2>
struct signalchooser<T1, T2, NilType>
{
  typedef signal2<T1, T2> type;
};

template<typename T1, typename T2, typename T3>
struct signalchooser
{
  typedef signal3<T1, T2, T3> type;
};


template<typename T1 = NilType, typename T2 = NilType, typename T3 = NilType>
struct signal : public signalchooser<T1, T2, T3>::type
{
  // all public member functions inherited...
};


// -----------------------------------------------------------------


// this should work too
struct Myslot : public slot
{
  void hello0()
  {
    std::cout << "Hello World" << std::endl;
  }
  
  void hello(int i)   
  {
    std::cout << "Hallo " << i << std::endl;
  }
  
  void doIt(const int& i)
  {
    std::cout << "doIt " << i << std::endl;
  }
  
  void doIt2(int& i)
  {
    std::cout << "doIt2 " << i << std::endl;
    i = 42;
  }
  
  void doItPtr(int* i)
  {
    std::cout << "doItPtr " << *i << std::endl;
    *i = 777;
  }
  
  void doItPtr2(const int* i)
  {
    std::cout << "doItPtr2 " << *i << std::endl;
  }
};


struct Anotherslot : public slot
{
  void welt(int i)
  {
    std::cout << "Welt " << i << std::endl;
  }
};


struct tester
{
  explicit
  tester(int i)
   : i(i)
  {
    std::cout << "ctor" << std::endl;
  }
  
  tester(const tester& t)
  {
    std::cout << "cctor" << std::endl;
  }
  
  tester& operator=(const tester& t)
  {
    std::cout << "op=" << std::endl;
  }
  
  ~tester()
  {
    std::cout << "dtor" << std::endl;
  }
  
  int i;
};


struct testerslot
{
  void test(const tester& t)
  {
    std::cout << t.i << std::endl;
  }
};


int main()
{
  // FIXME this is how 4-byte rule could work when casted...
  tester i(42);
  void* p = (void*&)i;
  tester& k = (tester&)p;
  
  signal<int> s;
  Myslot* myslot = new Myslot;
  Anotherslot a;
  s.connect(&a, &Anotherslot::welt);
  s.connect(myslot, &Myslot::hello);
  //delete myslot;
  s.emit(42);
  
  signal<> s0;
  s0.connect(myslot, &Myslot::hello0);
  s0.emit();
  
  signal<int&> s1;
//  s1.connect(myslot, &Myslot::doIt);
  s1.connect(myslot, &Myslot::doIt2);
  int j = 777;
  s1.emit(j);
  std::cout << "Now: " << j << std::endl;
  
  signal<const int*> s2;
  //s2.connect(myslot, &Myslot::doItPtr);
  s2.connect(myslot, &Myslot::doItPtr2);
  s2.emit(&j);
  std::cout << "Now: " << j << std::endl;
  
  testerslot t;
  signal<const tester&> s3;
  s3.connect(&t, &testerslot::test);
  s3.emit(tester(33));
  
  return EXIT_SUCCESS;
}
