/* COMP 530: Tar Heel SHell */
/* Author: Morgan J. Howell */

//I, Morgan James Howell, would like to pledge and sign the UNC Honor Code with this assignment which affirms that I did
//not receive any help on this assignment or plagiarise code.

//notes to self
/*
1. test the ins and outs of grep
2. make sure that internal commands can mix and pipe in with regular commands
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include <regex.h>
#include <stdarg.h>
#include <sys/types.h>
#include <fcntl.h>
// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024

//reserved internal commands (priority over external commands if overlap exists)
enum INTERNAL_CMD {
    NIL, EXIT, CD, GOHEELS, SET,
};
static const char *CMD_MAPPING[] = {
    "nil", "exit", "cd", "goheels", "set"
};
enum REDIRECT {
    PIPE=0, IN=1, OUT=2, 
};

static const char REDIRECTS[] = {'|', '<', '>',};

//goheels' ascii art file name
static char *HEEL_ART = "HeelArt.art";

//developer/debug mode ON=1/OFF=0
static int DEBUG_MODE = 0;

//constant pattern matching for valid environment variable sets
static char* SET_ENV_PATTERN = "^([a-zA-Z0-9]+|[!@#\\$?%\\^\\&*\\)\\(+._-])\\=";
static char* EMBEDDED_VAR = "\\$([a-zA-Z0-9]+|[!@#\\$?%\\^\\&*\\)\\(+._-])";
static regex_t setReg;
static regex_t embeddedVar;
static int regexCompileStatus;
static int processCount;

//wrapper for parsed arguments
struct Payload {
    char cmd[MAX_INPUT];
    char *arguments[MAX_INPUT];
    int argumentCount;
    int hasRedirectIn;
    int hasRedirectOut;
    int hasPipeAhead;
    int hasPipeBehind;
    int isInternal;
    char redirectIn[MAX_INPUT];
    char redirectOut[MAX_INPUT];
    //file descriptors for redirected output and inputs
    int redirectInFD;
    int redirectOutFD;
};

//wrapper for live cwd and history feedback
struct Navigation {
    char pwd[MAX_INPUT];
    char lastDirectory[MAX_INPUT];
};

//wrapper for debugging info
struct Debugger {
    char cmd[MAX_INPUT];
    int exitStatus;
    char cmdLaunch[50];
    char cmdEnd[50];
};

//color codes
#define ANSI_COLOR_RED  "\x1b[31m" //process or internal command returns with error exit code
#define ANSI_COLOR_RESET    "\x1b[0m" 
#define ANSI_COLOR_GREEN   "\x1b[32m" //process or internal command kicks off or has sucessful return
#define ANSI_COLOR_YELLOW  "\x1b[33m" //process that has been kicked off, but not terminated
#define ANSI_COLOR_CYAN    "\x1b[36m"

//function headers
void execute(char *cmd, struct Navigation *nav, struct Debugger *debug); 
int isInternalCommand(char *token);
int spawnProcess(char *cmd, struct Payload args);
int isAbsoluteOrRelativePath(char *path);
char* findBinary(char *path, char *fileName);
char* concat(char *s1, char *s2);
int isXFile(char* file);
//struct Payload scrapeProcessArguments(char *absoluteFilePath, const char *delimiters, char **token, char* parserState);
int isArgument(char *arg);
int isSpecialRedirect(char *arg);
//struct Payload scrapeArguments(char **token, const char *delimiters, char* parserState);
int isDirectory(char *path);
int changeDirectory(struct Payload Args, struct Navigation *nav);
int isFile(char *file);
void setDebugger(int argCount, char **args);
void checkExitCode(int code, struct Debugger *debug);
char *getCurrentTime();
int buildHeelArt(struct Navigation *nav, struct Debugger *debug);
void updateAndLogLaunch(struct Debugger *debug, char *token);
int setEnvVar(struct Navigation *nav, struct Debugger *debug, struct Payload *Args);
int isValidEnvSet(char **varSet);
char** parseEnvVar(char *arg);
void reverse(char **var);
char* interpolate(char *token);
int hasVar(char *token);
char* sanitizeRedirectsAndPipes(char *cmd);
int formatExternalCommand(struct Payload **Process, char *token);
struct Payload scrapeArguments(char *absoluteFilePath, const char *delimiters, char** token, char* parserState);
char* scrapeFile(char **token, const char *delimiters, char **parserState);
int filterRedirectsAndPipes(struct Payload **Process, const char *delimiters, char **token, char **parserState);
char* preprocess(char *cmd);
void printProcess(struct Payload Process);
struct Payload** parsePayloads(struct Debugger *debug, char *tokenizedString);
int spawnProcesses(struct Payload ***ProcessContainer, struct Navigation *nav, struct Debugger *debug);
int spawnChild(int in, int out, struct Payload *Proc, struct Navigation *nav, struct Debugger *debug);
int execInternal(struct Payload *Proc, struct Navigation *nav, struct Debugger *debug);
char* cutComments(char *cmd);
int containsComments(char *cmd);

int main (int argc, char **argv, char **envp) {
    int finished = 0;
    processCount = 0;
    char prompt[MAX_INPUT];
    char cmd[MAX_INPUT];

    //hydrate wrapper for nav status/history
    struct Navigation nav;
    getcwd(nav.pwd, MAX_INPUT);
    strcpy(nav.lastDirectory, nav.pwd);

    //check to see if debugging mode has been activated
    struct Debugger debug;
    if(argc>1) {
        setDebugger(argc, argv);
    }

    //precompile regexs for optimized pattern matching (for parsing environment variables)
    regexCompileStatus = regcomp(&setReg, SET_ENV_PATTERN, 1);
    regexCompileStatus = regcomp(&embeddedVar, EMBEDDED_VAR, 1);

    while (!finished) {
        char *cursor;
        char last_char;
        int rv;
        int count;
        
        //setting prompt with current directory prefix
        getcwd(nav.pwd, MAX_INPUT);
        memset(prompt, '\0', strlen(prompt));
        sprintf(prompt, "[%s] thsh> ", nav.pwd);

        rv = write(1, prompt, strlen(prompt));
        if (!rv) { 
            finished = 1;
            break;
        }

        // read and parse the input
        for(rv = 1, count = 0, cursor = cmd, last_char = 1; rv && (++count < (MAX_INPUT-1)) && (last_char != '\n');
                cursor++) { 
            rv = read(0, cursor, 1);
            last_char = *cursor;
        } 
        *cursor = '\0';

        if (!rv) { 
            finished = 1;
            break;
        }

        execute(cmd, &nav, &debug);
    }

    return 0;
}

//spawn and pipe together (where indicated) all processes 
//taking note that the first process will always take input from STDIN=0 and the last
//process will always write to STDOUT=1 unless a redirect is included
int spawnProcesses(struct Payload ***ProcessContainer, struct Navigation *nav, struct Debugger *debug) {
    struct Payload **Processes = *ProcessContainer;
    int fd[2];
    int currentPipe;
    int in = 0;
    int out = 1;
    int exitStatus = 0; 

    for(currentPipe=0; currentPipe<processCount-1;  currentPipe++) {
        //initialize new file descriptors
        pipe(fd);

        exitStatus = spawnChild(in, fd[1], Processes[currentPipe], nav, debug);
        
        //child process kicked off above will write to this FD
        close(fd[1]);

        //the next process will take input that the previous process wrote to
        in = fd[0];
    }

    //the last process will receive output from the previous process and retain STDOUT=1
    struct Payload *LastProc = Processes[currentPipe];
    //printProcess(*LastProc);
    // if(in != 0 ) { //&& !LastProc->hasRedirectIn) {
    //     dup2(in, 0);
    //     close(in);
    // }
    // } else if(LastProc->hasRedirectIn) {
    //     in = open(LastProc->redirectIn, O_RDONLY);
    //     if(in < 0) {
    //         return 1;
    //     } else {
    //         dup2(in, 0);
    //         close(in);
    //     }
    // }
    // if(LastProc->hasRedirectOut) {
    //     out = open(LastProc->redirectOut, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR); 
    //     dup2(out, 1);
    //     close(out);
    // }
    
    if(LastProc->isInternal) {
        exitStatus |= execInternal(LastProc, nav, debug);
    } else {
        exitStatus |= spawnChild(in, out, LastProc, nav, debug);
    } 

    return exitStatus;
}

int spawnChild(int in, int out, struct Payload *Proc, struct Navigation *nav, struct Debugger *debug) {
    int exitStatus = 0;
    pid_t pid;
    pid = fork();

    if(pid < 0) {
        exitStatus |= -1;
    }
    //successful spawning of child process, take into account proper redirects and current pipe statuses
    if (pid == 0) {
        //link the proper redirect files
        if(Proc->hasRedirectIn) {
            in = open(Proc->redirectIn, O_RDONLY);
            //accessing this file failed
            if(in < 0) {
                write(1, "File does not exists", 20);
                return 1;
            } else {
                dup2(in, 0);
                close(in);
            }
        //Just as the standard Bash implementation, redirects have priority over pipes
        } else {
            if (in != 0) {
                dup2(in, 0);
                close(in);
            }
        }

        if(Proc->hasRedirectOut) {
            out = open(Proc->redirectOut, O_WRONLY | O_TRUNC | O_CREAT, S_IRUSR | S_IRGRP | S_IWGRP | S_IWUSR);
            if(out < 0) {
                write(1, "File could not be opened", 24);
                return 1;
            } else {
                dup2(out, 1);
                close(out);
            }
        } else {
            if(out != 1) {
                dup2(out, 1);
                close(out);
            }

        }

        //commands that are internal should bubble up to the parent process
        if(!Proc->isInternal) {
            execv(Proc->cmd, Proc->arguments);
        } else {
            return 0;
        }
        
        _exit (EXIT_FAILURE);
    }
    //forking failed, report the failure
    else if (pid < 0) {
        exitStatus |= -1;

    }
    //filtering parent process through this branch, parent waits for child to complete
    else {

        pid_t childExit = waitpid (pid, &exitStatus, 0); 

        if (childExit != pid) {
            exitStatus |= -1;
        }

        if(Proc->isInternal) {
            exitStatus |= execInternal(Proc, nav, debug);
        }

    }
    return exitStatus;
}

int execInternal(struct Payload *Proc, struct Navigation *nav, struct Debugger *debug) {
    int exitStatus = 0;
    switch(Proc->isInternal) {
        case EXIT:
            exit(0);
            break;
        case CD:
            exitStatus = changeDirectory(*Proc, nav);
            break;
        case GOHEELS:
            exitStatus = buildHeelArt(nav, debug);
            break;
        case SET:
            exitStatus = setEnvVar(nav, debug, Proc);
            break;
    }
    return exitStatus;
}

void flushAndResetMain() {
    fflush(stdin);
    fflush(stdout);
}

void execute(char *cmd, struct Navigation *nav, struct Debugger *debug) {
    //preprocess the command string to interpolate environment variables and format piping/redirection
    cmd = preprocess(cmd);
    
    char *tokenizedString = malloc(MAX_INPUT+1);
    strcpy(tokenizedString, cmd);

    //parse all the provided commands, redirects, and pipes as a collection of Payloads
    struct Payload **Processes = parsePayloads(debug, tokenizedString);
    int num;

    // for(num=0;num<processCount;num++){    
    //     struct Payload *sample = Processes[num];
    //     //write(1, sample->arguments[1], strlen(sample->arguments[1]));
    //     printProcess(*sample);
    // }
    // printf("\n*************************\n");
    int exitCode = spawnProcesses(&Processes, nav, debug);
    //ensure that our standard streams are open and clear
    flushAndResetMain();
    checkExitCode(exitCode, debug);


/*
    int numProcesses = 0;
    struct Payload *Process = Processes[0];
    while(Process != NULL) {
        numProcesses++;
        Process = Processes[numProcesses];
    }

    printf("%d", numProcesses);
    exit(3);

  */  
    
    /*
    while(token != NULL) {
        //check to see if the provided command is an internal command
        if(Process.isInternal) {
            token = strtok_r(NULL, delimiters, &parserState);
            struct Payload Args = scrapeArguments(&token, delimiters, parserState);
            int responseCode = 0;

            switch(Process.isInternal) {
                case EXIT:
                    exit(3);
                    break;
                case CD:
                    responseCode = changeDirectory(Args, nav);
                    break;
                case GOHEELS:
                    responseCode = buildHeelArt(nav, debug);
                    break;
                case SET:
                    responseCode = setEnvVar(nav, debug, &Args);
                    break;
            }
            checkExitCode(responseCode, debug);
        //if the provided command is not internal, we begin our search in the path provided or $PATH
        } else { 

            //kick off process spawning, with respect to piping and redirects
            

            checkExitCode(responseCode, debug);
        }
    }
    free(tokenizedString);*/
}


//detect if argument has dash prefix denoting an argument
int isArgument(char *arg) {
    return strlen(arg) > 0 && !isSpecialRedirect(arg);
}

//return true if this argument contains a redirection character
int isSpecialRedirect(char *arg) {
    int i;
    int j;
    for(i=0;i<strlen(arg);i++) {
        for(j=0;j<sizeof(REDIRECTS); j++) {
            if(arg[i] == REDIRECTS[j]) {
                return 1;
            }
        }
    }
    return 0;
}


//check if file exists and ensure file has executable permissions
int isXFile(char *file) {
    struct stat *fileInfo;
    fileInfo = malloc(sizeof(struct stat));
    int isFile = stat(file, fileInfo);
    int isExecutable = (isFile) == 0 && (fileInfo->st_mode & S_IXUSR);
    free(fileInfo);
    return isExecutable;
} 

//check if file exists, disregarding permissions
int isFile(char *file) {
    struct stat *fileInfo;
    fileInfo = malloc(sizeof(struct stat));
    int isFile = stat(file, fileInfo);
    int fileStatus = isFile == 0;
    free(fileInfo);
    return fileStatus;
}

//check if the given path leads to a valid directory
int isDirectory(char *path) {
    struct stat *directoryInfo;
    directoryInfo = malloc(sizeof(struct stat));
    //char *truncatedPath = truncPath(file);
    int fileStatus = stat(path, directoryInfo);
    //free(truncatedPath);
    int isDirectory = fileStatus==0 && S_ISDIR(directoryInfo->st_mode);
    free(directoryInfo);
    return isDirectory;
} 

//generic concatenation function
char* concat(char *s1, char *s2) {
    char *result = malloc(strlen(s1) + strlen(s2)+1);
    strcpy(result, s1);
    strcat(result, s2);
    return result;
}

//detect whether the supplied path is absolute or relative
int isAbsoluteOrRelativePath(char *path) {
	int i;
	for(i=0; i<strlen(path); i++) {
		if(path[i] == '/') {
			return 1;
		}
	}
	return 0;
}


//fetch current time as string
char *getCurrentTime() {
    time_t rawtime;
    struct tm * timeinfo;
    time ( &rawtime );
    timeinfo = localtime ( &rawtime );
    return asctime(timeinfo);
}

//kick off new child process with the provided payload (arguments)
int spawnProcess(char *path, struct Payload Args) {
    int exitStatus;
    pid_t pid;
    pid = fork();

    if(pid < 0) {
        exitStatus = -1;
    }
    //successful spawning of child process
    if (pid == 0) {
        execv(path, Args.arguments);
        _exit (EXIT_FAILURE);
    }
    //forking failed, report the failure
    else if (pid < 0) {
        exitStatus = -1;
    }
    //filtering parent process through this branch, parent waits for child to complete
    else {
        if (waitpid (pid, &exitStatus, 0) != pid) {
            exitStatus = -1;
        }
    }
    
    return exitStatus;
}


/****************************************
 *                                      *
 *     PARSER HELPER FUNCTIONS          *
 *                                      *
 ****************************************/

/*
struct Payload scrapeArguments(char **token, const char *delimiters, char* parserState) {
    struct Payload Args;
    char **arguments;
    arguments = NULL;
    int argls -all | catumentCount = 0;
    
    while(*token != NULL && isArgument(*token)) {
        argumentCount++;

        if(arguments==NULL) {
            arguments = (char **) malloc(argumentCount * sizeof(char *));
        } else {
            arguments = (char **) realloc(arguments, argumentCount * sizeof(char *));
        }

        arguments[argumentCount-1] = (char *) malloc(strlen(*token)+1);
        arguments[argumentCount-1] = *token;
        *token = strtok_r(NULL, delimiters, &parserState);
    }ls -all | cat

    Args.arguments = arguments;
    Args.argumentCount = argumentCount;
    return Args;
}*/

char* preprocess(char *cmd) {
    char* noComments = cutComments(cmd);
    printf("\n%s\n", noComments);
    exit(0);
    char* interpolated = interpolate(noComments);    
    char* pipeSafe = sanitizeRedirectsAndPipes(interpolated);
    //free buffers that will no longer be used
    
    if(containsComments(cmd) != -1) {
        free(noComments);
    }
    if(isSpecialRedirect(cmd)) {
        free(interpolated);
    }
    return pipeSafe;
}

//return string without comments
char* cutComments(char *cmd) {
    int firstCommentOccurrence = containsComments(cmd);
    if(cmd == NULL || strlen(cmd) == 0 || firstCommentOccurrence == -1) {
        return cmd;
    } else {
        char *buffer = malloc(MAX_INPUT);
        strncpy(buffer, cmd, firstCommentOccurrence);
        return buffer;
    }
}

//return first occurrence of a comment, otherwise return -1
int containsComments(char *cmd) {
    int i; 
    for(i=0; i<strlen(cmd)+1; i++) {
        if(cmd[i] == '#') {
            return i;
        }
    }
    return -1;
}

char* sanitizeRedirectsAndPipes(char *cmd) {
    if(cmd == NULL || strlen(cmd) == 0 || !isSpecialRedirect(cmd)) {
        return cmd;
    } else {
        char *buffer = malloc(MAX_INPUT);
        int i;
        int j;
        int bufferTick = 0;

        //scan backwards to expand and isolate redirects
        for(i=strlen(cmd)-1;i>=0;i--) {
            int redirectDetected = 0;
            for(j=0;j<sizeof(REDIRECTS); j++) {
                if(cmd[i] == REDIRECTS[j]) {
                    redirectDetected = 1;
                    break;
                }
            }

            if(redirectDetected) {
                //add space to create expansion (if necessary)
                if(i+1<strlen(cmd) && cmd[i+1] != ' ' && cmd[i+1] != '\t') {
                    buffer[bufferTick] = ' ';
                    bufferTick++;
                }

                //add redirect component
                buffer[bufferTick] = REDIRECTS[j];
                bufferTick++;

                //isolate redirect component
                while(i-1>=0 && cmd[i-1] >= '0'&& cmd[i-1] <= '9') {
                    buffer[bufferTick] = cmd[i-1];
                    bufferTick++;
                    i--;
                }

                //add space past the isolate for full expansion (if necessary)
                if(i-1>=0 && cmd[i-1] != ' ' && cmd[i-1] != '\t') {
                    buffer[bufferTick] = ' ';
                    bufferTick++;
                }
            } else {
                buffer[bufferTick] = cmd[i];
                bufferTick++;
            }
        }
        reverse(&buffer);
        return buffer;
    }
}


// //scrape the arguments that directly follow a particular recognized command
// struct Payload scrapeArguments(char *absoluteFilePath, const char *delimiters, char** token, char* parserState) {
//     //when passing arguments to execv, we must include file descriptor name and an ending NULL terminator 
//     struct Payload Args;
//     unsigned long currentArgumentSize = 0;
//     int argumentCount = 1;
//     char** arguments;
//     arguments = (char **) malloc(argumentCount * sizeof(char *));
//     *arguments = (char *) malloc(strlen(absoluteFilePath+1));
//     arguments[0] = absoluteFilePath;

//     while(*token != NULL && isArgument(*token)) {
//         argumentCount++;
//         arguments = (char **) realloc(arguments, argumentCount * sizeof(char *));
//         arguments[argumentCount-1] = (char *) malloc(strlen(*token)+1); 
//         arguments[argumentCount-1] = *token;
//         *token = strtok_r(NULL, delimiters, &parserState);
//     }

//     argumentCount++;
//     arguments = (char **) realloc(arguments, argumentCount * sizeof(char *));
//     arguments[argumentCount-1] = (char *) malloc(1);
//     arguments[argumentCount-1] = NULL;
//     Args.arguments = arguments;
//     Args.argumentCount = argumentCount;
//     return Args;
// }

int filterRedirectsAndPipes(struct Payload **Process, const char *delimiters, char **token, char **parserState) {
    char *file;
    switch(*token[strlen(*token)-1]) {
        case '|':
            (*Process)->hasPipeAhead = 1;
            return 1;
            break;
        case '<':
            (*Process)->hasRedirectIn = 1;
            file = scrapeFile(token, delimiters, parserState);
            strcpy((*Process)->redirectIn, file);
            break;

        case '>':
            (*Process)->hasRedirectOut = 1;
            file = scrapeFile(token, delimiters, parserState);
            strcpy((*Process)->redirectOut,file);
            break;
    }
    return 0;
}

char* scrapeFile(char **token, const char *delimiters, char **parserState) {
    *token = strtok_r(NULL, delimiters, parserState);
    char *filePath = malloc(sizeof(*token));
    strcpy(filePath, *token);
    return filePath;
}

//return true if the supplied string starts with a quote
int startsWithQuote(char* token) {
    if(token != NULL) {
        return token[0] == '\'' || token[0] == '\"';
    }
    return 0;
}

//precondition: the string token starts with a quote
void atomsizeWithQuote(char **token, char** parserState, const char *delimiters, struct Payload **Process, int argc) {
    int isInitialized = 0;
    int argSize = 0;
    //remove quote
    (*token)++;

    while(*token != NULL) {
        //this argument space must dynamically resize to the number of tokens within the quote
        if(!isInitialized) {
            (*Process)->arguments[argc-1] = (char *) malloc(strlen(*token)+2); 
            isInitialized = 1;
        } else {
            char *current = realloc((*Process)->arguments[argc-1], 
                            strlen((*Process)->arguments[argc-1]) + strlen(*token)+2);
            (*Process)->arguments[argc-1] = current;
        }

        int tokenTick;
        for(tokenTick=0; tokenTick<strlen(*token); tokenTick++) {
            if((*token)[tokenTick] != '\'' && (*token)[tokenTick] != '\"') {
                (*Process)->arguments[argc-1][argSize] = (*token)[tokenTick];
                argSize++;
            } else {
                *token = strtok_r(NULL, delimiters, parserState);
                return;
            }
        } 
        //add white space after tokens and before ending quote, because these were the original delimiter
        (*Process)->arguments[argc-1][argSize] = ' ';
        argSize++;   
        (*Process)->arguments[argc-1][argSize+1] = '\0';
        *token = strtok_r(NULL, delimiters, parserState);   
    }
    (*Process)->arguments[argc-1][argSize] = '\0';
}

//scrape the arguments that directly follow a particular recognized command
void scrapeProcessArguments(struct Payload **Process, char *absoluteFilePath, const char *delimiters, 
        char** token, char** parserState) {
    //when passing arguments to execv, we must include file descriptor name and an ending NULL terminator 
    //unsigned long currentArgumentSize = 0;
    int argumentCount = 1;
    //char* arguments[MAX_INPUT];
    //arguments = (char **) malloc(argumentCount * sizeof(char *));
    //*arguments = (char *) malloc(strlen(absoluteFilePath+1));
    (*Process)->arguments[0] = (char *) malloc(strlen(absoluteFilePath)+1);
    strcpy((*Process)->arguments[0], absoluteFilePath);



    int hasQuotes = startsWithQuote(*token);
    //scrape arguments that should be associated with the command
    while(*token != NULL && isArgument(*token)) {
            if(!hasQuotes) {
                //accumulating string and disregard quotes
                argumentCount++;
                (*Process)->arguments[argumentCount-1] = (char *) malloc(strlen(*token)+1); 
                strcpy((*Process)->arguments[argumentCount-1], *token);
                *token = strtok_r(NULL, delimiters, parserState);
            //assuming that all entities that start with a quote should be atomic and have an ending quote
            } else {
                argumentCount++;
                atomsizeWithQuote(token, parserState, delimiters, Process, argumentCount);
                hasQuotes = startsWithQuote(*token);
            }
    }   

    //scrape all the redirects associated with this command
    int encounteredPipe = 0;
    while(*token != NULL && !isArgument(*token) && !encounteredPipe) { 
        encounteredPipe = filterRedirectsAndPipes(Process, delimiters, token, parserState);
        *token = strtok_r(NULL, delimiters, parserState);
    }

    argumentCount++;
    (*Process)->arguments[argumentCount-1] = (char *) malloc(sizeof(NULL));
    (*Process)->arguments[argumentCount-1] = NULL;
    (*Process)->argumentCount = argumentCount;
    //TODO: here no args were lost
}

//intialize a null process
void initProcess(struct Payload **process) {
  //(*process)->arguments = (char **) malloc(sizeof(char **));
    //memset((*process)->cmd, '\0', 254);
    (*process)->argumentCount = 0;
    (*process)->hasRedirectIn = 0;
    (*process)->hasRedirectOut= 0;
    (*process)->hasPipeAhead = 0;
    (*process)->hasPipeBehind = 0;
    (*process)->isInternal = 0;
    //(*process)->redirectIn = (char *) malloc(sizeof(char *));
    //(*process)->redirectIn = "";
    //(*process)->redirectOut = (char *) malloc(sizeof(char *));
    //(*process)->redirectOut = "";
    //memset((*process)->redirectIn, '\0', 254);
    //memset((*process)->redirectOut, '\0', 254);
    (*process)->redirectInFD = 0;
    (*process)->redirectOutFD = 0;
}

//given the user's inputted string, parse out all of the contents as a list of separate 
//commands (Payloads) with appropriate arguments, redirects, and upcoming pipes
struct Payload** parsePayloads(struct Debugger *debug, char *tokenizedString) {
    struct Payload **Processes = (struct Payload **) malloc(sizeof(struct Payload **));
    const char *delimiters = " \n\r\t";
    char *parserState;
    char *token = strtok_r(tokenizedString, delimiters, &parserState);
    int numberOfProcesses = 1;
    int exitCode;

    //format each command into a payload and aggregate
    while(token != NULL) {
        //make room for the new process in the container of processes
        Processes[numberOfProcesses-1] = malloc(sizeof(struct Payload));
        struct Payload *Process = Processes[numberOfProcesses-1];
        initProcess(&Process);

        memset(Process->cmd, '\0', strlen(Process->cmd));
        strcpy(Process->cmd, token);
        //maintain debugging info with both successful and failed command attempts
        if(DEBUG_MODE) updateAndLogLaunch(debug, Process->cmd);

        Process->isInternal = isInternalCommand(Process->cmd);
        //if command is not internal, we take the absolute path or search in $PATH to find the binary
        if(!Process->isInternal) {
            exitCode = formatExternalCommand(&Process, Process->cmd);
            if(exitCode) {
                checkExitCode(exitCode, debug);
            }
        }

        token = strtok_r(NULL, delimiters, &parserState);
        scrapeProcessArguments(&Process, Process->cmd, delimiters, &token, &parserState);

        Processes[numberOfProcesses-1] = Process;
        numberOfProcesses++;
        struct Payload **ProcessesCopy = (struct Payload**) realloc(Processes, (numberOfProcesses * sizeof( struct Payload** )));
        if(ProcessesCopy != NULL) {
            Processes = ProcessesCopy;
        }
    }
    processCount = numberOfProcesses-1;
    return Processes;
}


//verify that payload is a valid external command, search and format if necessary
int formatExternalCommand(struct Payload **Process, char *token) {
        char pathAttempts[MAX_INPUT];
        char *absoluteFilePath;
        int responseCode;
    
        //attempt a search in $PATH environment variable    
        if(!isAbsoluteOrRelativePath(token)) {
            strcpy(pathAttempts, getenv("PATH"));
            char file[MAX_INPUT];
            strcpy(file, token);
            absoluteFilePath = findBinary(pathAttempts, file);
        //search for binary in provided path
        } else {
            strcpy(pathAttempts, token);
            strcat(pathAttempts, ":");
            absoluteFilePath = findBinary(pathAttempts, "");
        }

        if(absoluteFilePath != NULL) {
            strcpy((*Process)->cmd, absoluteFilePath);
            return 0;
            //token = strtok_r(NULL, delimiter`s, &parserState);
            //struct Payload Args = scrapeProcessArguments(absoluteFilePath, delimiters, &token, parserState);
            //responseCode = spawnProcess(absoluteFilePath, Args);
            //free(Args.arguments);
        } else {
            char noCommand[MAX_INPUT];
            strcpy(noCommand, token);
            strcat(noCommand, ": command could not be found\n");
            //char *noCommand = concat(token, ": command could not be found\n");
            write(1, noCommand, strlen(noCommand));
            //free(noCommand);
            //token = strtok_r(NULL, delimiters, &parserState);
            return -1;
        }
}
char* interpolate(char *token) {
    if(token == NULL || strlen(token) == 0 || !hasVar(token)) {
        printf("wtf");
        return token;
    } else {
        printf("here");
        char *buffer = malloc(MAX_INPUT);
        char *interpolatedVar = malloc(MAX_INPUT); 
        int tokenTick, bufferTick = 0, varTick = 0;

        for(tokenTick=0; tokenTick<strlen(token); tokenTick++) {

            if(token[tokenTick] == '$') {
                tokenTick++;
                //scrape the environment variable name indicated after the $
                while(tokenTick<strlen(token) && token[tokenTick] != '$' && token[tokenTick] != ' '
                        && token[tokenTick] != '\n' && token[tokenTick] != '\t' && token[tokenTick] != '\r') {
                    interpolatedVar[varTick] = token[tokenTick];
                    tokenTick++;
                    varTick++;
                }
                interpolatedVar[varTick] = '\0';
                //attempt to find this variable in the local environment and splice into the original token
                char* envVar = getenv(interpolatedVar);
                //check to see if this variable actually exists
                if(envVar != NULL) {
                    strcat(buffer, envVar);
                    bufferTick += strlen(envVar);
                }
                memset(interpolatedVar, '\0', strlen(interpolatedVar));
                varTick = 0;
                
                //we must account for the case of N embedded environment vars within the same string
                if(tokenTick>=strlen(token)) {
                    break;
                } else {
                    tokenTick--;
                    continue;
                }

            } else {
                buffer[bufferTick] = token[tokenTick];
                bufferTick++;
            } 
        }     
        free(interpolatedVar);
        return buffer;
    }
}
/*
//print arbitrary strings and variables
void print(int num, ...) {
    char buffer[MAX_INPUT];
    va_list valist;
    va_start(valist, num);
    int i;
    for(i=0;i<num;i++) {
        if(i == 0) {
            strcpy(buffer, va_arg(valist, char *))
        } else {
            strcat(buffer, va_arg(valist, char *));
        }
    }

    memset(prntStr, '\0', strlen(prntStr));
    snprintf(prntStr, sizeof(prntStr), "buffer: %s  | buffertick: %d", buffer, bufferTick);
    write(1, prntStr, strlen(prntStr));
}*/

//return true if the token contains a variable
int hasVar(char *token) {
    int i;
    for(i=0; i<strlen(token); i++) {
        if(token[i] == '$') {
            return 1;
        }
    }
    return 0;
}

//attempt to find a binary executable within the paths provided, returning NULL if file is not found
char* findBinary(char *path, char *fileName) {
    char *paths = malloc(strlen(path)+1);
    strcpy(paths, path);
    char *internalParserState;
    char *shard = strtok_r(paths, ":", &internalParserState);
    struct stat fileStat;

    while(shard != NULL) {
        char *completeFileName = malloc(strlen(shard) + strlen(fileName) + 1);
        strcpy(completeFileName, shard);
        strcat(completeFileName, "/");
        strcat(completeFileName, fileName);
        //check first to see if the file exists and we have executable permissions
        if(isXFile(completeFileName)) {
            free(paths);
            return completeFileName;
        }
        free(completeFileName);
        shard = strtok_r(NULL, ":", &internalParserState);
    }
    free(paths);
    //binary could not be found
    return NULL;
}



/****************************************
 *                                      *
 *           INTERNAL COMMANDS          *
 *                                      *
 ****************************************/

//executes display commands to show the "Tar Heel Ascii Reel"
int buildHeelArt(struct Navigation *nav, struct Debugger *debug) {
    if(isFile(HEEL_ART)) {
        //"Tar" flow
        execute("clear", nav, debug);
        char* payload = concat("head -25 ", HEEL_ART);
        execute(payload, nav, debug);
        free(payload);
        sleep(2);
        execute("clear", nav, debug);
        //"Heel" flow
        payload = concat("sed -n 25,48p ", HEEL_ART);
        execute(payload, nav, debug);
        free(payload);
        sleep(2);
        execute("clear", nav, debug);
        //The actual Tar Heel
        payload = concat("sed -n 53,87p ", HEEL_ART);
        execute(payload, nav, debug);
        free(payload);
        sleep(2);
        execute("clear", nav, debug);
        return 0; 
    } else {
        char fileError[80]; 
        sprintf(fileError, "Please place \"%s\" in your cwd or set $HEEL_PATH to its location\n", HEEL_ART);
        write(1, fileError, strlen(fileError));
        return -1;
    }
}

//TODO: printing twice in debug after first CD call
//functionality for changing the working directory
int changeDirectory(struct Payload Args, struct Navigation *nav) {
    int responseStatus = 0;

    //if no arguments are specified with cd, we assume a change to the home directory
    if(Args.argumentCount == 2 || (Args.argumentCount==3 && Args.arguments[1][0] == '~')) {
        responseStatus = chdir(getenv("HOME"));
        memset(nav->lastDirectory, '\0', MAX_INPUT);
        strcpy(nav->lastDirectory, nav->pwd);
    } else {
        char *targetDirectory;
        targetDirectory = Args.arguments[1];
        if(strlen(targetDirectory)==1 && targetDirectory[0]=='-') {
            responseStatus = chdir(nav->lastDirectory);
            memset(nav->lastDirectory, '\0', MAX_INPUT);
            strcpy(nav->lastDirectory, nav->pwd);
        } else if(isDirectory(targetDirectory)) {
            responseStatus = chdir(targetDirectory);
            memset(nav->lastDirectory, '\0', MAX_INPUT);
            strcpy(nav->lastDirectory, nav->pwd);
        } else {
            char *noDirectory = concat(targetDirectory, ": is not a directory\n");
            write(1, noDirectory, strlen(noDirectory));
            responseStatus = 1;
        }
    }

    return responseStatus;
}

//set environment variable
int setEnvVar(struct Navigation *nav, struct Debugger *debug, struct Payload *Args) {
    int responseStatus = -1;

    if(!regexCompileStatus && Args->argumentCount == 1 && isValidEnvSet(&(Args->arguments[0]))) {
            char **valVar = parseEnvVar(Args->arguments[0]);
            //by default thsh will overwrite existing environment variables
            responseStatus = setenv(valVar[0], valVar[1], 1);
            free(valVar[0]);
            free(valVar[1]);
            free(valVar);
    }
    return responseStatus;
}

//test to see if the env var assignment is valid
int isValidEnvSet(char **varSet) {
    //if someone attempts to set $VAR=VAL, we will ignore the '$' and consider this a valid assignment
    if((*varSet)[0] == '$') {
        //memmove(*varSet, *varSet+1, strlen(*varSet));
        (*varSet)++;
    }
    int isMatch;
    isMatch = regexec(&setReg, *varSet, 0, NULL, 0);
    return !isMatch;
}

//parse out the variable and value of a given environmet variable assignment
char** parseEnvVar(char *arg) {
    //value buffer
    char* val;
    val = malloc(strlen(arg)+1);
    //var name buffer
    char* var;
    var = malloc(strlen(arg)+1);
    //container for both the var and val
    char **valVar = malloc(2*sizeof(char *)); 

    //parsing var and val
    int i = 0;
    int j = strlen(arg)-1;
    while(i<j) {
        if(arg[i]!='=') {
            val[i] = arg[i];
            i++;
        }
        if(arg[j]!='=') {
            var[strlen(arg)-1-j] = arg[j];
            j--;
        }
    }
    val[i] = '\0';
    var[strlen(arg)-1-j] = '\0';

    valVar[0] = val;
    reverse(&var);
    valVar[1] = var;
    return valVar;
}

//reverse given string
void reverse(char **var) {
    int i;
    for(i=strlen(*var)-1; i>=strlen(*var)/2; i--) {
        char tmp = (*var)[i];
        (*var)[i] = (*var)[strlen(*var)-1-i];
        (*var)[strlen(*var)-1-i] = tmp;
    }
}

//Internal command code mapping
int isInternalCommand(char *token) {
	if(strncmp(token, CMD_MAPPING[EXIT], strlen(token)) == 0) {
		return EXIT;
	} else if(strncmp(token, CMD_MAPPING[CD], strlen(token)) == 0) {
        return CD;
    } else if(strncmp(token, CMD_MAPPING[GOHEELS], strlen(token)) == 0) {
        return GOHEELS;
    } else if(strncmp(token, CMD_MAPPING[SET], strlen(token)) == 0)  {
        return SET;
    } else{
		return 0;
	}
}

/****************************************
 *                                      *
 *           DEBUGGING                  *
 *                                      *
 ****************************************/
void flush(char **buffer) {
    memset(*buffer, '\0', strlen(*buffer));
}

//update proper environment variables with most recent return code and show debugging console if enabled
void checkExitCode(int code, struct Debugger *debug) {
    debug->exitStatus = code;
    char exitStatus[10];
    snprintf(exitStatus, sizeof(exitStatus),"%d", code);
    setenv("?", exitStatus, 1);
    memset(debug->cmdEnd, '\0', 50);
    strcpy(debug->cmdEnd, getCurrentTime());
    //print the current debugger status if debugging mode is enabled
    if(DEBUG_MODE) {
        char debuggerConsole[MAX_INPUT];
        char buffer[MAX_INPUT];
        char *border = "\n--------------------------------\n";
        //information relevant to the command that was executed
        memset(buffer, '\0', strlen(buffer));
        snprintf(buffer, sizeof(buffer),"\n%s%s%s\n", ANSI_COLOR_CYAN, border, ANSI_COLOR_RESET);
        strcat(debuggerConsole, buffer);
        memset(buffer, '\0', strlen(buffer));
        snprintf(buffer, sizeof(buffer),"%sRAN: %s\n%s", ANSI_COLOR_GREEN, debug->cmd, ANSI_COLOR_RESET);
        strcat(debuggerConsole, buffer);
        memset(buffer, '\0', strlen(buffer));
        snprintf(buffer, sizeof(buffer), "executed at: %s \n", debug->cmdLaunch);
        strcat(debuggerConsole, buffer);

        //information relevant to the exit status
        char *ended = "%sENDED: \"%s\" (ret=%d)\n%s";
        memset(buffer, '\0', strlen(buffer));
        if(debug->exitStatus == 0) {
            snprintf(buffer, sizeof(buffer), ended, ANSI_COLOR_GREEN, debug->cmd, 
                debug->exitStatus, ANSI_COLOR_RESET);
        } else {
            snprintf(buffer, sizeof(buffer), ended, ANSI_COLOR_RED, debug->cmd, 
                debug->exitStatus, ANSI_COLOR_RESET);
        }
        strcat(debuggerConsole, buffer);
        memset(buffer, '\0', strlen(buffer));
        snprintf(buffer, sizeof(buffer), "process ended at: %s \n", debug->cmdEnd);
        strcat(debuggerConsole, buffer);
        memset(buffer, '\0', strlen(buffer));
        snprintf(buffer, sizeof(buffer),"%s%s%s\n\n", ANSI_COLOR_CYAN, border, ANSI_COLOR_RESET);
        strcat(debuggerConsole, buffer);
        write(2, debuggerConsole, strlen(debuggerConsole));
    }
}


void printProcess(struct Payload Process) {
        printf("\n-------------------\n");
        printf("process name %s\n", Process.cmd);
        //printf("process arguments %s\n", Process.arguments[1]);
        printf("process arguments: \n");
        int i;
        for(i=0;i <Process.argumentCount; i++) {
            printf("argument%d: %s\n", i, Process.arguments[i]);
        }

        if(Process.hasRedirectIn) {
            printf("input file %s\n", Process.redirectIn);
        }


        if(Process.hasRedirectOut) {
            printf("output file %s\n", Process.redirectOut);
        }
        printf("pipe ahead? %d\n", Process.hasPipeAhead);
        printf("\n-------------------\n");
}


//scrape given arguments for the developer option
void setDebugger(int argCount, char **args) {
    char *optionTarget = "-d";
    int i;
    for(i=1;i<argCount;i++) {
        if(strlen(args[i]) == 2 && strncmp(args[i], optionTarget, strlen(optionTarget))==0) {
            DEBUG_MODE = 1;
        }
    }
}

void updateAndLogLaunch(struct Debugger *debug, char *token) {
    memset(debug->cmd, '\0', MAX_INPUT);
    strcpy(debug->cmd, token);
    memset(debug->cmdLaunch, '\0', 50);
    strcpy(debug->cmdLaunch, getCurrentTime());
    char launchBuffer[MAX_INPUT];
    memset(launchBuffer, '\0', strlen(launchBuffer));
    snprintf(launchBuffer, sizeof(launchBuffer),"%sRUNNING: %s\n%s", ANSI_COLOR_YELLOW, debug->cmd, ANSI_COLOR_RESET); 
    write(2, launchBuffer, strlen(launchBuffer));
}



/****************************************
 *                                      *
 *           TESTER                     *
 *                                      *
 ****************************************/



//test all the ways to interpolate vars: $var, ---$var, ---$var---, asdas$var$anothervar$another
