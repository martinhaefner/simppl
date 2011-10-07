#include <iostream>

#include "include/sigslot.h"


class TestSlot : public Slot
{
public:
    void slotOutput(const std::string& str)
    {
        std::cout << str << std::endl;
    }

    void slotOutput2(int i, double d)
    {
        std::cout << "i=" << i << ", d=" << d << std::endl;
    }
};


class Test
{
public:

    Test(TestSlot& slot)
    {
        m_sig.connect(&slot, &TestSlot::slotOutput);
    }

    void sayHello()
    {
        m_sig.emit("Hallo Welt");
    }

    Signal<const std::string&> m_sig;
//    Signal<int, double> m_sig2;
};


int main()
{
    TestSlot* s = new TestSlot();
    TestSlot* s1 = new TestSlot();

    Test t(*s);
    t.m_sig.connect(s1, &TestSlot::slotOutput);

    //t.m_sig2.connect(s1, &TestSlot::slotOutput2);
    //t.m_sig2.connect(s, &TestSlot::slotOutput2);

    t.sayHello();

    //t.m_sig2.emit(2, 3.1415);
    //t.m_sig2.disconnect(s, &TestSlot::slotOutput2);
    //t.m_sig2.emit(3, 42.0);

    delete s;
    t.sayHello();

    //t.m_sig2.emit(4, 7.1);

    return 0;
}
