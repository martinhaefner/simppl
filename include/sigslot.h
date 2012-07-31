#ifndef SIGSLOT_H
#define SIGSLOT_H


#include <list>
#include <algorithm>

#include "tuple.h"


struct NoType
{
};


class AbstractConnection
{
    friend class Slot;

public:

    AbstractConnection()
     : next_(0)
    {
        // NOOP
    }

    virtual ~AbstractConnection()
    {
        // NOOP
    }

    virtual void dispatch(const void*) = 0;
    virtual void dispatch_1(const void*) = 0;
    
    virtual bool equals(const AbstractConnection& conn) const = 0;

    bool operator== (const AbstractConnection& conn) const
    {
        return this->equals(conn);
    }

protected:

    /// callback for slots going through their destructor
    virtual void slotDied() = 0;


public:
    // used in intrusive slist...
    AbstractConnection* next_;
};


class Slot
{
    AbstractConnection* m_depends;

public:

    Slot()
     : m_depends(0)
    {
        // NOOP
    }

    virtual ~Slot()
    {
        AbstractConnection* next = m_depends;
        while(next)
        {
            next->slotDied();
            AbstractConnection* old = next;
            next = next->next_;
            old->next_ = 0;
        }
    }

    /// add another connection at this slot object
    void addConnection(AbstractConnection* conn)
    {
        AbstractConnection* head = m_depends;
        m_depends = conn;
        m_depends->next_ = head;
    }

    /// connection dies -> remove callback
    void removeConnection(AbstractConnection* search)
    {
        std::cout << "removeConnection" << std::endl;

        AbstractConnection* last = 0;
        AbstractConnection* current = m_depends;

        while(current)
        {
            if (current == search)
            {
                if (last == 0)
                {
                    m_depends = current->next_;
                }
                else
                {
                    last->next_ = current->next_;
                }

                break;
            }

            last = current;
            current = current->next_;
        }
    }
};


template<typename SlotT, typename FuncT, typename ParamT>
class Connection : public AbstractConnection
{
public:

    Connection(SlotT& slot, FuncT func)
     : slot_(&slot), func_(func)
    {
        slot_->addConnection(this);
    }

    ~Connection()
    {
        if (slot_)
            slot_->removeConnection(this);
    }

    void addRef(...)    // no Slot baseclass inherited
    {
        // NOOP
    }

    void addRef(Slot& slot)
    {
        std::cout << "addRef " << &slot << std::endl;
        slot.addConnection(this);
    }

    void slotDied()
    {
        std::cout << "slotDied" << std::endl;
        slot_ = 0;
    }

    void dispatch(const void* data)
    {
      // enable_if since this is virtual this must be instantiated.
      #if 0
        if (slot_)
        {
            const ParamT* param = reinterpret_cast<
                const typename remove_ref<ParamT>::type *>(data);

            internaldispatch(param->template get<0>(),
                             param->template get<1>(),
                             param->template get<2>());
        }
	#endif
    }

    void dispatch_1(const void* data)
    {
      // FIXME don't need this, it can be dispatched via compiler in the dispatch method...
        if (slot_)
        {
            const typename remove_ref<ParamT>::type* param = reinterpret_cast<
                const typename remove_ref<ParamT>::type *>(data);

            internaldispatch(*param);
        }
    }

    template<typename T>
    inline void internaldispatch(T t, NoType, NoType)
    {
        (slot_->*func_)(t);
    }
    
    template<typename T>
    inline void internaldispatch(T t)
    {
        (slot_->*func_)(t);
    }

    template<typename T1, typename T2>
    inline void internaldispatch(const Tuple<T1, T2, NoType>& t)
    {
        (slot_->*func_)(t.template get<0>(), t.template get<1>());
    }

    template<typename T1, typename T2, typename T3>
    inline void internaldispatch(const Tuple<T1, T2, T3>& t)
    {
        (slot_->*func_)(t.template get<0>(), t.template get<1>(), t.template get<2>());
    }

    bool equals(const AbstractConnection& conn) const
    {
        const Connection<SlotT, FuncT, ParamT>* c
            = dynamic_cast<const Connection<SlotT, FuncT, ParamT>*>(&conn);

        return c && c->slot_ == slot_ && c->func_ == func_;
    }

private:

    SlotT* slot_;
    FuncT func_;
};


template<typename InheriterT>
class SignalBase
{
protected:
    
    ~SignalBase()
    {
        disconnectAll();
    }


  public:
    
    /// connect with certain slot
    template<typename SlotT, typename FuncT>
    void connect(SlotT* slot, FuncT func)
    {
        if (slot)
        {
            Connection<SlotT, FuncT, typename InheriterT::param_type>* newconn
                = new Connection<SlotT, FuncT, typename InheriterT::param_type>(*slot, func);
            m_slots.push_back(newconn);
        }
    }

    template<typename SlotT, typename FuncT>
    void disconnect(SlotT* slot, FuncT func)
    {
        if (slot)
        {
            Connection<SlotT, FuncT, typename InheriterT::param_type> conn(*slot, func);

            for (std::list<AbstractConnection*>::iterator iter = m_slots.begin();
                    iter != m_slots.end(); ++iter)
            {
                if (conn == **iter)
                {
                    std::cout << "Removing" << std::endl;
                    delete *iter;
                    m_slots.remove(*iter);
                    break;
                }
            }
        }
    }

    /// disconnect all
    void disconnectAll()
    {
        std::cout << "disconnectAll" << std::endl;

        for (std::list<AbstractConnection*>::iterator iter = m_slots.begin();
             iter != m_slots.end(); ++iter)
        {
            std::cout << "deleting..." << std::endl;
            delete *iter;
        }
        m_slots.clear();
    }

protected: 
    
    std::list<AbstractConnection*> m_slots;
};


template<typename T1>
class Signal_1 : public SignalBase<Signal_1<T1> >
{
public:
  
    typedef SignalBase<Signal_1<T1> > base_type;
    typedef T1 param_type;

    /// emit signal
    void emit(T1 t)
    {
        for (std::list<AbstractConnection*>::iterator iter = base_type::m_slots.begin();
             iter != base_type::m_slots.end(); ++iter)
        {
            (*iter)->dispatch_1(&t);
        }
    }
};



template<typename T1, typename T2>
class Signal_2 : public SignalBase<Signal_2<T1, T2> >
{
public:
  
    typedef SignalBase<Signal_2<T1, T2> > base_type;
    typedef Tuple<T1, T2, NoType> param_type;
    
    /// emit signal
    void emit(T1 t1, T2 t2)
    {
        param_type param(t1, t2);

        for (std::list<AbstractConnection*>::iterator iter = base_type::m_slots.begin();
             iter != base_type::m_slots.end(); ++iter)
        {
            (*iter)->dispatch(&param);
        }
    }
};


template<typename T1, typename T2, typename T3>
class Signal_3 : public SignalBase<Signal_3<T1, T2, T3> >
{
public:
  
    typedef SignalBase<Signal_3<T1, T2, T3> > base_type;
    typedef Tuple<T1, T2, T3> param_type;

    /// emit signal
    void emit(T1 t1, T2 t2, T3 t3)
    {
        param_type param(t1, t2, t3);

        for (std::list<AbstractConnection*>::iterator iter = base_type::m_slots.begin();
             iter != base_type::m_slots.end(); ++iter)
        {
            (*iter)->dispatch(&param);
        }
    }
};


template<typename T1, typename T2 = NoType, typename T3 = NoType>
class Signal 
 : public if_<is_same<T2, NoType>::value, Signal_1<T1>, 
   typename if_<is_same<T3, NoType>::value, Signal_2<T1, T2>, Signal_3<T1, T2, T3> >::type>::type
{
};


#endif  // SIGSLOT_H
