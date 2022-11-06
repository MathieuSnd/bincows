#include <stdint.h>
#include <unistd.h>
#include <bc_extsc.h>
#include <string.h>
#include <stdlib.h>
#include <sprintf.h>


#include "../../kernel/int/syscall_interface.h"




/**
 * @brief create a string list:
 * strings separated by '\0'
 * with double '\0' at the end
 * 
 * @param len number of null-terminated strings in arr
 * @param arr null terminated array of null terminated
 *            strings
 * @param size (output) size of the resulting string 
 *            list buffer
 * @return mallocated string list
 */
static char* create_string_list(char const* const* arr, size_t* size) {

    // first compute the needed buffer size
    // and the number of strings
    size_t len = 0;
    size_t bufsize = 1; // for the last '\0'

    while(arr[len])
        bufsize += strlen(arr[len++]);


    bufsize += len; // for the '\0' between each string


    *size = bufsize;


    // allocate the buffer
    char* list = malloc(bufsize);

    if(!list)
        return NULL;

    // fill the buffer
    char* p = list;
    for(size_t i = 0; i < len; i++) {
        int slen = strlen(arr[i]);
        memcpy(p, arr[i], slen);
        p += slen;
        if(p >= list + bufsize)
            for(;;);
        *p++ = '\0';
    }

    *p++ = '\0';
    return list;
}

// base function for all exec* functions
int exec(const char* file, 
         char const* const argv[], 
         char const* const envp[], 
         int new_process,
        fd_mask_t fd_mask
) {

    // replace argv[0] by file
    int size = 0;
    
    for(unsigned i = 0; argv[i] != NULL; i++)
        size ++;
    

    const char** _argv = malloc(sizeof(char*) * (size + 1));

    if(!_argv) {
        return -1;
    }

    _argv[0] = file;
    for(int i = 1; i < size; i++)
        _argv[i] = argv[i];
    _argv[size] = 0;

    

    size_t args_sz = 0, env_sz = 0;

    char* args = create_string_list(_argv, &args_sz);
    char* env  = create_string_list(envp, &env_sz);

    if(!args || !env) {
        free(_argv);
        return -1;
    }


    struct sc_exec_args sc_args = {
        .args = args,
        .args_sz = args_sz,
        .env = env,
        .env_sz = env_sz,
        .new_process = new_process,
        .fd_mask = fd_mask,
    };

    int r = syscall(SC_EXEC, &sc_args, sizeof(sc_args));

    free(args);
    free(env);

    free(_argv);

    return r;
}




int execvpe(const char* file, 
            char const* const argv[], 
            char const* const envp[]
) {
    if(strcmp(file, argv[0]) != 0) {
        return -1;
    };

    return exec(file, argv, envp, 0, 0);
}


static int try_access(const char* file, int mode) {
    return access(file, mode);
}


static char* search_path(const char* file) {
    if(file[0] == '/' || try_access(file, O_RDONLY) == 0) {
        return strdup(file);
    }
    
    // search for file in path
    // path is a null terminated string
    // path_sz is the size of the path buffer
    // file is a null terminated string

    char* buf = malloc(PATH_MAX);

    char* path = getenv("PATH");

    char* p = strtok(path, ":");

    while(p) {
        if(strlen(p) + strlen(file) + 2 > PATH_MAX)
            continue;

        sprintf(buf, "%s/%s", p, file);

        if(try_access(buf, O_RDONLY) == 0) {
            // found
            return buf;
        }

        p = strtok(NULL, ":");
    }

    free(buf);
    return NULL;
}



int execv (const char* path, char const* const argv[]) {
    char* path_buf = search_path(path);
    
    if(!path_buf) {
        return -1;
    }
    
    int res = exec(path_buf, argv, 
                (char const* const*)__environ, 0, 0);

    free(path_buf);

    return res;
}


int execvp (const char *le, char const* const argv[]) {
    return exec(le, argv, (char const* const*)__environ, 0, 0);
}



int forkexec(char const* const argv[], fd_mask_t mask) {
    char* path_buf = search_path(argv[0]);
    
    if(!path_buf) {
        return -1;
    }
    
    int res = exec(path_buf, argv, (char const* const*)__environ, 1, mask);


    free(path_buf);

    return res;
}
