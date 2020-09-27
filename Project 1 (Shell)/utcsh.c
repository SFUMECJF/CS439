/*
  utcsh - The UTCS Shell

  Timothy Zhang - tz3723
  Preeth Kanamangala - preeth
*/

/* Read the additional functions from util.h. They may be beneficial to you
in the future */

#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Global variables */
int keepGoing = 1;
/* number of tokens in original command line input*/
int numTokens;
/* delimiters to tokenize one line of command line input*/
char *lineDelims = " \n";
/* delimiters to tokenize a file containing scripts into individual command lines*/
char *scriptDelims = "\n";
/* number of paths in shell_paths */
int numPaths = 1;
/* number of different command lines(number of "&" + 1) */
int numCommands = 0;
/* list of command lines to run */
char **commandLineList;
/* list of Commands to run */
struct Command **commandList;
/* nonzero if any commands are built-in commands. Used to choose to run sequentially or concurrently */
int areBuiltInCommands = 0;

/* The array for holding shell paths. Can be edited by the functions in util.c*/
char shell_paths[MAX_DIRS_IN_PATH][MAX_CHARS_IN_CMDLINE];
static char prompt[] = "utcsh> "; /* Command line prompt */
static char *default_shell_path[2] = {"/bin", NULL};
/* End Global Variables */

/* Convenience struct for describing a command. Modify this struct as you see
 * fit--add extra members to help you write your code. */
struct Command
{
  char *name;
  int numArgs;      /* total number of arguments on command line, including the name*/
  char **args;      /* Argument array for the command line, including the name */
  char *outputFile; /* Redirect target for file (NULL means no redirect) */
};

/* Here are the functions we recommend you implement */

char **tokenize_command_line(char *cmdline);
/* our modified header for parse_command */
void parse_command(int, char **tokens);
void eval(struct Command *cmd);
int try_exec_builtin(struct Command *cmd);
void exec_external_cmd(struct Command *cmd);

/* prints error message and sets keepGoing to 0 */
void printErrorMessage()
{
  keepGoing = 0;
  char error_message[30] = "An error has occurred\n";
  int nbytes_written = write(STDERR_FILENO, error_message, strlen(error_message));
  if ((int)nbytes_written != (int)strlen(error_message))
  {
    exit(2); /* Should almost never happen -- if it does, error is unrecoverable*/
  }
  return;
}

/* run commands in commandList sequentially */
void runSequentially()
{
  int i = 0;
  for (i = 0; i < numCommands; i++)
  {
    eval(commandList[i]);
  }
}

/* run commands in commandList in parallel */
void runConcurrently()
{
  int i = 0;
  for (i = 0; i < numCommands; i++)
  {
    int iAmParent = fork();
    if (!iAmParent)
    {
      eval(commandList[i]);
      exit(0);
    }
  }
  /* wait(NULL) will return -1 when there are no chile processes running */
  while (wait(NULL) > 0)
  ;
}

/* convert inputLine into strings of individual commands */
void splitConcurrentCommands(char* inputLine){
  commandLineList = malloc(sizeof(char *) * MAX_ARGS_IN_CMDLINE + 1);
  char *token = strtok(inputLine, "&");
  int i = 0;
  while (token != NULL)
  {
    if (i >= MAX_ARGS_IN_CMDLINE)
    {
      printErrorMessage();
    }
    commandLineList[i] = token;
    i++;
    token = strtok(NULL, "&");
  }
  numCommands = i;
  commandLineList[i] = '\0';
}


/* processes scripts */
void readScript(char **argv)
{
  FILE *file = fopen(argv[1], "r");
  char *inputLine = NULL;
  size_t inputLength = 0;

  if (file == NULL)
  {
    printErrorMessage();
    exit(1);
  }

  while (getline(&inputLine, &inputLength, file) != -1)
  {
    keepGoing = 1;
    commandList = malloc(sizeof(struct Command *) * MAX_ARGS_IN_CMDLINE + 1);
    numCommands = 0;
    areBuiltInCommands = 0;

    if (inputLength > MAX_CHARS_IN_CMDLINE)
    {
      printErrorMessage();
      continue;
    }

    splitConcurrentCommands(inputLine);
    int i = 0;
    /* Evaluate */
    for (i = 0; i < numCommands; i++)
    {
      char **inputTokens = tokenize_command_line(commandLineList[i]);
      parse_command(i, inputTokens);
    }
    if (areBuiltInCommands)
    {
      runSequentially();
    }
    else
    {
      runConcurrently();
    }
  }
  free(inputLine);
  fclose(file);
  exit(0);
}

/* REPL loop */
void readCommandLine()
{
  while (1)
  {
    keepGoing = 1;
    commandList = malloc(sizeof(struct Command *) * MAX_ARGS_IN_CMDLINE + 1);
    numCommands = 0;
    areBuiltInCommands = 0;
    printf("%s", prompt);

    /* Read */
    char *inputLine = NULL;
    size_t inputLength = 0;
    getline(&inputLine, &inputLength, stdin);

    if (inputLength > MAX_CHARS_IN_CMDLINE)
    {
      printErrorMessage();
      continue;
    }

    splitConcurrentCommands(inputLine);
    free(inputLine);
    int i = 0;
    /* Evaluate */
    for (i = 0; i < numCommands; i++)
    {
      char **inputTokens = tokenize_command_line(commandLineList[i]);
      parse_command(i, inputTokens);
    }
    if (areBuiltInCommands)
    {
      runSequentially();
    }
    else
    {
      runConcurrently();
    }
  }
}

/* starts the shell */
int main(int argc, char **argv)
{
  set_shell_path(default_shell_path);
  numPaths = 1;

  if (argc == 2)
  {
    readScript(argv);
  }
  else if (argc == 1)
  {
    readCommandLine();
  }
  else
  {
    printErrorMessage();
    exit(1);
  }
  return 0;
}

/** Turn a command line into tokens with strtok
 *
 * This function turns a command line into an array of arguments, making it
 * much easier to process. First, you should figure out how many arguments you
 * have, then allocate a char** of sufficient size and fill it using strtok()
 */
char **tokenize_command_line(char *cmdline)
{
  if (!keepGoing)
  {
    return NULL;
  }

  /* put tokens into a char** and return */
  char **tokensarray = malloc(sizeof(char *) * MAX_ARGS_IN_CMDLINE + 1);
  char *token = strtok(cmdline, lineDelims);
  int i = 0;
  while (token != NULL)
  {
    if (i >= MAX_ARGS_IN_CMDLINE)
    {
      printErrorMessage();
    }
    tokensarray[i] = token;
    i++;
    token = strtok(NULL, lineDelims);
  }
  numTokens = i;
  tokensarray[i] = '\0';
  return tokensarray;
}

/** Turn tokens into a command.
 *
 * The `struct Command` represents a command to execute. This is the preferred
 * format for storing information about a command, though you are free to change
 * it. This function takes a sequence of tokens and turns them into a struct
 * Command, and sets commandList[index] to point to that Command
 */
void parse_command(int index, char **tokens)
{
  if (!keepGoing)
  {
    return;
  }

  char *namet;
  int numArgst;
  char **argst;
  char *outputFilet = NULL;
  /* check for redirect */
  int i;
  for (i = 0; i < numTokens; i++)
  {
    if (!keepGoing)
    {
      return;
    }
    if (0 == strcmp(">", tokens[i]))
    {
      if (i == 0)
      {
        printErrorMessage();
      }
      else if (i != numTokens - 2)
      {
        printErrorMessage();
      }
      else if (0 == strcmp(">", tokens[i + 1]))
      {
        printErrorMessage();
      }
      else
      {
        outputFilet = tokens[++i];
        numTokens -= 2;
        tokens[i - 1] = NULL;
      }
    }
  }

  if (!keepGoing)
  {
    return;
  }

  if (numTokens == 0)
  {
    /* no input in command line*/
    namet = NULL;
    numArgst = -1;
    argst = NULL;
  }
  else if (numTokens == 1)
  {
    /* only token in command line is command name*/
    namet = tokens[0];
    numArgst = 1;
    argst = tokens;
  }
  else
  {
    /* more than one token in command line*/
    namet = tokens[0];
    numArgst = numTokens;
    argst = tokens;
  }

  if (namet == NULL || 0 == strcmp("exit", namet) || 0 == strcmp("cd", namet) || 0 == strcmp("path", namet))
  {
    areBuiltInCommands = 1;
  }
  commandList[index] = malloc(sizeof(struct Command));
  commandList[index]->name = namet;
  commandList[index]->numArgs = numArgst;
  commandList[index]->args = malloc(numArgst + 1);
  commandList[index]->args = argst;
  commandList[index]->outputFile = outputFilet;
  return;
}

/* helper methods for built-in commands*/
void processNoInput()
{
  return;
}

/* checks errors and executes exit command */
void processExitCommand(struct Command *cmd)
{
  /* error check: must have no arguments besides name*/
  if (cmd->numArgs > 1)
  {
    printErrorMessage();
  }
  else
  {
    exit(0);
  }
  return;
}

/* checks errors and executes cd command */
void processCDCommand(struct Command *cmd)
{
  /* error check: must have only one argument besides name*/
  if (cmd->numArgs != 2)
  {
    printErrorMessage();
  }
  else
  {
    if (-1 == chdir(cmd->args[1]))
    {
      printErrorMessage();
    }
  }
  return;
}

/* checks errors and executes path command */
void processPathCommand(struct Command *cmd)
{
  if (cmd->numArgs == 1)
  {
    char *null = NULL;
    set_shell_path(&null);
    numPaths = 0;
  }
  else
  {
    set_shell_path(&(cmd->args[1]));
    numPaths = cmd->numArgs - 1;
  }
  return;
}

/** Evaluate a single command
 *
 * Both built-ins and external commands can be passed to this function--it
 * should work out what the correct type is and take the appropriate action.
 */
void eval(struct Command *cmd)
{
  if (!keepGoing)
  {
    return;
  }

  char *name = cmd->name;
  /* built-in commands*/
  if (name == NULL)
  {
    processNoInput();
  }
  else if ((0 == strcmp(name, "exit")) ||
           (0 == strcmp(name, "cd")) ||
           (0 == strcmp(name, "path")))
  {
    try_exec_builtin(cmd);
  }
  else
  {
    /* PROCESS EXTERNAL COMMAANDS*/
    exec_external_cmd(cmd); /* throw error for exe not existing in curr directory*/
  }

  return;
}

/** Execute built-in commands
 *
 * If the command is a built-in command, immediately execute it and return 1
 * If the command is not a built-in command, do nothing and return 0
 */
int try_exec_builtin(struct Command *cmd)
{
  if (!keepGoing)
  {
    return 0;
  }
  char *name = cmd->name;
  if (0 == strcmp(name, "exit"))
  {
    processExitCommand(cmd);
  }
  else if (0 == strcmp(name, "cd"))
  {
    processCDCommand(cmd);
  }
  else if (0 == strcmp(name, "path"))
  {
    processPathCommand(cmd);
  }
  else
  {
    return 0;
  }
  return 1;
}

/** Execute an external command
 *
 * Execute an external command by fork-and-exec. Should also take care of
 * output redirection, if any is requested
 */
void exec_external_cmd(struct Command *cmd)
{
  if (!keepGoing)
  {
    return;
  }
  int iAmParent = fork();
  if (iAmParent)
  {
    wait(NULL);
  }
  else
  {
    /* set output to write to the output file, or stdout. code adapted from kevin's blog post */
    int fd;
    if (cmd->outputFile)
    {
      if ((fd = open(cmd->outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1 || dup2(fd, 1) == -1 || dup2(fd, 2) == -1)
      {
        printErrorMessage();
      }
    }
    if (!keepGoing)
    {
      return;
    }
    /* first try this directory */
    execv(cmd->name, cmd->args);

    /* now try each path in shell_paths if cmd->name is not an absolute path beginning with "/" */
    if (0 != strcmp("/", cmd->name))
    {
      int i;
      for (i = 0; i < numPaths; i++)
      {
        char *pathAndName = strcat(shell_paths[i], "/");
        pathAndName = strcat(pathAndName, cmd->name);
        execv(pathAndName, cmd->args);
      }
    }
    printErrorMessage();
    exit(0);
  }

  return;
}
