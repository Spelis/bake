#pragma once

#include <stdarg.h>

void LOG(const char* fmt, ...);

int indent_log(int delta);
