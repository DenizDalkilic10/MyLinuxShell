#define _GNU_SOURCE
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#define TOKEN_DIVIDER " \t\r\n\a"
#define READ_LINE_BUFFER_SIZE 1024 // dynamic allocation used if the buffer size is exceeded

int processHandler(char ** firstCommand, char ** secondCommand, int bytesToTransfer)
{

	pid_t cp1_id, cp2_id;
	int pipeMC1[2], pipeMC2[2]; //pipeMC1 = Main - CHild1 pipe , pipeMC2 = Main - Child2 pipe
	int isCompoundCommand;
	
	if (firstCommand[0] == NULL) { // An empty command was entered.
		return 1;
	}
	else if(secondCommand == NULL)
	{
		isCompoundCommand = 0;

		if (strcmp(firstCommand[0],"exit") == 0) // if the user types "exit" the program terminates
		{
			exit(1);
		}
	}
	else{
		isCompoundCommand = 1;
	}


/////////Pipe 1 is being created////////
if(isCompoundCommand){ // this will be true if the command is compound command with "|"
	if(pipe(pipeMC1) == -1){
		perror("Pipe between Main and Child 1 failed");
		exit(EXIT_FAILURE);
	}

	
	fcntl(pipeMC1[0],F_SETPIPE_SZ,1024*1024); //new pipe size
	fcntl(pipeMC1[1],F_SETPIPE_SZ,1024*1024); //new pipe size
	
	//printf("New Pipe Size %d\n",b );
}

//printf("BulletPoint\n");
cp1_id = fork(); //child process 1


if (cp1_id == 0) {
	if(isCompoundCommand){
		dup2(pipeMC1[1],1); //redirection of standart output
	}
  	//the command is a single command
	if (execvp(firstCommand[0], firstCommand) == -1) {
		printf("Error in firstCommand\n");
    exit(1);
  }

} 
else if (cp1_id < 0) {
	printf("Error during first process creation");

} else { //main process

	wait(NULL);

	if (isCompoundCommand)
	{

		char oneReadValue [bytesToTransfer]; //forms a buffer according to the size of the bytesToRead
		ssize_t readSize;
		ssize_t totalBytesRead = 0;
		int totalReadOperations = 0;

		close(pipeMC1[1]);
		////////Pipe 2 is being created////////
		if(pipe(pipeMC2) == -1){
			perror("Pipe between Main and Child 2 failed");
			exit(1);
		}
		fcntl(pipeMC2[0],F_SETPIPE_SZ,1024*1024); //new pipe size
		fcntl(pipeMC2[1],F_SETPIPE_SZ,1024*1024); //new pipe size

	/////////////////////////////////////// Data Transfer Between Pipes Handled By Main Process ///////////////////////////////////////////////

    while((readSize = read(pipeMC1[0],oneReadValue, bytesToTransfer)) > 0){ // main reads from pipe 1 and writes to pipe 2
    	totalBytesRead += readSize;
    	totalReadOperations++;
      	write(pipeMC2[1], oneReadValue, readSize); //read doğru çalışıyo burdan sonrasını check et bi yerde segmentation fault alıyosunnnnn!!!!!!
      }

  /////////////////////////////////////// Data Transfer Between Pipes Handled By Main Process ///////////////////////////////////////////////

  close(pipeMC2[1]); //closing the write end of pipe 2


  cp2_id = fork();
  //printf("CHILD 2 FORKED\n");

  if (cp2_id == 0) { //second child

    dup2(pipeMC2[0],0); //redirection of standart input
    close(pipeMC2[0]); //closes the read end of Main - CHild2 pipe

    if (execvp(secondCommand[0], secondCommand) == -1) {
      //printf("%s\n",secondCommand[0]);
      printf("Error in secondCommand\n"); 
      exit(1);
    }
    
  } 
  else if (cp2_id < 0) {
   printf("Error during second process creation");
 } else {
    wait(NULL); // wait for the child to terminate
    printf("\nCharacter Count: %ld\n", totalBytesRead);
    printf("Read Call Count: %d\n\n", totalReadOperations);

  }
}

}

return 1;
}


char **createArguments(char *line)
{
	
	int bufsize = 64;
	char **wordArray = malloc(bufsize * sizeof(char*));
	char *word = NULL;

	word = strtok(line, TOKEN_DIVIDER);

  int i = 0;
	while (word != NULL) {
		wordArray[i++] = word;
		
		if (i >= bufsize) {
			bufsize += 64;
			wordArray = realloc(wordArray, bufsize * sizeof(char*));
		}
		word = strtok(NULL, TOKEN_DIVIDER);
	}
	wordArray[i] = NULL;
	return wordArray;
}


void shellCore(char * file, int bytesToTransfer)
{

  char *line = NULL;                  // the command in interactive mode
  char * firstCommand = NULL;         // the first command in bash mode
  char * secondCommand = NULL;        // the second command in bash mode
  
  char **args = NULL;                 // used for interactive mode for keeping tokens of one single command
  char ** argsOfFirstCommand = NULL;  // used for bash mode
  char ** argsOfSecondCommand = NULL; // used for bash mode

  size_t len = 0;
  ssize_t read = 0;

  int success = 1;                 // integer for keeping the success value of operations
  int fd = 0;                      // file descriptor
  int interactiveMode = 0;     // identifies the mode of the shell
  int isCompoundCommand = 0;       // identifies the type of the given command

  const char tok[2] = "|";     // used for dividing compound commands into tokens


  if(file == NULL){
  	interactiveMode = 1;
  }

  if(!interactiveMode){ //if bash mode is active
  	if ( ( fd = open (file, O_RDONLY) ) < 0 ) {
  		exit (1);
  	}

  	FILE * fp = fdopen(fd,"r"); // opens the input file, fp points to beginning of the file

  	do {

  	read = getline(&line, &len, fp); //for reading from input file


  	if(read > 1){
  		firstCommand = strtok(line,tok); //if not null, and if null it is a single command
  		if (firstCommand != NULL)
  		{
  			secondCommand = strtok(NULL,tok);
  			if (secondCommand != NULL){
  				isCompoundCommand = 1;
  			}
  			else
  			{
  				isCompoundCommand = 0;
  			}
  		}

  		if(isCompoundCommand){

        	//printf("Compound Command detected...\n");
  			argsOfFirstCommand = createArguments(firstCommand);
  			argsOfSecondCommand = createArguments(secondCommand);
  			success = processHandler(argsOfFirstCommand,argsOfSecondCommand,bytesToTransfer);
  			if(argsOfFirstCommand != NULL){
  				free(argsOfFirstCommand);
  			}
  			if(argsOfSecondCommand != NULL){
  				free(argsOfSecondCommand);
  			}

  		}
  		else{
        	//printf("Single Command detected...\n");
  			args = createArguments(line);
  			success = processHandler(args,NULL,bytesToTransfer);
  			if(args != NULL){
  			free(args); //bu değişti
  		}
  	}  	
  }


} while (success && read != -1);
free(line);
fclose(fp);
}
else if (interactiveMode){

	do {

		printf("bilshell-$: ");
    read = getline(&line,&len,stdin);//readLine(); // reads an input from the user


    if(read > 1){ //if the command is not empty
  		firstCommand = strtok(line,tok); //if not null, and if null it is a single command
  		if (firstCommand != NULL)
  		{
  			secondCommand = strtok(NULL,tok);
  			if (secondCommand != NULL){
  				isCompoundCommand = 1;
  			}
  			else
  			{
  				isCompoundCommand = 0;
  			}
  		}

  		if(isCompoundCommand){

        	//printf("Compound Command detected...\n");
  			argsOfFirstCommand = createArguments(firstCommand);
  			argsOfSecondCommand = createArguments(secondCommand);
  			success = processHandler(argsOfFirstCommand,argsOfSecondCommand,bytesToTransfer);
  			if(argsOfFirstCommand != NULL){
  				free(argsOfFirstCommand);
  			}
  			if(argsOfSecondCommand != NULL){
  				free(argsOfSecondCommand);
  			}

  		}
  		else{
        	//printf("Single Command detected...\n");
  			args = createArguments(line);
  			success = processHandler(args,NULL,bytesToTransfer);
  			if(args != NULL){
  			free(args); //bu değişti
     }
   }
 }
} while (success && read != -1);
free(line);
}
}

int main(int argc, char * argv[])
{
	int bytesToTransfer = atoi(argv[1]);

	if (bytesToTransfer == 0 || bytesToTransfer > 4096){
		printf("Illegal number of bytesToTransfer: %d\n",bytesToTransfer);
		printf("Exiting from bilshell...");
		exit(1);
	}
	else{
		printf("Bytes to transfer in each operation: %d \n", bytesToTransfer);
	}
	if(argv[2] == NULL){
		printf("No input file\n");
		printf("\n********Interactive Mode Activated************\n");
	}
	else{
		printf("Input File Name Is: %s \n",argv[2]); 
		printf("\n********Bash Mode Activated************\n");
		printf("\nPLease wait...Reading the file...\n");
	}

 	shellCore(argv[2], bytesToTransfer); //pass input file as an argument

 	return EXIT_SUCCESS;
 }
