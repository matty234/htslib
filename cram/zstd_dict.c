#include <zdict.h>
#include "zstd_dict.h"
#include "../htslib/hts.h"


static const unsigned g_defaultMaxDictSize = 110 * KB;

char ***zstd_dict_examples = NULL;  // Array of pointers to arrays of strings
size_t **zstd_dict_examples_sizes = NULL; // Array of pointers to arrays of sizes
size_t *current_capacity = NULL;          // Tracks the current capacity of each array
size_t zstd_dict_examples_offsets[CONTENT_TYPE_COUNT] = {0};

/*enum cram_inner_content {
    CRAM_INNER_CONTENT_SEQUENCE = 1,
    CRAM_INNER_CONTENT_QUALITY_SCORES = 2,
    CRAM_INNER_CONTENT_NAMES = 3,
    CRAM_INNER_CONTENT_BLK = 4,
    CRAM_INNER_CONTENT_BASE = 5,
    CRAM_INNER_CONTENT_SOFT_CLIP = 6,
    CRAM_INNER_CONTENT_OTHER = 7,
};
*/
char *dict_names[] = {
    "sequence",
    "quality_scores",
    "names",
    "blk",
    "base",
    "soft_clip",
    "other",
};

void print_sizes(size_t content_type)
{
    for (int i = 0; i < zstd_dict_examples_offsets[content_type]; i++)
    {
        printf("Size of example %d: %zu\n", i, zstd_dict_examples_sizes[content_type][i]);
    }
}

int add_example(int content_type, const char *data, size_t size)
{   


    // if the size is less than 8 bytes, ignore it
    if (size < 8)
    {
        hts_log_trace("Example size is less than 8 bytes");
        return 0;
    }
    // increase the size of offsets
    
    if (content_type < 0 || content_type >= CONTENT_TYPE_COUNT)
    {
        hts_log_error("Invalid content type %d", content_type);
        return -1;
    }

    if (content_type == 6)
    {
        // ignore other content type for the mo
       return 0;
    }
    
    if (zstd_dict_examples_sizes[content_type] == NULL)
    {
        zstd_dict_examples_sizes[content_type] = malloc(sizeof(size_t));
        zstd_dict_examples[content_type] = malloc(INITIAL_CAPACITY * sizeof(char *));
        current_capacity[content_type] = INITIAL_CAPACITY;
        zstd_dict_examples_offsets[content_type] = 0;
    }

    //print_sizes(content_type);
  
    zstd_dict_examples_sizes[content_type] = realloc(zstd_dict_examples_sizes[content_type], (zstd_dict_examples_offsets[content_type] + 1) * sizeof(size_t));
    if (!zstd_dict_examples_sizes[content_type])
    {
        perror("Failed to reallocate memory for sizes");
        exit(EXIT_FAILURE);
    }
    

    // if the new data is gonna exceed the current capacity, double the capacity
    if (current_capacity[content_type] + size > current_capacity[content_type])
    {
        size_t new_capacity = current_capacity[content_type] + size;
        char **new_examples = realloc(zstd_dict_examples[content_type], new_capacity * sizeof(char *));
        if (!new_examples)
        {
            perror("Failed to reallocate memory for example arrays");
            exit(EXIT_FAILURE);
        }

        zstd_dict_examples[content_type] = new_examples;
        current_capacity[content_type] = new_capacity;
    }


    zstd_dict_examples[content_type][zstd_dict_examples_offsets[content_type]] = strdup(data);
    zstd_dict_examples_sizes[content_type][zstd_dict_examples_offsets[content_type]] = size;
    zstd_dict_examples_offsets[content_type]++;
   // printf("Example added to content type %d\n: now at %zu\n", content_type, zstd_dict_examples_offsets[content_type]);

    return 0;
}

void initialize_example_arrays()
{
    zstd_dict_examples = malloc(CONTENT_TYPE_COUNT * sizeof(char **));
    zstd_dict_examples_sizes = malloc(CONTENT_TYPE_COUNT * sizeof(size_t *));
    current_capacity = malloc(CONTENT_TYPE_COUNT * sizeof(size_t));

    if (!zstd_dict_examples || !zstd_dict_examples_sizes || !current_capacity)
    {
        perror("Failed to allocate memory for example arrays");
        exit(EXIT_FAILURE);
    }

    for (int i = 0; i < CONTENT_TYPE_COUNT; i++)
    {
        zstd_dict_examples[i] = NULL; // Start with NULL, allocate as needed
        zstd_dict_examples_sizes[i] = NULL;
        current_capacity[i] = 0;
    }
}


int DiB_saveDict(const char *dictFileName,
                 const void *buff, size_t buffSize, char* data, size_t dataSize)
{
    // dicts / dictFileName

    char fullPath[256];
    snprintf(fullPath, sizeof(fullPath), "./dicts/%s", dictFileName);
    char dataFullPath[256];

    snprintf(dataFullPath, sizeof(fullPath), "./dicts/%s.data", dictFileName);

    FILE *const fData = fopen(dataFullPath, "wb");
    if (fData == NULL)
    {
        hts_log_error("Failed to open file %s for writing", dataFullPath);
        return -1;
    }

    fwrite(data, 1, dataSize, fData);
    fclose(fData);


    FILE *const f = fopen(fullPath, "wb");
    if (f == NULL)
    {
        hts_log_error("Failed to open file %s for writing", fullPath);
        return -1;
    }

    {
        //buffSize = 10;
        //printf("Writing dictionary to file %s\n", fullPath);
        // print buffer size
        //printf("Buffer size: %zu\n", buffSize);
        size_t const n = fwrite(buff, 1, buffSize, f);
        if (n != buffSize)
        {
            char *err = strerror(errno);
            hts_log_error("Failed to write dictionary to file %s: %s", fullPath, err);
            return -1;
        }
    }

    {
        size_t const n = (size_t)fclose(f);
        if (n != 0)
        {
            hts_log_error("Failed to close file %s", fullPath);
            return -1;
        }
    }

    return 0;
}



int write_zdicts()
{
    for (int i = 0; i < CONTENT_TYPE_COUNT; i++)
    {
        size_t number_of_examples = zstd_dict_examples_offsets[i];
        //printf("[%s] Number of examples: %d\n", dict_names[i], number_of_examples);

        if (number_of_examples > 4)
        {
            //printf("Number of examples: %d\n", number_of_examples);
            void *const dictBuffer = malloc(g_defaultMaxDictSize);
            if (dictBuffer == NULL)
            {
                hts_log_warning("Failed to allocate memory for dictionary buffer");
                return -1;
            }

            const void* data = zstd_dict_examples[i];

    
            size_t dictSize = ZDICT_trainFromBuffer(dictBuffer, g_defaultMaxDictSize, data, zstd_dict_examples_sizes[i], number_of_examples);
            if (ZDICT_isError(dictSize))
            {
                char *err = ZDICT_getErrorName(dictSize);
                hts_log_warning("Failed to train dictionary from buffer: %s", err);
                return -1;
            }
           

            size_t cumulSize = 0;
            for (int j = 0; j < number_of_examples; j++)
            {
                cumulSize += zstd_dict_examples_sizes[i][j];
            }

            char *dictFileName = dict_names[i];
            int save_result = DiB_saveDict(dictFileName, dictBuffer, dictSize, (void *)data, cumulSize);
            if (save_result != 0)
            {
                hts_log_warning("Failed to save dictionary %s", dict_names[i]);
                return -1;
            }

            hts_log_warning("Dictionary %s saved with %zu elements", dict_names[i], number_of_examples);

        }

        hts_log_info("Dictionary %s saved", dict_names[i]);
    }
    return 0;
}
