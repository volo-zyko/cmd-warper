#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
#include <io.h>
#include <sys/locking.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
#endif // _WIN32

size_t append(const char** array, const size_t arrayLen, char** result)
{
    size_t i;
    for (i = 0; i < arrayLen; ++i)
    {
        result[i] = strdup(array[i]);
    }

    return arrayLen;
}

const char* EnvVarName = "WARPED_CMD";

int main(const int argc, const char* argv[], char* envp[])
{
    const char* executable = getenv(EnvVarName);

#if defined(_WIN32)

    if (!executable)
    {
        fprintf(stderr, "Error: %s environment variable is not set\n", EnvVarName);
        ExitProcess(1);
    }

    char commandLine[32768] = {'\0'};

    int i;
    strcat(commandLine, "\"");
    strcat(commandLine, executable);
    for (i = 1; i < argc; ++i)
    {
        char* argument = NULL;
        append(&argv[i], 1, &argument);

        strcat(commandLine, "\" \"");
        strcat(commandLine, argument);
        free(argument);
    }
    strcat(commandLine, "\"");

    FILE* log = fopen("cmd-warper.log", "a+b");
    // Always lock at the beginning of the file.
    fseek(log, 0, SEEK_SET);
    do
    {
        if (_locking(fileno(log), _LK_LOCK|_LK_NBLCK, 1) == 0)
        {
            break;
        }
        else if (errno != EDEADLOCK)
        {
            perror("_locking failed");
            ExitProcess(1);
        }
    }
    while (1);
    // Return to the end of file for appending.
    fseek(log, 0, SEEK_END);

    fprintf(log, "@%lld: %s\n", (long long) time(NULL), commandLine);
    fclose(log);

    STARTUPINFO startupInfo;
    PROCESS_INFORMATION processInfo;

    memset(&processInfo, '\0', sizeof processInfo);

    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStderr = GetStdHandle(STD_ERROR_HANDLE);

    memset(&startupInfo, '\0', sizeof startupInfo);
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.hStdInput = hStdin;
    startupInfo.hStdOutput = hStdout;
    startupInfo.hStdError = hStderr;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;

    if (!CreateProcess(executable, commandLine, NULL, NULL, TRUE,
                       NORMAL_PRIORITY_CLASS, NULL, NULL, &startupInfo, &processInfo))
    {
        DWORD errorID = GetLastError();

        char* messageBuffer = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                       NULL, errorID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        fprintf(stderr, "Error (%d): %s", errorID, messageBuffer);

        LocalFree(messageBuffer);
        ExitProcess(1);
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD exitCode = 0;
    GetExitCodeProcess(processInfo.hProcess, &exitCode);

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);

    ExitProcess(exitCode);

#else // _WIN32

    if (!executable)
    {
        fprintf(stderr, "Error: %s environment variable is not set\n", EnvVarName);
        exit(EXIT_FAILURE);
    }

    char** const fullArgs = malloc((argc + 1) * sizeof argv[0]);
    size_t appendIndex = append(&executable, 1, fullArgs);
    appendIndex += append(&argv[1], argc - 1, &fullArgs[appendIndex]);
    fullArgs[appendIndex] = NULL;

    FILE* log = fopen("cmd-warper.log", "a+b");
    do
    {
        if (flock(fileno(log), LOCK_EX|LOCK_NB) == 0)
        {
            break;
        }
        else if (errno != EWOULDBLOCK)
        {
            perror("flock failed");
            exit(EXIT_FAILURE);
        }
    }
    while (1);

    size_t i;
    fprintf(log, "@%lld: ", (long long) time(NULL));
    for (i = 0; i < appendIndex - 1; ++i)
    {
        fprintf(log, "%s ", fullArgs[i]);
    }
    fprintf(log, "%s\n", fullArgs[appendIndex - 1]);
    fclose(log);

    execve(executable, fullArgs, envp);
    perror("execve failed");
    exit(EXIT_FAILURE);

#endif // _WIN32
}
