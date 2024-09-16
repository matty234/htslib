#ifndef ZSTD_DICT_H
#define ZSTD_DICT_H

#include <zdict.h>
#include "../htslib/hts.h"

#define KB (1024)
#define CONTENT_TYPE_COUNT 7
#define INITIAL_CAPACITY 2


extern char *dict_names[CONTENT_TYPE_COUNT];

void initialize_example_arrays();
int add_example(int content_type, const char *data, size_t size);
int write_zdicts();

#endif // ZSTD_DICT_H
