#ifndef TIE_H
#define TIE_H


template<typename T1>
struct Tie1
{
   inline
   Tie1(T1& t1)
    : t1_(t1)    
   {
      // NOOP
   }
   
   inline
   void operator=(const Tuple<T1>& tup)
   {
      t1_ = tup.template get<0>();     
   }
   
   T1& t1_;   
};


template<typename T1, typename T2>
struct Tie2
{
   inline
   Tie2(T1& t1, T2& t2)
    : t1_(t1)
    , t2_(t2)
   {
      // NOOP
   }
   
   inline
   void operator=(const Tuple<T1, T2>& tup)
   {
      t1_ = tup.template get<0>();
      t2_ = tup.template get<1>();
   }
   
   T1& t1_;
   T2& t2_;
};

template<typename T1, typename T2, typename T3>
struct Tie3
{
   inline
   Tie3(T1& t1, T2& t2, T3& t3)
    : t1_(t1)
    , t2_(t2)
    , t3_(t3)
   {
      // NOOP
   }
   
   inline
   void operator=(const Tuple<T1, T2, T3>& tup)
   {
      t1_ = tup.template get<0>();
      t2_ = tup.template get<1>();
      t3_ = tup.template get<2>();
   }
   
   T1& t1_;
   T2& t2_;
   T3& t3_;   
};

template<typename T1, typename T2, typename T3, typename T4>
struct Tie4
{
   inline
   Tie4(T1& t1, T2& t2, T3& t3, T4& t4)
    : t1_(t1)
    , t2_(t2)
    , t3_(t3)
    , t4_(t4)
   {
      // NOOP
   }
   
   inline
   void operator=(const Tuple<T1, T2, T3, T4>& tup)
   {
      t1_ = tup.template get<0>();
      t2_ = tup.template get<1>();
      t3_ = tup.template get<2>();
      t4_ = tup.template get<3>();
   }
   
   T1& t1_;
   T2& t2_;
   T3& t3_;
   T4& t4_;
};

template<typename T1>
Tie1<T1> tie(T1& t1)
{
   return Tie1<T1>(t1);
}

template<typename T1, typename T2>
Tie2<T1, T2> tie(T1& t1, T2& t2)
{
   return Tie2<T1, T2>(t1, t2);
}

template<typename T1, typename T2, typename T3>
Tie3<T1, T2, T3> tie(T1& t1, T2& t2, T3& t3)
{
   return Tie3<T1, T2, T3>(t1, t2, t3);
}

template<typename T1, typename T2, typename T3, typename T4>
Tie4<T1, T2, T3, T4> tie(T1& t1, T2& t2, T3& t3, T4& t4)
{
   return Tie4<T1, T2, T3, T4>(t1, t2, t3, t4);
}

// more to follow...


#endif  // TIE_H
