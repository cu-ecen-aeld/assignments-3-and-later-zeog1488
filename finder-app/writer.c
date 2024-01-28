#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    // Open the syslog with LOG_USER facility
    openlog("writer.c", 0, LOG_USER);

    // Make sure 2 args are provided
    if (argc != 3)
    {
        syslog(LOG_ERR, "Invalid number of arguments: 2 args needed but %d args specified.", argc - 1);
        return 1;
    }

    // Assign args to vars for readability
    const char *writeFile = argv[1];
    const char *writeStr = argv[2];

    // Log info message
    syslog(LOG_DEBUG, "Writing %s to %s.", writeStr, writeFile);

    // Attempt to open/create the file with correct permissions
    int fd = open(writeFile, O_WRONLY | O_CREAT, 0644);
    // Log and quit if error occurs
    if (fd == -1)
    {
        syslog(LOG_ERR, "Error opening file.");
        return 1;
    }

    // Attempt to write to the file
    ssize_t numWritten = write(fd, writeStr, strlen(writeStr));
    // If write error occurs, log and quit
    if (numWritten == -1)
    {
        syslog(LOG_ERR, "Error writing to file.");
        close(fd);
        closelog();
        return 1;
    }
    else if (numWritten != strlen(writeStr))
    {
        syslog(LOG_ERR, "Error writing to file: partial write occurred.");
        close(fd);
        closelog();
        return 1;
    }
    // Ensure new file data is written to disk
    if (fdatasync(fd) == -1)
    {
        syslog(LOG_ERR, "fdatasync error.");
        close(fd);
        closelog();
        return 1;
    }

    // Close file and syslog
    close(fd);
    closelog();
    return 0;
}