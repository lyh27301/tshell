//COMP 310 Assignment 1
//Student ID: 260797917
//Last Name: Liu
//First Name: Yanhan

#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>

size_t PATH_MAX_SIZE = 100;

struct rlimit rl;

jmp_buf loop_buf;

const char* delim = " \0";

int status = 1;

int MAX_LINE = 1024; 

int fifo = 0; // equals 1 if a valid fifo pathname is provided, 0 otherwise.

char* line = NULL;

char* history[100];

char fifo_pathname[100];

char directory[100];

// declare functions
void loop(void);
char* get_a_line(void);
int my_system(char* line);
char*** parse(char* line);
void handle_sigint(int sig);
void handle_sigtstp(int sig);

int main(int argc, char** argv){
	
	//signal handlers
	signal(SIGINT, handle_sigint); 
    signal(SIGTSTP, handle_sigtstp); 

	for (int i = 0; i < 100; i++){
    history[i] = NULL;
    }
    
    //check if a fifo pathname is provided. 
    if (argc > 1) 
    {
		int if_strcat_success = 1;
		if(argv[1][0]!='/'){ 
			char cwd[100];
			getcwd(cwd,PATH_MAX_SIZE);
			if((strcat(cwd, "/")) == NULL){
				if_strcat_success = 0;
			}
			if((strcat(cwd, argv[1])) == NULL){
				if_strcat_success = 0;
			}
			strcpy(argv[1], cwd);
		}
		
        if ((strcpy(fifo_pathname, argv[1]) == NULL )|| (if_strcat_success == 0)){
            fprintf(stderr, "Failed to copy fifo pathname!\nYou are unableto use pipes in your following commands\n");
            fflush(stderr);		
        }else{
            printf( "Fifo pathname was copied successfully.\nYou can use commands with pipes in tshell.\n");
            fflush(stdout);
            fifo = 1;
        }
    }else{
        printf("No fifo pathname is provided.\nYou are unableto use pipes in your following commands\n");
        fflush(stdout);
    }
    
    //keep reading and executing commands read until ^C
    loop();
	
}

void loop(void){
	char* line;
	
	
	
	
	//execute the commands
	while(1){
		setjmp(loop_buf);
		signal(SIGINT, handle_sigint); 
		signal(SIGTSTP, handle_sigtstp); 
		getcwd(directory,100);
		printf("tshell: %s $", directory);
		fflush(stdout);
		
		//read the commands entered by the user line by line
		line = get_a_line();
		
		if(line==NULL){
			printf("\n");
			break;
		}
		
		//parse and execute the command
		if(strlen(line)>0){
			my_system(line);
		}
	    else{
			free(line);
		}
		//freeing the memory
	}
}

char* get_a_line(void){
	line = (char*)malloc(sizeof(char)*MAX_LINE);
	int count = 0;
	
	while(1){
		int c = fgetc(stdin);
		if(c == '\n'){
			break;
		}else if(count==(MAX_LINE-2)) {
		    MAX_LINE = 2*MAX_LINE;
			line = (char*)realloc(line, sizeof(char)*MAX_LINE);
		}else if(c==EOF){
			if(count != 0)
			{
				line[count]='\0';
				return line;
			}
			else
			{
				line[count]='\0';
				return NULL;
			}
		}
		line[count] = (char) c;
		count++;	
	   }	
	
	line[MAX_LINE-1] = '\0';
	return line;
	
}

int my_system(char* line){
	//store line to history array
	if(history[99]!=NULL){
		free(history[99]);//free the record of the command that we no longer need
		history[99] = NULL;
	}
	for(int i=99; i>0; i--){
		history[i] = history[i-1]; 
	}
	history[0] = line;
	
	//copy a line to parse
	char line_copy[MAX_LINE];
    strcpy(line_copy, line);
	
	//status
	int status = 0;
	
	//pipe
	int f[2]; //= {0,1};
	
	//parse the line 
	char*** commands = parse(line_copy);
	char** args_1 = commands[0];
	char** args_2 = commands[1];
	
	//internal command_1
	//chdir
	if(strcmp(args_1[0], "chdir") == 0||strcmp(args_1[0], "cd") == 0){
		char* new_directory = NULL;//
		if (args_1[1] == NULL){ // no path is given 
		  fprintf(stderr, "No path is given. Failed to change directory!\n");
          fflush(stderr); // no change to current working directory
          return 0;
 
        }else{// change to the given directory
            new_directory = args_1[1];
            if (chdir(new_directory) < 0){
                fprintf(stderr, "\nFailed to change directory!\n");
                fflush(stderr);
                return 0;
            }else{
				return 1;
			}
        }
       
	}
	
	//internal command_2
	//history
	if(strcmp(args_1[0], "history") == 0){
		printf("\ncommand history from the most recent one: \n");
	    fflush(stdout);
	    for(int i=0; i<100; i++){
			if(history[i]==NULL){
			    printf("history ends.\n");
			    fflush(stdout);
			    break;
			}
		    printf("%d. %s\n",(i+1),history[i] );
		    fflush(stdout);
		}
		return 0;
	}
	
	//internal command_3
	//limit
	if(strcmp(args_1[0], "limit") == 0){
		// limit with correct 2 arguments
		if (args_1[1] != NULL && args_1[2] != NULL && args_1[3] == NULL){
            //get soft limit 
            rlim_t current = strtoul(args_1[1], (char **)NULL, 10); 
            if (errno == ERANGE){
                fprintf(stderr, "range error occurred when setting soft limit");
                fflush(stderr);
                errno = 0;
                return 0;
            }
           
            //get hard limit
            rlim_t max = strtoul(args_1[2], (char **)NULL, 10);
            if (errno == ERANGE){
                fprintf(stderr, "range error occurred when setting hard limit");
                fflush(stderr);
                errno = 0;
                return 0;
            }
            //set limits
            rl.rlim_cur = current;
            rl.rlim_max = max;
            if (setrlimit(RLIMIT_DATA, &rl) < 0){ 
				 //if limit setting failes 
                fprintf(stderr, "Failed to set new limit\n" ); 
                fflush(stderr);
                return 0;
            }else{ 
				// if limit setting successes
                fprintf(stdout, "limit updated.\n");
            }
		}else if(args_1[1] != NULL && args_1[2] == NULL){//one argument; change soft limit only
			//get soft limit
			rlim_t current = strtoul(args_1[1], (char **)NULL, 10); 
            if (errno == ERANGE){
                fprintf(stderr, "range error occurred when setting soft limit");
                fflush(stderr);
                errno = 0;
                return 0;
            }
			//set limits
            rl.rlim_cur = current;
			rl.rlim_max = 2333333333;
            if (setrlimit(RLIMIT_DATA, &rl) < 0){ 
				 //if limit setting failes 
                fprintf(stderr, "Failed to set new soft limit\n" ); 
                fflush(stderr);
                return 0;
            }else{ 
				// if limit setting successes
                fprintf(stdout, "soft limit updated.\n");
            }
                  
        }else{ // if more or less arguments are given for limit command
            fprintf(stderr, "Wrong argument for limit:\n(a) enter 2 arguments only, for soft limit and hard limit respectively;\n(b) enter 1 argument only to set new soft limit\n" );
            fflush(stderr);
            return 0;
        }
        fflush(stdout);
        return 1;
    }
	
	//external command:
	//execute the command
	if(commands[1] == NULL){//no pipe
		pid_t pid = fork();
		if (pid < 0){ // error
			printf("Failed to  create a child process\n");
		}else if(pid == 0){//child process
			if(execvp(args_1[0], args_1)==-1){
				fprintf(stderr, "Command %s execution failed.\n", args_1[0]);
				fflush(stderr);
				_exit(0);
			}
			_exit(0);
		}else{// parent process
			wait(NULL);
		}
	
	}else{

		//pipe exists 
		if(fifo == 0){//no fifo pathname is provided when starting tshell, end execution of this command and continue to read the next one.
		    fprintf(stderr, "No fifo pathname is provided. Failed to execute the command with pipe.");
		    fflush(stderr);
		    return 0;
		}
		
		//fork child process 1 for command_1
		pid_t pid_1 = fork();
		if(pid_1 < 0){
			printf("failed to  create a child process for command_1.\n");
		    return 0;
		//execute command_1
		}else if(pid_1==0){ 
			fflush(stdout);
			f[1] = open(fifo_pathname, O_WRONLY);
			close(1);
			dup(f[1]);
			close(f[0]);
			chdir(directory);
			if (execvp(args_1[0], args_1)<0){
				printf("failed to execute command_1.\n" );
                close(f[1]);
                _exit(0);
			}
		    close(f[1]);
            _exit(0);
		}
		
		//fork child process 2 for command_2
		pid_t pid_2 = fork();
		if(pid_2 < 0){
		    printf("failed to  create a child process for command_2.\n");	
		//execute command 2
		}else if(pid_2==0){
			f[0] = open(fifo_pathname, O_RDONLY);
			close(0);
			dup(f[0]);
			//close(f[1]);
			chdir(directory);
			if (execvp(args_2[0], args_2)<0){
				printf("failed to execute command_2.\n" );
                close(f[0]);
                _exit(0);
			}
		    close(f[0]);
            _exit(0);
		}
		
		//wait for child process 1 to end, store its status in s_1
		int s_1 = 0;
		waitpid(pid_1, &s_1, 0);
		//wait for child process 2 to end, store its status in s_2
		int s_2 = 0;
		waitpid(pid_2, &s_2, 0);
	}
	
	return 0;

}
	
char*** parse(char* line){
	int MAX_ARGS_1 = 100;
	int MAX_ARGS_2 = 100;
	char*** commands = (char***)malloc(sizeof(char**) * 2);
	
	char** args_1 = (char**)malloc(sizeof(char*) * MAX_ARGS_1);
	char** args_2 = (char**)malloc(sizeof(char*) * MAX_ARGS_2);

	int l = strlen(line);
	
	int count = 0;
	
	int pipe_existence = 0;
	
	char* p = (char*)malloc(sizeof(char) * l);
	
	strcpy(p, line); 
	
	while((char*)(p = strtok(p, delim))!=NULL){
	    //check whether the current token is a pipe or not
	    
	    //if the current token is a pipe, go read command_2
	    if(strcmp(p,"|") == 0){
			count = 0;
			pipe_existence = 1;
			p = NULL;
			while((char*)(p = strtok(p, delim))!=NULL){
				args_2[count] = p;
				p = NULL;
				count++;
			}
			break;
			
			if (count > MAX_ARGS_2){
			    MAX_ARGS_2 = 2 * MAX_ARGS_2;
			    args_2 = (char**)realloc(args_2, sizeof(char*) * MAX_ARGS_2);
		    }
		}
		//read command_1
	    args_1[count] = p;
	    p = NULL;		
       	count++;
       	if (count > MAX_ARGS_1){
			MAX_ARGS_1 = 2 * MAX_ARGS_1;
			args_1 = (char**)realloc(args_1, sizeof(char*) * MAX_ARGS_1);
		}
		
	}
	
	commands[0] = args_1;
	commands[1] = args_2;	
	
	if (pipe_existence != 1){
		free(args_2);
		commands[1] = NULL;
	}
	
	
	
	free(p);
	
	if (count == 0){
			fprintf(stderr, "syntax error.\n");
			fflush(stderr);
			longjmp(loop_buf, 1);
		}
	
	return commands;

}

void handle_sigint(int sig){
	//ask for confirmation
	printf("\nDo you wish to exit tshell? (Y/n)\n");
	fflush(stdout);
	
	//read the answer
	char ans[MAX_LINE];
	fgets(ans, MAX_LINE, stdin);
	if((strcmp(ans, "Y\n")*strcmp(ans, "y\n"))==0){
		//clean-up
		if(line!=NULL){
			free(line);
			line = NULL;
		}
		for(int i=0; i<100; i++){
			if(history[i]!=NULL){
				 free(history[i]);
				 history[i] = NULL;
			}else{
				break;
			}
		}
		//exit
		printf("tshell terminated.\n");
	    fflush(stdout);
	    signal(SIGINT, SIG_DFL);
        raise(SIGINT);
		
	}else if((strcmp(ans, "n\n")*strcmp(ans, "N\n"))==0){
		printf("tshell continues.\n");
	    fflush(stdout);
	    signal(SIGINT, handle_sigint);
	    longjmp(loop_buf, 1);
	}else{
		printf("Unknown input %s. tshell continues by default. if you wish to exit, please enter 'Y' to confirm exiting next time.\n", ans);
	    fflush(stdout);
	    signal(SIGINT, handle_sigint);
	    longjmp(loop_buf, 1);
	}
}

void handle_sigtstp(int sig){
	printf("\n");
	longjmp(loop_buf, 1);
}
