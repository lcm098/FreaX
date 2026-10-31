

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "./main.h"


static int insert_open_bracket(int count, int key) {
    (void)count;
    (void)key;

    rl_insert_text("[]");
    rl_point--;

    return 0;
}

static int insert_open_formatted_bracket(int count, int key) {
    (void)count;
    (void)key;

    rl_insert_text("f[]");
    rl_point--;

    return 0;
}

static int insert_open_braces(int count, int key) {
    (void)count;
    (void)key;

    rl_insert_text("{}");
    rl_point--;

    return 0;
}

static int insert_open_paren(int count, int key) {
    (void)count;
    (void)key;

    rl_insert_text("()");
    rl_point--;

    return 0;
}

static int insert_double_quote(int count, int key) {
    (void)count;
    (void)key;

    rl_insert_text("\"\"");
    rl_point--;

    return 0;
}

static int insert_single_quote(int count, int key) {
    (void)count;
    (void)key;

    rl_insert_text("''");
    rl_point--;

    return 0;
}

static bool append_command_text(
    char **buffer,
    size_t *length,
    size_t *capacity,
    const char *text,
    size_t text_length)
{
    if (text_length > SIZE_MAX - *length - 1) {
        return false;
    }

    size_t required = *length + text_length + 1;

    if (required > *capacity) {
        size_t new_capacity = (*capacity == 0) ? 128 : *capacity;

        while (new_capacity < required) {
            if (new_capacity > SIZE_MAX / 2) {
                new_capacity = required;
                break;
            }

            new_capacity *= 2;
        }

        char *new_buffer = realloc(*buffer, new_capacity);

        if (new_buffer == NULL) {
            return false;
        }

        *buffer = new_buffer;
        *capacity = new_capacity;
    }

    if (text_length > 0) {
        memcpy(*buffer + *length, text, text_length);
    }

    *length += text_length;
    (*buffer)[*length] = '\0';

    return true;
}

void read_eval_print_loop(void)
{
    rl_bind_key('[', insert_open_bracket);
    rl_bind_key('(', insert_open_paren);
    rl_bind_key('{', insert_open_braces);
    rl_bind_key('"', insert_double_quote);
    rl_bind_key('\'', insert_single_quote);
    rl_bind_key('f', insert_open_formatted_bracket);

    for (;;) {
        char *cwd = getcwd(NULL, 0);
        char hostname[256] = {0};
        const char *is_root = (geteuid() == 0) ? "True" : "False";

        if (gethostname(hostname, sizeof(hostname) - 1) != 0) {
            snprintf(hostname, sizeof(hostname), "unknown");
        }

        if (cwd != NULL) {
            printf(
                BOLD "[" GREEN "%s" RESET "]"
                "::"
                "[" BLUE "%s" RESET "]"
                "::"
                "root=[" RED "%s" RESET "]\n",
                cwd, hostname, is_root
            );

            free(cwd);
        } else {
            fprintf(stderr, "PROMPT ERROR: unable to find current directory\n");
            printf(BOLD">>>" RESET);
            printf("\n");
        }

        char *command = NULL;
        size_t length = 0;
        size_t capacity = 0;
        bool input_ended = false;
        const char *prompt = RED"||=> "YELLOW;

        for (;;) {
            char *line = readline(prompt);

            if (line == NULL) {
                input_ended = true;
                break;
            }

            size_t line_length = strlen(line);
            bool continuation =
                line_length > 0 && line[line_length - 1] == '\\';

            if (continuation) {
                line_length--;
            }

            bool appended = append_command_text(
                &command,
                &length,
                &capacity,
                line,
                line_length
            );

            free(line);

            if (!appended) {
                fprintf(stderr, "ush: not enough memory for command\n");
                input_ended = true;
                break;
            }

            if (!continuation) {
                break;
            }

            prompt = CYAN"...> "RESET;
        }

        if (input_ended) {
            free(command);
            break;
        }

        if (command == NULL) {
            command = malloc(1);

            if (command == NULL) {
                fprintf(stderr, "ush: not enough memory for command\n");
                break;
            }

            command[0] = '\0';
        }

        if (*command != '\0') {
            add_history(command);
        }

        if (strcmp(command, "exit") == 0) {
            free(command);
            break;
        }

        if (*command != '\0') {
            interpret(command);
        }

        free(command);
    }
}

bool check_file_extension(const char *path) {
    const char *ext = strrchr(path, '.');

    if (ext == NULL) {
        return false;
    }

    return strcmp(ext, ".linux") == 0;
}

static char* read_file(const char* path){
  FILE* file= fopen(path,"rb");

  if (file == NULL) {
      fprintf(stderr, "Could not open file \"%s\"\n", path);
      exit(74);
  }

  fseek(file, 0L, SEEK_END);
  size_t fileSize= ftell(file);
  rewind(file);

  char* buffer=(char*)malloc(fileSize+ 1);

  if(buffer== NULL){
    fprintf(stderr,"Not enough memory to read\"%s\".\n", path);
    exit(74);
  }

  size_t bytesRead= fread(buffer, sizeof(char), fileSize, file);

  if(bytesRead < fileSize){
    fprintf(stderr,"Could not read file\"%s\".\n", path);
    exit(74);
  }

  buffer[bytesRead]='\0';

  fclose(file);
  return buffer;
}

static void run_file(const char *path) {

    if (check_file_extension(path)) {

        char *source = read_file(path);
        InterpretResult result = interpret(source);
        free(source);

        if (result == INTERPRET_COMPILE_ERROR) {
            exit(65);
        }
        if (result == INTERPRET_RUNTIME_ERROR) {
            exit(70);
        }
    }
}

int main(int argc, const char *argv[]) {

    init_vm();

    if (argc == 1) {
        read_eval_print_loop();
    } else if (argc == 2) {
        run_file(argv[1]);
    } else {
        fprintf(stderr, "Uses: freax <path>");
        exit(64);
    }

    free_vm();

    return 0;
}
