/* COMP 530: Tar Heel SHell */
/* Author: Morgan J. Howell */

//notes to self
/*
1. test the ins and outs of grep
2. quotation and more precise argument parsing


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

// Assume no input line will be longer than 1024 bytes
#define MAX_INPUT 1024

//reserved internal commands (priority over external commands if overlap exists)
enum INTERNAL_CMD {
    NIL, EXIT, CD, GOHEELS,
};
static const char *CMD_MAPPING[] = {
    "nil", "exit", "cd", "goheels", 
};
static const char REDIRECTS[] = {'|', '>', '<',};

//path for ascii art
static char *HEEL_ART = "HeelArt.art";

//wrapper for parsed arguments
struct Payload {
    char **arguments;
    int argumentCount;
};

struct Navigation {
    char pwd[MAX_INPUT];
    char lastDirectory[MAX_INPUT];
};

//function headers
void execute(char *cmd, struct Navigation *nav); 
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

int main (int argc, char **argv, char **envp) {
    int finished = 0;
    //255 is the usual max path length in linux
    //TODO: this changed the cd for some reason..
    char prompt[MAX_INPUT];
    //strncpy(prompt, " thsh> ", strlen(" thsh> "));
    char cmd[MAX_INPUT];
    //info regarding navigation and directory status
    struct Navigation nav;
    getcwd(nav.pwd, MAX_INPUT);
    strcpy(nav.lastDirectory, nav.pwd);

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
        execute(cmd, &nav);
    }

    return 0;
}

//TODO: return status code to parent?
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
            //write(1, targetDirectory, strlen(targetDirectory));
            responseStatus = chdir(targetDirectory);
            memset(nav->lastDirectory, '\0', MAX_INPUT);
            strcpy(nav->lastDirectory, nav->pwd);
        } else {
            char *noDirectory = concat(targetDirectory, ": is not a directory\n");
            write(1, noDirectory, strlen(noDirectory));
        }
    }

    return responseStatus;
}


//set path variable for return code
void checkCode(int code) {
    if(code == -1) {
        exit(EXIT_FAILURE);
    }
}


int buildHeelArt(struct Navigation *nav) {
    if(isFile(HEEL_ART)) {
        //"Tar" flow
        execute("clear", nav);
        char* payload = concat("head -25 ", HEEL_ART);
        execute(payload, nav);
        free(payload);
        sleep(2);
        execute("clear", nav);
        //"Heel" flow
        payload = concat("sed -n 25,48p ", HEEL_ART);
        execute(payload, nav);
        free(payload);
        sleep(2);
        execute("clear", nav);
        //The actual Tar Heel
        payload = concat("sed -n 53,87p ", HEEL_ART);
        execute(payload, nav);
        free(payload);
        sleep(2);
        execute("clear", nav);
        return 0; 
    } else {
        char fileError[80]; 
        sprintf(fileError, "Please place \"%s\" in your cwd or set $HEEL_PATH to its location\n", HEEL_ART);
        write(1, fileError, strlen(fileError));
        return -1;
    }
}

void execute(char *cmd, struct Navigation *nav) {
    const char *delimiters = " \n\r\t";
    char *tokenizedString = malloc(MAX_INPUT+1);
    strcpy(tokenizedString, cmd);
    char *parserState;
    char *token = strtok_r(tokenizedString, delimiters, &parserState);
    int isInternal = isInternalCommand(token);

    while(token != NULL) {
        if(isInternal) {
            token = strtok_r(NULL, delimiters, &parserState);
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
                    responseCode = buildHeelArt(nav);
                    break;
            }
            checkCode(responseCode);
        } else { 
            char *pathAttempts;
            char *absoluteFilePath;

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
                struct Payload Args = scrapeProcessArguments(absoluteFilePath, delimiters, &token, parserState);
                spawnProcess(absoluteFilePath, Args);
                free(Args.arguments);
                break;
            } else {
                char *noCommand = concat(token, ": command could not be found\n");
                write(1, noCommand, strlen(noCommand));
                free(noCommand);
                token = strtok_r(NULL, delimiters, &parserState);
            }
        }
    }
    free(tokenizedString);
}

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
    }

    Args.arguments = arguments;
    Args.argumentCount = argumentCount;
    return Args;
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

//Internal command code mapping
int isInternalCommand(char *token) {
	if(strncmp(token, CMD_MAPPING[EXIT], strlen(token)) == 0) {
		return EXIT;
	} else if(strncmp(token, CMD_MAPPING[CD], strlen(token)) == 0) {
        return CD;
    } else if(strncmp(token, CMD_MAPPING[GOHEELS], strlen(token)) == 0) {
        return GOHEELS;
    } else{
		return 0;
	}
}

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
