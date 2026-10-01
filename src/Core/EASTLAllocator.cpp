// EASTLAllocator.cpp

#include <EASTL/allocator.h>
#include <cstdarg>
#include <cstdio>
#include <new>

void* operator new[](size_t size, const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/,
                     int /*line*/)
{
	return ::operator new[](size);
}

void* operator new[](size_t size, size_t alignment, size_t alignmentOffset, const char* /*pName*/, int /*flags*/,
                     unsigned /*debugFlags*/, const char* /*file*/, int /*line*/)
{
	if (alignment > __STDCPP_DEFAULT_NEW_ALIGNMENT__ || alignmentOffset % alignment != 0) throw std::bad_alloc();

	return ::operator new[](size);
}

void operator delete[](void* p, const char* /*pName*/, int /*flags*/, unsigned /*debugFlags*/, const char* /*file*/,
                       int /*line*/) EA_NOEXCEPT
{
	::operator delete[](p);
}

void operator delete[](void* p, size_t /*alignment*/, size_t /*alignmentOffset*/, const char* /*pName*/, int /*flags*/,
                       unsigned /*debugFlags*/, const char* /*file*/, int /*line*/) EA_NOEXCEPT
{
	::operator delete[](p);
}

namespace EA::StdC
{
	int Vsnprintf(char* __restrict pDestination, unsigned __int64 n, const char* __restrict pFormat, char* arguments)
	{
		return ::vsnprintf(pDestination, static_cast<size_t>(n), pFormat, (va_list)arguments);
	}
} // namespace EA::StdC
