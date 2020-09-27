#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

const int MAX = 13;

static void doFib(int n, int doPrint);

/*
 * unix_error - unix-style error routine.
 */
inline static void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

int main(int argc, char **argv)
{
    int arg;
    int print = 1;

    if (argc != 2)
    {
        fprintf(stderr, "Usage: fib <num>\n");
        exit(-1);
    }

    arg = atoi(argv[1]);
    if (arg < 0 || arg > MAX)
    {
        fprintf(stderr, "number must be between 0 and %d\n", MAX);
        exit(-1);
    }

    doFib(arg, print);

    return 0;
}

/* 
 * Recursively compute the specified number. If print is
 * true, print it. Otherwise, provide it to my parent process.
 *
 * NOTE: The solution must be recursive and it must fork
 * a new child for each call. Each process should call
 * doFib() exactly once.
 */
void doFib(int n, int doPrint)
{
    // ans stores nth fibonacci number(starting from 0)
    int ans;
    if (n == 0 || n == 1)
    {
        // base cases
        ans = n;
    }
    else
    {
        // use forks and waits to recursively determine ans
        int child1ans, child2ans;
        pid_t pid = fork();
        if (pid == 0)
        {
            // first child exits with code of n-1th fibonacci number
            doFib(n - 1, 0);
        }
        else
        {
            // parent waits for first child and stores its exit status in child1ans
            wait(&child1ans);
            child1ans = WEXITSTATUS(child1ans);
            pid = fork();
            if (pid == 0)
            {
                // parent creates a second child which exits with code of n-2th fibonacci number
                doFib(n - 2, 0);
            }
            else
            {
                // parent waits for second child and stores its exit status in child2ans
                wait(&child2ans);
                child2ans = WEXITSTATUS(child2ans);
                // parent's ans is child1ans + child2ans
                ans = child1ans + child2ans;
            }
        }
    }

    // print if original parent, otherwise pass ans to parent as exit status
    if (doPrint)
    {
        printf("%d\n", ans);
    }
    else
    {
        exit(ans);
    }
}
