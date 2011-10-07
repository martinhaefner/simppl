// Intrusive List

#include <iostream>
#include <algorithm>

#include "intrusive_list.h"


// TODO access by typedef'd iterator (e.g. pointer or index for SHM or offset or anything other)
struct MyData : public intrusive::ListBaseHook
{
    int i_;
};

struct MyOtherData
{
    ~MyOtherData()
    {
        std::cout << "+MyOtherData" << std::endl;
    }

    int d_;
    intrusive::ListMemberHook thehook_;
};


int main()
{
    MyData data[40];
    for (unsigned int i = 0; i<sizeof(data)/sizeof(data[0]); ++i)
    {
        data[i].i_ = i;
    }

    intrusive::List<MyData> dt;
    const intrusive::List<MyData>& empty(dt);

    dt.push_back(data[0]);
    dt.push_back(data[4]);
    dt.push_back(data[3]);
    dt.push_back(data[1]);
    dt.push_back(data[2]);

    std::cout << "size=" << empty.size() << std::endl;

    for (intrusive::List<MyData>::const_iterator iter = empty.begin();
         iter != empty.end();
         ++iter)
    {
        std::cout << (*iter).i_ << std::endl;
        //(*iter).i_ = 4;   // const_iterator...
    }

    //MyOtherData od[10];
    MyOtherData* od0 = new MyOtherData();
    od0->d_ = 1;
    MyOtherData* od1 = new MyOtherData();
    od1->d_ = 2;
    MyOtherData* od2 = new MyOtherData();
    od2->d_ = 3;

    typedef intrusive::List<
            MyOtherData,
            intrusive::ListMemberHookAccessor<MyOtherData, &MyOtherData::thehook_>,
            intrusive::CalculatingPolicy,
            intrusive::OperatorDeleteDestructionPolicy
        > another_type;

    another_type another;
    another.push_back(*od2);
    another.push_back(*od1);
    another.push_back(*od0);
    std::cout << "size=" << another.size() << std::endl;

    for (another_type::const_iterator iter = another.begin(); iter != another.end(); ++iter)
    {
        std::cout << (*iter).d_ << std::endl;
    }

    std::cout << "next=" << another.erase(another.begin())->d_ << std::endl;

    for (another_type::const_iterator iter = another.begin(); iter != another.end(); ++iter)
    {
        std::cout << (*iter).d_ << std::endl;
    }

    another.clear();

    return 0;
}
