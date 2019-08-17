//
// Created by pengyibo on 2019-07-31.
//

#ifndef UNTITLED1_UTIL_H
#define UNTITLED1_UTIL_H
#include <assert.h>
#include <string.h>
#include <string>
#include <pthread.h>

class T
{
public:
    T(const char* str, int len)
            :str_(str),
             len_(len)
    {
    }

    const char* str_;
    const size_t len_;
};
#endif //UNTITLED1_UTIL_H
