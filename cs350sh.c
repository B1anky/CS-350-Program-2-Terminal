//Name: Brett Sackstein
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>

#define MAX_LINE_LEN 10000

FILE *fp;

char line[MAX_LINE_LEN];
static int backgroundProcCnt = 0;
static int totalBGProcs = 0;
static int commandCount = 0;

typedef struct{
	int argCnt;
	bool foreground;
	bool pipe;
	bool redirIn;
	bool redirOut;
	char ** args;
}Command;

void deleteCharIndex(char **array, int index);
void deleteIntIndex(int *array, int index);
void removePID(char ** procExecuted, int *procNum, int pidToRemove);
bool input(char **procExecuted, int *procNum, Command **commands);
bool isEmpty(char *lineptr);
bool tokenize(char *lineptr, char **procExecuted, int *procNum, Command **commands);
void handler(/*int signal*/);
void freeMem(char **commands, int tokenCount);
void pipeRedirect(char **commands);
void execute(Command *command, char **procExecuted, int *procNum, bool foreground);
void loopPipe(Command **commands, int commandCount, char **procExecuted, int *procNum);

Command *Command_init(int argc, char **argv, bool foregroundIn, bool redirInputIn, bool redirOutputIn, bool pipeIn) {
	Command *newCommand = malloc(sizeof(Command));
	newCommand->argCnt = argc;
	newCommand->foreground = foregroundIn;
	newCommand->args = malloc((argc + 1)*sizeof(char*));
	newCommand->pipe = pipeIn;
	newCommand->redirIn = redirInputIn;
	newCommand->redirOut = redirOutputIn;

	for (int i = 0; i < argc; i++) {
		newCommand->args[i] = strdup(argv[i]);
	}
	newCommand->args[argc] = NULL;

	return newCommand;
}

void Command_free(Command *command) {
	for(int i = 0; i < command->argCnt; i++){
		free(command->args[i]);
	}
	free(command);
}

void Command_print(Command *command){
	for(int i = 0; i < command->argCnt; i++){
		printf("%s, ", command->args[i]);
	}
	printf("NULL");
	if(command->foreground){
		printf(" in the foreground.\n");
	}else{
		printf(" in the background.\n");
	}

	if(command->pipe){
		printf("This command pipes into the next.\n");
	}

	if(command->redirIn){
		printf("This command reads from file.\n");
	}

	if(command->redirOut){
		printf("This command outputs to file.\n");
	}
}

void printCommands(Command ** commandList, int commandCount){
	for(int i = 0; i < commandCount; i++){
		Command_print(commandList[i]);
	}
}

void handler(/*int signal*/){
	wait(NULL);
}

bool isEmpty(char * lineptr){
	unsigned int i;
	bool isEmpty = true;
	for(i = 0; i < strlen(lineptr) - 1; i++){
		if(lineptr[i] != ' '){
			isEmpty = false;
		}
	}
	return isEmpty;
}

bool tokenize (char * lineptr, char **procExecuted, int *procNum, Command **commandList){
	int tokenCount = 0;
	commandCount = 0;
	bool foreground = true;
	//Error if only white space and no command
	if(isEmpty(lineptr)){
		return(true);
	}
	//process total count
	char **tokens = (char**)calloc(strlen(lineptr), sizeof(char*)); //Fixes token size constraint
	int size = strlen(lineptr);
	char* temp = (char*) malloc(size);
	while (sscanf(lineptr, "%s", temp) != EOF){
		tokens[tokenCount] = temp;
		while(*lineptr == ' ' && *lineptr){ //Fixes whitespace error
			lineptr++;
		}
		lineptr += strlen(tokens[tokenCount]);
		tokenCount++;
		size = strlen(lineptr);
		temp = (char*) realloc(0, size);
	}
	free(temp);

	const char * bg = "&";
	if(strncmp(tokens[tokenCount - 1], bg, 1) == 0){
		foreground = false;
		free(tokens[tokenCount - 1]);
		tokenCount--;
	}
	char *tempList[tokenCount];

	int listIndex = 0;
	int tempIndex = 0;
	const char * pipe = "|";
	const char * redirIn = "<";
	const char * redirOut = ">";
	bool redirInputBool = false;
	bool redirOutputBool = false;
	bool pipeBool = false;

	for(int i = 0; i < tokenCount; i++){
		redirInputBool = false;
		redirOutputBool = false;
		pipeBool = false;

		if(strncmp(tokens[i], pipe, 1) == 0){
			pipeBool = true;
			commandList[listIndex++] = Command_init(tempIndex, tempList, foreground, redirInputBool, redirOutputBool, pipeBool);
			tempIndex = 0;	
			commandCount++;
		}else{
			pipeBool = false;
		}

		if(strncmp(tokens[i], redirIn, 1) == 0){
			redirInputBool = true;
			i++;
			tempList[tempIndex++] = tokens[i];
		}else{
			redirInputBool = false;
		}

		if(strncmp(tokens[i], redirOut, 1) == 0){
			redirOutputBool = true;
			i++;
			tempList[tempIndex++] = tokens[i];
		}else{
			redirOutputBool = false;
		}

		if(!(pipeBool || redirInputBool || redirOutputBool)){
			tempList[tempIndex++] = tokens[i];
		}
	}

	commandList[listIndex] = Command_init(tempIndex, tempList, foreground, redirInputBool, redirOutputBool, pipeBool);
	commandCount++;
	//printCommands(commandList, commandCount);
	freeMem(tokens, tokenCount);
	if(strcmp(commandList[0]->args[0], "exit")){
		loopPipe(commandList, commandCount, procExecuted, procNum);
	}else{
		return(false);	
	}
	return(true);
}

//For multiple commands
void loopPipe(Command **commands, int commandCount, char **procExecuted, int *procNum){
	bool fgRecentPid = false;
	bool fgSpecificPid = false;
	bool foreground = commands[0]->foreground;
	int pipefds[2];
	int status = 0;
	pid_t pid;
	int fd_in = 0;
	int fileDesc = 0;

	if(!strcmp(*commands[0]->args,"fg")){
		if(commands[0]->argCnt == 1){ //they want to bring the most recent pid to fg
			//Bring most recently bg process to fg
			fgRecentPid = true;
		}else if(commands[0]->argCnt == 2){
			//bring specific bg process to fg if possible
			fgSpecificPid = true;
		}else{
			//error
			printf("Error. Syntax: fg [PID...]\n");
		}
		//skips over the exec calls if fg is called, since handled differently
		goto fgResume;
	}

	if(!strcmp(*commands[0]->args,"listjobs")){
		printf("List of %d backgrounded processes:\n", backgroundProcCnt);
		for(int j = 0; j < backgroundProcCnt; j++){
			if(kill(procNum[j], 0) == -1){
				printf("%s with PID %d Status:FINISHED\n", procExecuted[j], procNum[j]);
				//removePID(procExecuted, procNum, procNum[j]);
			}else{
				printf("%s with PID %d Status:RUNNING\n", procExecuted[j], procNum[j]);
			}
		}
		return;
	}

	for(int i = 0; i < commandCount; i++){
		pipe(pipefds);
		const char* path;

		if(commands[i]->redirOut){
			path = commands[i]->args[commands[i]->argCnt - 1];
			fileDesc = open(path, O_WRONLY | O_CREAT | O_TRUNC);
			if(fileDesc < 0){
				printf("File couldn't be opened, exiting now.\n");
				exit(1);
			}else{
				commands[i]->args[commands[i]->argCnt - 1] = NULL;
				commands[i]->argCnt--;
			}
		}

		if(commands[i]->redirIn){
			path = commands[i]->args[commands[i]->argCnt - 1];
			fileDesc = open(path, O_RDONLY);
			if(fileDesc < 0){
				printf("File couldn't be opened, exiting now.\n");
				exit(EXIT_FAILURE);
			}else{
				commands[i]->args[commands[i]->argCnt - 1] = NULL;
				commands[i]->argCnt--;
			}
		}

		if((pid = fork()) == -1){
			exit(EXIT_FAILURE);
	    	}else if(pid == 0 && !(fgRecentPid || fgSpecificPid)){ 
			if(dup2(fd_in, 0) < 0){  //change the input according to the old one 
				exit(EXIT_FAILURE);	
			}

			if(commands[i]->args != NULL && i != commandCount - 1 && !commands[i]->redirOut && !commands[i]->redirIn){
				if(dup2(pipefds[1], 1) < 0){//feed exec output into the next command
					exit(EXIT_FAILURE);	
				} 
				close(pipefds[0]);
			}

			if(commands[i]->args != NULL && commands[i]->redirOut){
				if(dup2(fileDesc, 1) < 0){//Write to file from regular output 
					exit(EXIT_FAILURE);
				} 
				close(pipefds[0]);
			}
			
			if(commands[i]->args != NULL && commands[i]->redirIn){ //Read from file as intput to command
				if(dup2(fileDesc, 0) < 0){ //Read from file as input to exec
					exit(EXIT_FAILURE);
				} 
				if(commandCount > 1){ //feed exec output into the next command
					if(dup2(pipefds[1], 1) < 0){
						exit(EXIT_FAILURE);
					}
				}
			}

			execvp(commands[i]->args[0], commands[i]->args);
			exit(EXIT_FAILURE);		
		}else if(pid > 0){ 
			if(!foreground){
				signal(SIGCHLD, handler);
				procExecuted[backgroundProcCnt] = (char*)realloc(0, strlen(commands[i]->args[0]) + 1);
				strcpy(procExecuted[backgroundProcCnt], commands[i]->args[0]);
				procNum[backgroundProcCnt] = pid;
				backgroundProcCnt++;
				totalBGProcs++;
			}

			close(pipefds[1]);
			fd_in = pipefds[0]; //save the input for the next command
			commands[i]->args++;

			fgResume:
			if(fgRecentPid){
				waitpid(procNum[backgroundProcCnt - 1], &status, 0);
				removePID(procExecuted, procNum, procNum[backgroundProcCnt - 1]);
			}

			if(fgSpecificPid){
				int fgPid = strtoimax(commands[0]->args[1], NULL, 10);
				waitpid(fgPid, &status, 0);
				removePID(procExecuted, procNum, fgPid);
			}

			if(foreground){ 
				waitpid(pid, &status, 0); //Wait normally
			}
		}
	}
	if(fileDesc != 0)
		close(fileDesc);
}

void deleteCharIndex(char **array, int index){
	int i;
	for(i = index; i < backgroundProcCnt - 1; i++){
		if(array[i]){
			free(array[i]);
		}
		array[i] = array[i + 1];	
	} 
}

void deleteIntIndex(int *array, int index){
	int i;
	for(i = index; i < backgroundProcCnt - 1; i++){
		array[i] = array[i + 1];
	}
}

void removePID(char ** procExecuted, int *procNum, int pidToRemove){
	//search for matching pid index and remove both index matches in procExecuted and procNum
	int i;

	for(i = 0; i < backgroundProcCnt; i++){
		if(procNum[i] == pidToRemove && procNum[i]){
			deleteIntIndex(procNum, i);
			deleteCharIndex(procExecuted, i);
			backgroundProcCnt--;
		}
	}
}

void freeMem(char **commands, int tokenCount){
	int i;
	for(i = 0; i < tokenCount; i++) free(commands[i]);
	free(commands);
}

bool input(char **procExecuted, int *procNum, Command **commands){
	printf("cs350sh>");
	if(fgets(line, MAX_LINE_LEN, fp) == NULL) {
		perror("error reading line from stdin\n");
		exit(2);
	}
	return(tokenize(line, procExecuted, procNum, commands));
}

int main(){
	char *procExecuted[MAX_LINE_LEN];
	int procNum[MAX_LINE_LEN];
	Command *commandList[MAX_LINE_LEN]; 

	printf("Type exit at any time to quit the shell\n");
	if((fp = fdopen(STDIN_FILENO, "r")) == NULL){
		perror("error converting stdin to FILE *\n");
		exit(1);
	} 
	bool keepGoing = true;
	while(keepGoing){
		keepGoing = input(procExecuted, procNum, commandList);
	}
	fclose(fp);
	//Cleans up all background processes, not even the normal shell does this
	//when you exit
	for(int i = 0; i < backgroundProcCnt; i++){
		 kill(procNum[i], SIGKILL);
	}

	for(int i = 0; i < totalBGProcs; i++){
		if(procExecuted[i] != NULL){
			free(procExecuted[i]);
		}
	}

	for(int i = 0; i < commandCount; i++){
		Command_free(commandList[i]);
	}

	return 0;
}
