
#include "../VALUES/value.h"

typedef struct
{
    char *name;
    bool constant;
    Value value;
    dtype var_dtype;

} Entry;


typedef struct Environment
{
    Entry *entries;
    size_t count;
    size_t capacity;

    struct Environment *enclosing;
} Environment;


void init_environment(Environment *environment);
void free_environment(Environment *environment);

bool add_entry(Environment *environment,
               Value value,
               const char* name,
               bool is_const,
               dtype var_dtype);

Value get_entry(Environment *environment, const char *name);
bool assign_entry(Environment *environment, const char* name, Value value);
bool remove_entry(Environment *environment, const char *name);
bool is_define_entry(Environment *environment, const char *name);

Environment *new_environment(Environment *enclosing);
void destroy_environment(Environment *enclosing);
