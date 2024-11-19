#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

int backgroundFlag = 1;

void mainShell(struct sigaction, char*[], int, int, int, char[], char[]);
void getInput(char*[], int, int*, char[], char[]);
void execOther(struct sigaction, char*[], int*, int*, char[], char[]);
void printStatus(int);
void catchSignal(int);

// Prints status of exit value or
// termination signal
void printStatus(int exCode) {
  //exit code handling
  if (WIFEXITED(exCode)) {
    printf("the exit value is: %d\n", WIFEXITED(exCode));
  }
  //termination signal handling
  else {
    printf("the termination signal is: %d\n", WTERMSIG(exCode));
  }
}

// Signal handler, write() implementation taken inspiration
// from Signal Handling API exploration
void catchSignal(int sig) {
  if (backgroundFlag == 1) {
		char *temp = "Entering foreground mode. Ignoring &.\n";
		write(STDOUT_FILENO, temp, 38); //buffer equal to temp size
		fflush(stdout);
		backgroundFlag = 0;
	}
	else {
		char *temp = "Exiting foreground mode, no longer ignoring &.\n";
		write (STDOUT_FILENO, temp, 47); //buffer equal to temp size
		fflush(stdout);
		backgroundFlag = 1;
	}
}

// Handles input and output files, then executes commands by using fork()
// Also handles the background processes
void execOther(struct sigaction sigint, char* input[], int* exCode, int* background, char inputFile[], char outputFile[]) {

  pid_t spawnPid = fork();
  int success1;
  int success2;
  int assign;

  switch(spawnPid) {
    case -1:
      perror("fork() error\n");
      exit(1);
      break;
    case 0:
      sigint.sa_handler = SIG_DFL;
      sigaction(SIGINT, &sigint, NULL);
      //assign inputfile process
      if(strcmp(inputFile, "")) { //check for input filename
        success1 = open(inputFile, O_RDONLY);
        //check for open success
        if(success1 == -1) {
          perror("Input file found invalid\n");
          exit(1);
        }
        //assign the input file
        assign = dup2(success1, 0);
        if(assign == -1) {
          perror("Input file invalid assign\n");
          exit(2);
        }
        //close the assignment
        fcntl(success1, F_SETFD, FD_CLOEXEC);
      }
      //assign outfile process
      if(strcmp(outputFile, "")) {//check for output filename
        success2 = open(outputFile, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        //check for open success
        if(success2 == -1) {
          perror("Input file found invalid\n");
          exit(1);
        }
        //assign the output file
        assign = dup2(success2, 1);
        if(assign == -1) {
          perror("Input file invalid assign\n");
          exit(2);
        }
        //close the assignment
        fcntl(success2, F_SETFD, FD_CLOEXEC);
      }
      //execute the input command, with error handling
      if(execvp(input[0], input)) {
        printf("%s: no file or directory found\n", input[0]);
        fflush(stdout);
        exit(2);
      }
      break;
    default:
      if(*background && backgroundFlag) { //handle for backgroundFlag
        pid_t thisPid = waitpid(spawnPid, exCode, WNOHANG);
        printf("the background pid is %d\n", spawnPid);
        fflush(stdout);
      }
      else {
        pid_t thisPid = waitpid(spawnPid, exCode, 0);;
      }
      //look for terminated background process
      while((spawnPid = waitpid(-1, exCode, WNOHANG)) > 0) {
        printf("terminated the child %d\n", spawnPid);
        printStatus(*exCode);
        fflush(stdout);
      }
  }
}

// Queries user input then iterates over the input so that
// it can be handled properly in later functions
void getInput(char* input[], int pid, int* background, char inputFile[], char outputFile[]) {
  char tempInput[2048];
  //int i;
  //int j;

  //clear output stream, take in a new user input
  printf(": ");
  fflush(stdout);
  fgets(tempInput, 2048, stdin);

  int newline = 0; //flag for detecting newline character
  //iterates over input finding the newline character and changing it to a null terminator
  for(int i = 0; newline == 0 && i < 2048; i++) {
    if(tempInput[i] == '\n') {
      tempInput[i] = '\0';
      newline = 1;
      //printf("input taken as: %s\n", tempInput); //debug statement
    }
  }

  //catch blank input as nothing
  if(!strcmp(tempInput, "")) {
    input[0] = strdup("");
    return;
  }

  //set up the iterating over the user input with space detection
  char space[2] = " ";
  char* token = strtok(tempInput, space);

  for(int i = 0; token; i++) {
    //detect if it is a background flagged command
    if(strcmp(token, "&") == 0) {
      *background = 1;
    }
    //find input file with the '<' character
    else if(strcmp(token, "<") == 0) {
      token = strtok(NULL, space);
      strcpy(inputFile, token);
    }
    //find output file with the '>' character
    else if(strcmp(token, ">") == 0) {
      token = strtok(NULL, space);
      strcpy(outputFile, token);
    }
    //look for $$ for expansion to the pid
    else{
      input[i] = strdup(token);
      for(int j = 0; input[i][j]; j++) {
        if(input[i][j] == '$' && input[i][j+1] == '$') {
          input[i][j] = '\0';
          snprintf(input[i], 256, "%s%d", input[i], pid);
          //printf("found variable, is now %s\n", input[i][j]);
        }
      }
    }
    //next item
    token = strtok(NULL, space);
  }
  fflush(stdout);
}

// Main loop that the Shell runs in. Prompts user input and handles the built-in commands
// and redirects other commands to execOther() function
void mainShell(struct sigaction sigint, char* input[], int pid, int exCode, int background, char inputFile[], char outputFile[]) {
  int running = 1;

  //main Shell loop
  do {
    fflush(stdout);
    getInput(input, pid, &background, inputFile, outputFile); //get user input

    //case of line starting with #
    if(input[0][0] == '#') {
      continue;
    }
    //case of blank line
    else if(input[0][0] == '\0') {
      continue;
    }
    //case of exit statement, just set loop to end
    else if (strcmp(input[0], "exit") == 0) {
      running = 0;
    }
    //case of using "cd" command
    else if(strcmp(input[0], "cd") == 0) {
      if(input[1]) {
        if(chdir(input[1]) == -1) {
          //invalid directory handling
          printf("Could not find specified directory (%s)\n", input[1]);
          fflush(stdout);
        }
      }
      else {
        //given no input, go to HOME directory
        chdir(getenv("HOME"));
        //printf("going to %s\n", getenv("HOME")); //debug statement
      }
    }
    //case of using "status" command
    else if(strcmp(input[0], "status") == 0) {
      //printf("pid is: %d\n", pid); //debug statement
      printStatus(exCode);
    }
    //case of using any other command, use the execOther() function
    else {
      execOther(sigint, input, &exCode, &background, inputFile, outputFile);
    }

    //reset
    for(int i = 0; input[i] != NULL; i++) {
      input[i] = NULL;
    }
    background = 0;
    inputFile[0] = '\0';
    outputFile[0] = '\0';

  } while(running != 0);
}



int main (int argc, char *argv[]) {

  int pid = getpid();
  int exitCode = 0;
  int background = 0;

  char inputFile[256];
  char outputFile[256];
  char* input[512];
  for(int i = 0; i < 512; i++) {
    input[i] = NULL;
  }

  //handling signals
  struct sigaction sigstop = {0};
	//^Z redirection
	sigstop.sa_handler = catchSignal;
	sigfillset(&sigstop.sa_mask);
	sigstop.sa_flags = 0;
	sigaction(SIGTSTP, &sigstop, NULL);

	//^C ignore
	struct sigaction sigint = {0};
	sigint.sa_handler = SIG_IGN; //set ignore
	sigfillset(&sigint.sa_mask);
	sigint.sa_flags = 0;
	sigaction(SIGINT, &sigint, NULL);

  mainShell(sigint, input, pid, exitCode, background, inputFile, outputFile);

  return 0;
}
