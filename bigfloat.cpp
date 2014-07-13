#include "bigfloat.h"
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <iostream>

using namespace std;

#ifndef USE_DOUBLE_FOR_BIGFLOAT
const char * BigFloat::c_str() const
{
    static thread_local char buffer[512];
    ostringstream os;
    os << *this;
    strcpy(buffer, os.str().c_str());
    return buffer;
}
#endif

#if 0 // testing code
namespace
{
struct initializer
{
    initializer()
    {
        cout << (1_bf + 1_bf) << endl;
        exit(0);
    }
} init;
}
#endif // testing code
