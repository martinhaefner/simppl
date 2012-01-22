#ifndef TYPETRAITS_H
#define TYPETRAITS_H


template<typename FirstT, typename SecondT>
struct is_same
{
    enum { value = 0 };
};

template<typename T>
struct is_same<T, T>
{
    enum { value = 1 };
};


// ---------------------------------------------------------------------------------------


template<typename T>
struct add_reference
{
    typedef T& type;
};

template<typename T>
struct add_reference<T&>
{
    typedef T& type;
};

template<typename T>
struct add_reference<const T&>
{
    typedef const T& type;
};


// ---------------------------------------------------------------------------------------


// need partial template specialization
template<typename T>
struct remove_ref
{
    typedef T type;
};


template<typename T>
struct remove_ref<T&>
{
    typedef T type;
};

template<typename T>
struct remove_ref<const T&>
{
    typedef const T type;
};


// ---------------------------------------------------------------------------------------


// need partial template specialization
template<typename T> struct remove_const { typedef T type; };

template<typename T> struct remove_const<const T> { typedef T type; };
template<typename T> struct remove_const<const T*> { typedef T* type; };
template<typename T> struct remove_const<const T&> { typedef T& type; };
template<typename T> struct remove_const<volatile const T> { typedef volatile T type; };
template<typename T> struct remove_const<volatile const T*> { typedef volatile T* type; };
template<typename T> struct remove_const<volatile const T&> { typedef volatile T& type; };


// ---------------------------------------------------------------------------------------


// need partial template specialization
template<typename T>
struct remove_ptr
{
    typedef T type;
};


template<typename T>
struct remove_ptr<T*>
{
    typedef T type;
};

template<typename T>
struct remove_ptr<const T*>
{
    typedef const T type;
};

template<typename T>
struct remove_ptr<T&>
{
    typedef T& type;
};


// ---------------------------------------------------------------------------------------


template<typename T> struct is_integral { enum { value = false }; };

template<> struct is_integral<unsigned char>  { enum { value = true  }; };
template<> struct is_integral<unsigned short> { enum { value = true  }; };
template<> struct is_integral<unsigned int>   { enum { value = true  }; };
template<> struct is_integral<unsigned long>  { enum { value = true  }; };
template<> struct is_integral<unsigned long long> { enum { value = true  }; };

template<> struct is_integral<signed char>  { enum { value = true  }; };
template<> struct is_integral<signed short> { enum { value = true  }; };
template<> struct is_integral<signed int>   { enum { value = true  }; };
template<> struct is_integral<signed long>  { enum { value = true  }; };
template<> struct is_integral<signed long long> { enum { value = true  }; };

template<> struct is_integral<char> { enum { value = true  }; };
template<> struct is_integral<bool> { enum { value = true  }; };


// -----------------------------------------------------------------------


template<typename T> struct is_float { enum { value = false }; };

template<> struct is_float<float>  { enum { value = true  }; };
template<> struct is_float<double> { enum { value = true  }; };


// -----------------------------------------------------------------------


template<typename T> struct is_pointer                    { enum { value = false }; };

template<typename T> struct is_pointer<T*>                { enum { value = true  }; };
template<typename T> struct is_pointer<const T*>          { enum { value = true  }; };
template<typename T> struct is_pointer<const volatile T*> { enum { value = true  }; };


// -----------------------------------------------------------------------


template<typename T> struct is_reference                    { enum { value = false }; };

template<typename T> struct is_reference<T&>                { enum { value = true  }; };
template<typename T> struct is_reference<const T&>          { enum { value = true  }; };
template<typename T> struct is_reference<const volatile T&> { enum { value = true  }; };


// ----------------------------------------------------------------------------------------------


template<typename DerivedT, typename BaseT>
struct is_a
{
	struct yes_type
	{
		char dummy;
	};

	struct no_type
   {
      int dummy;
   };

	static yes_type test(const BaseT&, const BaseT&);
	static no_type  test(const BaseT&, ...);

	static DerivedT& make_derived();
	static BaseT& make_base();

	enum { value = (sizeof(test(make_base(), make_derived())) == sizeof(yes_type) ? true : false ) };
};


#endif  // TYPETRAITS_H
