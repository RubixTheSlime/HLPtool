#ifndef ARG_GLOBAL_H
#define ARG_GLOBAL_H
#include "../config.h"
#include "global.h"
#include "argp.h"

struct arg_settings_global {
    int verbosity;
    enum LAYER_NOTATION layer_notation;
};

#endif
