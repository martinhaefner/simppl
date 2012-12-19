#ifndef SIMPPL_ALIGNEDSTORAGE_H
#define SIMPPL_ALIGNEDSTORAGE_H


namespace detail
{

template<typename T>
struct AlignmentHelper
{
	unsigned char c;
	T t;
};


template<size_t A, size_t B>
struct SizeDiscriminator
{
	enum { value = A<B ? A:B };
};


template<size_t alignment>
struct AlignedType;

// template specializations for discriminated aligned types
template<> struct AlignedType<1> { typedef unsigned char  type; };
template<> struct AlignedType<2> { typedef unsigned short type; };
template<> struct AlignedType<4> { typedef unsigned int type; };
template<> struct AlignedType<8> { typedef double type; };

}   // namespace detail


template<typename T>
struct alignment_of
{	
   enum { value = detail::SizeDiscriminator<sizeof(detail::AlignmentHelper<T>) 
          - sizeof(T), sizeof(T)>::value };   
};


template<size_t size, size_t alignment>
struct AlignedStorage
{      
   inline
	void* address()
   {
      return this; 
   }
	
   inline   
	const void* address() const
   {
      return this; 
   }   
   
private:
      
	union tDataType 
	{      
		unsigned char m_buf[size];
      typename detail::AlignedType<alignment>::type m_align;
	} m_data;
};


#endif   // SIMPPL_ALIGNEDSTORAGE_H
