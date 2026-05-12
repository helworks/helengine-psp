#include <sys/types.h>

extern "C" int ftruncate(int, off_t) {
    return 0;
}
