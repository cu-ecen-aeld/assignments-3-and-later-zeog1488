#include "systemcalls.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>

/**
 * @param cmd the command to execute with system()
 * @return true if the command in @param cmd was executed
 *   successfully using the system() call, false if an error occurred,
 *   either in invocation of the system() call, or if a non-zero return
 *   value was returned by the command issued in @param cmd.
 */
bool do_system(const char *cmd)
{
    int ret = system(cmd);

    if (ret != 0)
    {
        return false;
    }

    return true;
}

/**
 * @param count -The numbers of variables passed to the function. The variables are command to execute.
 *   followed by arguments to pass to the command
 *   Since exec() does not perform path expansion, the command to execute needs
 *   to be an absolute path.
 * @param ... - A list of 1 or more arguments after the @param count argument.
 *   The first is always the full path to the command to execute with execv()
 *   The remaining arguments are a list of arguments to pass to the command in execv()
 * @return true if the command @param ... with arguments @param arguments were executed successfully
 *   using the execv() call, false if an error occurred, either in invocation of the
 *   fork, waitpid, or execv() command, or if a non-zero return value was returned
 *   by the command issued in @param arguments with the specified arguments.
 */

bool do_exec(int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int status;
    pid_t pid;
    fflush(stdout);
    // Create a new process
    pid = fork();
    if (pid == -1)
    {
        // Print error if fork fails
        perror("fork");
        return false;
    }
    else if (pid == 0)
    {
        // This is the child process
        // Execute the command with args
        execv(command[0], command);
        // If we get here then command wasn't provided with absolute filepath
        perror("execv");
        exit(EXIT_FAILURE);
    }

    // Wait for child process to terminate
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid");
        return false;
    }
    else if (WIFEXITED(status))
    {
        // If the process exited normally
        if (WEXITSTATUS(status) != 0)
        {
            // If the child process had a non-zero exit status
            return false;
        }
    }
    else
    {
        // If the process exited abnormally
        return false;
    }

    va_end(args);

    return true;
}

/**
 * @param outputfile - The full path to the file to write with command output.
 *   This file will be closed at completion of the function call.
 * All other parameters, see do_exec above
 */
bool do_exec_redirect(const char *outputfile, int count, ...)
{
    va_list args;
    va_start(args, count);
    char *command[count + 1];
    int i;
    for (i = 0; i < count; i++)
    {
        command[i] = va_arg(args, char *);
    }
    command[count] = NULL;

    int status;
    pid_t pid;
    // Create the output redirection file
    int fd = open(outputfile, O_WRONLY | O_TRUNC | O_CREAT, 0644);
    if (fd < 0)
    {
        perror("open");
        return false;
    }
    fflush(stdout);
    // create a new child process
    pid = fork();
    if (pid == -1)
    {
        perror("fork");
        return false;
    }
    else if (pid == 0)
    {
        // This is the child process
        // Duplicate the file descriptor
        if (dup2(fd, 1) < 0)
        {
            perror("dup2");
            exit(EXIT_FAILURE);
        }
        // Close the old descriptor
        fdatasync(fd);
        close(fd);
        // Execute the command and args
        execv(command[0], command);
        // If we get here the improper command and path provided
        perror("execv");
        exit(EXIT_FAILURE);
    }
    // Close the old file descriptor
    fdatasync(fd);
    close(fd);
    // Wait for the child process to terminate
    if (waitpid(pid, &status, 0) == -1)
    {
        perror("waitpid");
        return false;
    }
    else if (WIFEXITED(status))
    {
        // If process terminated normally
        if (WEXITSTATUS(status) != 0)
        {
            // If process terminated with non-zero exit status
            return false;
        }
    }
    else
    {
        // If process terminated abnormally
        return false;
    }

    va_end(args);

    return true;
}
