/* COMP 530: Tar Heel SHell */
/* Author: Morgan J. Howell */

//I, Morgan James Howell, would like to pledge and sign the UNC Honor Code with this assignment which affirms that I did
//not receive any help on this assignment or plagiarise code.

//notes to self
/*
1. test the ins and outs of grep
2. quotation and more precise argument parsing
3. for some reason your debugger prints the command twice
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

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024
// Every command can contain at most 512 embedded environment variables: len($1$2$3....$n) =1024
// 512 = 1024/2 ->
#define MAX_GROUPS 512

//reserved internal commands (priority over external commands if overlap exists)
enum INTERNAL_CMD {
    NIL, EXIT, CD, GOHEELS, SET,
};
static const char *CMD_MAPPING[] = {
    "nil", "exit", "cd", "goheels", "set"
};
static const char REDIRECTS[] = {'|', '>', '<',};

//goheels' ascii art file name
static char *HEEL_ART = "HeelArt.art";

//developer/debug mode ON=1/OFF=0
static int DEBUG_MODE = 0;

//constant pattern matching for valid environment variable sets
static char* SET_ENV_PATTERN = "^([a-zA-Z0-9]+|[!@#\\$?%\\^\\&*\\)\\(+._-])\\=";
static char* EMBEDDED_VAR = "\\$([a-zA-Z0-9]+|[!@#\\$?%\\^\\&*\\)\\(+._-])";
static regex_t setReg;
static regex_t embeddedVar;
regmatch_t embedGroups[MAX_GROUPS];
static int regexCompileStatus;

//wrapper for parsed arguments
struct Payload {
    char **arguments;
    int argumentCount;
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
struct Payload scrapeProcessArguments(char *absoluteFilePath, const char *delimiters, char **token, char* parserState);
int isArgument(char *arg);
int isSpecialRedirect(char *arg);
struct Payload scrapeArguments(char **token, const char *delimiters, char* parserState);
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

int main (int argc, char **argv, char **envp) {
    int finished = 0;
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
    if(regexCompileStatus) {
        printf("fuck");
        exit(3);
    }

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

        // Execute the command, handling built-in commands separately 
        // Just echo the command line for now
        execute(cmd, &nav, &debug);
    }

    return 0;
}

void execute(char *cmd, struct Navigation *nav, struct Debugger *debug) {
    const char *delimiters = " \n\r\t";
    char *tokenizedString = malloc(MAX_INPUT+1);
    strcpy(tokenizedString, cmd);
    char *parserState;
    char *token = strtok_r(tokenizedString, delimiters, &parserState);
    //interpolation calls preprocess the token for potentially embedded environment variables
    token = interpolate(token);
    int isInternal = isInternalCommand(token);
    //maintain debugging info with both successful and failed command attempts
    if(DEBUG_MODE) updateAndLogLaunch(debug, token);

    while(token != NULL) {
        //check to see if the provided command is an internal command
        if(isInternal) {
            token = strtok_r(NULL, delimiters, &parserState);
            token = interpolate(token);
            struct Payload Args = scrapeArguments(&token, delimiters, parserState);
            int responseCode = 0;

            switch(isInternal) {
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
            char *pathAttempts;
            char *absoluteFilePath;
            int responseCode;

            //attempt a search in $PATH environment variable    
            if(!isAbsoluteOrRelativePath(token)) {
                pathAttempts = getenv("PATH");
                char *file = concat("/", token);
                absoluteFilePath = findBinary(pathAttempts, file);
                free(file);
            //search for binary in provided path
            } else {
                pathAttempts = concat(token, ":");
                absoluteFilePath = findBinary(pathAttempts, "");
                free(pathAttempts);
            }

            if(absoluteFilePath != NULL) {
                token = strtok_r(NULL, delimiters, &parserState);
                token = interpolate(token);
                struct Payload Args = scrapeProcessArguments(absoluteFilePath, delimiters, &token, parserState);
                responseCode = spawnProcess(absoluteFilePath, Args);
                free(Args.arguments);
            } else {
                char *noCommand = concat(token, ": command could not be found\n");
                write(1, noCommand, strlen(noCommand));
                free(noCommand);
                token = strtok_r(NULL, delimiters, &parserState);
                token = interpolate(token);
                responseCode = -1;
            }
            checkExitCode(responseCode, debug);
        }
    }
    free(tokenizedString);
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
int spawnProcess(char *path, struct Payload Args)
{
    int status;
    pid_t pid;
    pid = fork();
    //successful spawning of child process
    if (pid == 0) {
        execv(path, Args.arguments);
        _exit (EXIT_FAILURE);
    }
    //forking failed, report the failure
    else if (pid < 0) {
        status = -1;
    }
    //filtering parent process through this branch, parent waits for child to complete
    else {
        if (waitpid (pid, &status, 0) != pid) {
            status = -1;
        }
    }
    
    return status;
}


/****************************************
 *                                      *
 *     PARSER HELPER FUNCTIONS          *
 *                                      *
 ****************************************/

//scrape the arguments that directly follow a particular recognized command
struct Payload scrapeProcessArguments(char *absoluteFilePath, const char *delimiters, char** token, char* parserState) {
    //when passing arguments to execv, we must include file descriptor name and an ending NULL terminator 
    struct Payload Args;
    unsigned long currentArgumentSize = 0;
    int argumentCount = 1;
    char** arguments;
    arguments = (char **) malloc(argumentCount * sizeof(char *));
    *arguments = (char *) malloc(strlen(absoluteFilePath+1));
    arguments[0] = absoluteFilePath;

    while(*token != NULL && isArgument(*token)) {
        argumentCount++;
        arguments = (char **) realloc(arguments, argumentCount * sizeof(char *));
        arguments[argumentCount-1] = (char *) malloc(strlen(*token)+1); 
        arguments[argumentCount-1] = *token;
        *token = strtok_r(NULL, delimiters, &parserState);
        *token = interpolate(*token);
    }

    argumentCount++;
    arguments = (char **) realloc(arguments, argumentCount * sizeof(char *));
    arguments[argumentCount-1] = (char *) malloc(1);
    arguments[argumentCount-1] = NULL;
    Args.arguments = arguments;
    Args.argumentCount = argumentCount;
    return Args;
}


struct Payload scrapeArguments(char **token, const char *delimiters, char* parserState) {
    struct Payload Args;
    char **arguments;
    arguments = NULL;
    int argumentCount = 0;
    
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
        *token = interpolate(*token);
    }

    Args.arguments = arguments;
    Args.argumentCount = argumentCount;
    return Args;
}

char* interpolate(char *token) {
    if(token == NULL || strlen(token) == 0 || hasVar(token) == -1) {
        return token;
    } else {

        char *buffer = malloc(MAX_INPUT);
        char *interpolatedVar = malloc(MAX_INPUT); 
        int tokenTick, bufferTick = 0, varTick = 0;

        for(tokenTick=0; tokenTick<strlen(token); tokenTick++) {

            if(token[tokenTick] == '$') {
                tokenTick++;
                //scrape the environment variable name indicated after the $
                while(tokenTick<strlen(token) && token[tokenTick] != '$') {
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

//return index of the first embedded variabled, denoted by $
int hasVar(char *token) {
    int i;
    for(i=0; i<strlen(token); i++) {
        if(token[i] == '$') {
            return i;
        }
    }
    return -1;
}

//attempt to find a binary executable within the paths provided, returning NULL if file is not found
char* findBinary(char *path, char *fileName) {
    char *paths = malloc(strlen(path)+1);
    strcpy(paths, path);
    char *internalParserState;
    char *shard = strtok_r(paths, ":", &internalParserState);
    struct stat fileStat;

    while(shard != NULL) {
        char *completeFileName = concat(shard, fileName);
        //check first to see if the file exists and executable permissions
        if(isXFile(completeFileName)) {
            return completeFileName;
        }
        free(completeFileName);
        shard = strtok_r(NULL, ":", &internalParserState);
    }
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
    if(Args.argumentCount == 0 || (Args.argumentCount==1 && Args.arguments[0][0] == '~')) {
        responseStatus = chdir(getenv("HOME"));
        memset(nav->lastDirectory, '\0', MAX_INPUT);
        strcpy(nav->lastDirectory, nav->pwd);
    } else {
        char *targetDirectory;
        targetDirectory = Args.arguments[0];
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
//set path variable for return code
void checkExitCode(int code, struct Debugger *debug) {
    debug->exitStatus = code;
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
