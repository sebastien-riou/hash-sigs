#pragma once
#include <stdio.h>
static void soprint_impl(const char*msg){printf("%s",msg);}
#define PRINT_PREFIX so
#include "print.h"
