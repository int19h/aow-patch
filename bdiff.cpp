#include "stdafx.h"

#ifdef BDIFF
int wmain(int argc, wchar_t** argv) {
    assert(argc == 4);
    DELTA_INPUT di = {};
    CreateDeltaW(DELTA_FILE_TYPE_RAW, 0, 0, argv[1], argv[2], nullptr, nullptr, di, nullptr, 32, argv[3]);
    return 0;
}
#endif
