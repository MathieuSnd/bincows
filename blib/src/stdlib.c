#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static void (**atexit_funs)(void);
size_t n_atexit_funs = 0;


void exit(int status) {
    for(size_t i = 0; i < n_atexit_funs; i++)
        atexit_funs[i]();
    
    _exit(status);
}

int atexit(void (*func)(void)) {
    atexit_funs = realloc(atexit_funs, (n_atexit_funs + 1) * sizeof(void*));

    if(atexit_funs == NULL)
        return -1;


    atexit_funs[n_atexit_funs++] = func;

    return 0;
}


extern char** __environ;


int putenv(char *string) {
    char* name  = string;

    size_t envs = 0;

    for(char** p = __environ; *p; p++) {
        if(strcmp(name, *p) == 0) {
            *p = string;
            return 0;
        }
        envs++;
    }

    // add a new environment variable
    __environ = realloc(__environ, (envs + 2) * sizeof(char*));

    if(__environ == NULL)
        return -1;

    __environ[envs] = string;
    __environ[envs + 1] = NULL;

    return 0;
}


int setenv(const char *name, const char *value, int overwrite) {
    if(!overwrite) {
        char* env = getenv(name);

        if(env != NULL)
            return -1;
    }

    char* env = malloc(strlen(name) + strlen(value) + 2);

    strcpy(env, name);
    strcat(env, "=");
    strcat(env, value);

    putenv(env);

    return 0;
}


int unsetenv(const char *name) {
    char* env = getenv(name);

    if(env == NULL)
        return -1;

    char* p = env;

    while(*p) {
        *p = *(p + strlen(name) + 1);
        p++;
    }

    return 0;
}


char* getenv(const char *name) {
    for(char** p = __environ; *p; p++) {
        if(strncmp(name, *p, strlen(name)) == 0) {
            return *p;
        }
    }

    return NULL;
}


int system(const char *command) {
    return forkexec((char*[]){"/bin/sh", "-c", command, NULL});
}


