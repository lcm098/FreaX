#define _POSIX_C_SOURCE 200809L
#include "./storage.h"

/*
typedef struct
{
    char *name;
    bool constant;
    Value value;

} Entry;


typedef struct Environment
{
    Entry *entries;
    size_t count;
    size_t capacity;

    struct Environment *enclosing;
} Environment;
*/

/*
  Environment *global = new_environment(NULL);
  Environment *local = new_environment(global);
  Environment *inner = new_environment(local);
*/

Environment *new_environment(Environment *enclosing)
{
    Environment *environment = malloc(sizeof(Environment));

    if (environment == NULL)
        return NULL;

    init_environment(environment);
    environment->enclosing = enclosing;

    return environment;
}

void destroy_environment(Environment *environment)
{
    if (environment == NULL)
        return;

    free_environment(environment);
    free(environment);
}

bool is_define_entry(Environment *environment, const char *name)
{
    for (size_t i = 0; i < environment->count; i++) {
        if (strcmp(environment->entries[i].name, name) == 0) {
            return true;
        }
    }

    return false;
}


bool remove_entry(Environment *environment, const char *name)
{
    for (size_t i = 0; i < environment->count; i++) {
        if (strcmp(environment->entries[i].name, name) == 0) {

            free(environment->entries[i].name);

            for (size_t j = i; j < environment->count - 1; j++) {
                environment->entries[j] = environment->entries[j + 1];
            }

            environment->count--;
            return true;
        }
    }
    return false;
}

bool assign_entry(Environment *environment, const char *name, Value value)
{
    for (size_t i = 0; i < environment->count; i++) {
        Entry *entry = &environment->entries[i];

        if (strcmp(entry->name, name) == 0) {

            if (entry->constant) {
                fprintf(stderr, "Cannot assign to constant variable '%s'.\n", name);
                return false;
            }

            entry->value = value;
            return true;
        }
    }

    if (environment->enclosing != NULL) {
        return assign_entry(environment->enclosing, name, value);
    }

    fprintf(stderr, "Undefined variable '%s'.\n", name);
    return false;
}

Value get_entry(Environment *environment, const char *name)
{
    for (size_t i = 0; i < environment->count; i++) {
        if (strcmp(environment->entries[i].name, name) == 0) {
            return environment->entries[i].value;
        }
    }

    if (environment->enclosing != NULL) {
        return get_entry(environment->enclosing, name);
    }

    fprintf(stderr, "Undefined variable '%s'.\n", name);
    exit(EXIT_FAILURE);
}

bool add_entry(Environment *environment,
               Value value,
               const char *name,
               bool is_const,
               dtype var_dtype)
{
    if (environment->capacity < environment->count + 1) {
        size_t old_capacity = environment->capacity;
        size_t new_capacity = GROW_CAPACITY(old_capacity);

        environment->entries = GROW_ARRAY(
            Entry,
            environment->entries,
            old_capacity,
            new_capacity
        );

        environment->capacity = new_capacity;
    }

    char *entry_name = strdup(name);

    if (entry_name == NULL)
        return false;

    Entry *entry = &environment->entries[environment->count];

    entry->name = entry_name;
    entry->constant = is_const;
    entry->value = value;
    entry->var_dtype = var_dtype;

    environment->count++;
    return true;
}

void init_environment(Environment *environment)
{
    environment->entries = NULL;
    environment->enclosing = NULL;
    environment->count = 0;
    environment->capacity = 0;;
}

void free_environment(Environment *environment)
{
    for (size_t i=0; i<environment->count; i++) {
        free(environment->entries[i].name);
    }

    FREE_ARRAY(Entry, environment->entries, environment->capacity);
    init_environment(environment);
}
