#ifndef TOTORO_SH
#define TOTORO_SH

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/types.h>
#include <errno.h>
#include <pwd.h>
#define BUFF_SZ 256

const char *_CD = "cd";
const char *_RIN = "<";
const char *_ROUT = ">";
const char *_RROUT = ">>";
const char *_PIPE = "|";
enum
{
    OK = 0,
    ERROR_FORK,
    ERROR_COMMAND,
    ERROR_WRONG_PARAMETER,
    ERROR_MISS_PARAMETER,
    ERROR_TOO_MANY_PARAMETER,
    ERROR_CD,
    ERROR_EXIT,

    ERROR_IN,
    ERROR_OUT,

    ERROR_PIPE,
    ERROR_PIPE_MISS_PARAMETER
};

#endif