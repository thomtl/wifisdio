#pragma once

#include <stddef.h>

void* net_malloc(size_t s);
void net_free(void* ptr);