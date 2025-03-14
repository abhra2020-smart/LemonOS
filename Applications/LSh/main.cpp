#include <Lemon/System/Spawn.h>
#include <fcntl.h>
#include <limits.h>
#include <list>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <string>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#include <vector>

termios execAttributes; // Set before executing
termios readAttributes; // Set on ReadLine

std::string ln;
int lnPos = 0;

typedef void (*builtin_call_t)(int, char**);

typedef struct {
    const char* name;
    builtin_call_t func;
} builtin_t;

char currentDir[PATH_MAX];

std::list<std::string> path;

std::list<builtin_t> builtins;

std::vector<std::string> history;

void LShBuiltin_Cd(int argc, char** argv) {
    if (argc > 3) {
        printf("cd: Too many arguments");
    } else if (argc == 2) {
        if (chdir(argv[1])) {
            printf("cd: Error changing working directory to %s\n", argv[1]);
        }
    } else
        chdir("/");

    getcwd(currentDir, PATH_MAX);
}

void LShBuiltin_Pwd(int argc, char** argv) {
    getcwd(currentDir, PATH_MAX);
    printf("%s\n", currentDir);
}

void LShBuiltin_Export(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        putenv(argv[i]);
    }
}

void LShBuiltin_Clear(int argc, char** argv) {
    printf("\033c");
    fflush(stdout);
}

void LShBuiltin_Exit(int argc, char** argv) { exit(0); }

builtin_t builtinCd = {.name = "cd", .func = LShBuiltin_Cd};
builtin_t builtinPwd = {.name = "pwd", .func = LShBuiltin_Pwd};
builtin_t builtinExport = {.name = "export", .func = LShBuiltin_Export};
builtin_t builtinClear = {.name = "clear", .func = LShBuiltin_Clear};
builtin_t builtinExit = {.name = "exit", .func = LShBuiltin_Exit};

pid_t job = -1;

void InterruptSignalHandler(int sig){
    if(job > 0){
        // If we have a current job, send signal to child.
        kill(job, sig);
    }
}

void ReadLine() {
    tcsetattr(STDOUT_FILENO, TCSANOW, &readAttributes);

    bool esc = false; // Escape sequence
    bool csi = false; // Control sequence indicator

    lnPos = 0;         // Line cursor position
    int historyIndex = -1; // History index
    ln.clear();

    for (int i = 0;; i++) {
        char ch;
        while ((ch = getchar()) == EOF)
            ;

        if (esc) {
            if (ch == '[') {
                csi = true;
            }
            esc = false;
        } else if (csi) {
            switch (ch) {
            case 'A': // Cursor Up
                if (historyIndex < static_cast<long>(history.size()) - 1) {
                    historyIndex++;

                    ln = history[history.size() - historyIndex - 1];

                    printf("\e[%dD\e[K%s", lnPos, ln.c_str()); // Delete line and print new line

                    lnPos = ln.length();
                }
                break;
            case 'B': // Cursor Down
                if (historyIndex > 0) {
                    historyIndex--;
                    ln = history[history.size() - historyIndex - 1];
                } else {
                    historyIndex = -1;
                    ln.clear();
                }

                printf("\e[%dD\e[K%s", lnPos, ln.c_str()); // Delete line and print new line

                lnPos = ln.length();
                break;
            case 'C': // Cursor Right
                if (lnPos < static_cast<int>(ln.length())) {
                    lnPos++;
                    printf("\e[C");
                }
                break;
            case 'D': // Cursor Left
                lnPos--;
                if (lnPos < 0) {
                    lnPos = 0;
                } else {
                    printf("\e[D");
                }
                break;
            case 'F': // End
                printf("\e[%uC", static_cast<int>(ln.length()) - lnPos);
                lnPos = ln.length();
                break;
            case 'H': // Home
                printf("\e[%dD", lnPos);
                lnPos = 0;
                break;
            }
            csi = false;
        } else if (ch == '\b') {
            if (lnPos > 0) {
                lnPos--;
                ln.erase(lnPos, 1);
                putc(ch, stdout);
            }
        } else if (ch == '\n') {
            putc(ch, stdout);
            break;
        } else if (ch == '\e') {
            esc = true;
            csi = false;
        } else {
            ln.insert(lnPos, 1, ch);
            putc(ch, stdout);
            lnPos++;
        }

        if (lnPos < static_cast<int>(ln.length())) {
            printf("\e[K%s", &ln[lnPos]); // Clear past cursor, print everything in front of the cursor

            for (int i = 0; i < static_cast<int>(ln.length()) - lnPos; i++) {
                printf("\e[D");
            }
        }

        fflush(stdout);
    }
    fflush(stdout);

    tcsetattr(STDOUT_FILENO, TCSANOW, &execAttributes);
}

void ParseLine() {
    if (!ln.length()) {
        return;
    }

    history.push_back(ln);

    int argc = 0;
    std::vector<char*> argv;

    char* lnC = strdup(ln.c_str());

    if (!lnC) {
        return;
    }

    char* tok = strtok(lnC, " \t\n");
    argv.push_back(tok);
    argc++;

    while ((tok = strtok(NULL, " \t\n"))) {
        argv.push_back(tok);
        argc++;
    }

    for (builtin_t builtin : builtins) {
        if (strcmp(builtin.name, argv[0]) == 0) {
            builtin.func(argc, argv.data());
            return;
        }
    }

    errno = 0;

    if (strchr(argv[0], '/')) {
        job = lemon_spawn(argv[0], argc, argv.data(), 1);
        if (job > 0) {
            int status = 0;
            int ret = 0;
            while((ret = waitpid(job, &status, 0)) == 0 || (ret < 0 && errno == EINTR))
                ;

            job = -1;
        } else if (errno == ENOENT) {
            printf("\nNo such file or directory: %s\n", argv[0]);
        } else {
            perror("Error spawning process");
        }

        if (lnC)
            free(lnC);

        return;
    } else
        for (std::string path : path) {
            job = lemon_spawn((path + "/" + argv[0]).c_str(), argc, argv.data(), 1);
            if (job > 0) {
                int status = 0;
                int ret = 0;
                while((ret = waitpid(job, &status, 0)) == 0 || (ret < 0 && errno == EINTR))
                    ;

                job = -1;
                return;
            } else if (errno == ENOENT) {
                continue;
            } else {
                perror("Error spawning process");
                break;
            }
        }

    printf("\nUnknown Command: %s\n", argv[0]);

    if (lnC)
        free(lnC);

    return;
}

int main() {
    printf("Lemon SHell\n");

    if (const char* h = getenv("HOME"); h) {
        chdir(h);
    }

    getcwd(currentDir, PATH_MAX);
    tcgetattr(STDOUT_FILENO, &execAttributes);
    readAttributes = execAttributes;
    readAttributes.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo when reading user input

    struct sigaction action = {
        .sa_handler = InterruptSignalHandler,
        .sa_flags = 0,
    };
    sigemptyset(&action.sa_mask);

    // Send both SIGINT and SIGWINCH to child
    if(sigaction(SIGINT, &action, nullptr)){
        perror("sigaction");
        return 99;
    }

    if(sigaction(SIGWINCH, &action, nullptr)){
        perror("sigaction");
        return 99;
    }

    builtins.push_back(builtinPwd);
    builtins.push_back(builtinCd);
    builtins.push_back(builtinExport);
    builtins.push_back(builtinClear);
    builtins.push_back(builtinExit);

    std::string pathEnv = getenv("PATH");
    std::string temp;
    for (char c : pathEnv) {
        if (c == ':' && temp.length()) {
            path.push_back(temp);
            temp.clear();
        } else {
            temp += c;
        }
    }
    if (temp.length()) {
        path.push_back(temp);
        temp.clear();
    }

    fflush(stdin);

    for (;;) {
        printf("\n\e[33mLemon \e[91m%s\e[m$ ", currentDir);
        fflush(stdout);

        ReadLine();
        ParseLine();
    }
}