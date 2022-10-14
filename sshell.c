#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 16
#define PATH_MAX 4096
#define MAX_PIPE 4
#define TRUE 1

// Data Structures
struct Command
{
	char *args[MAX_ARGS+1];
	int fd_redir[3];
};

struct Stack 
{
	int size;
	int size_max;
	char **stack;
};

// Functions for pushd, popd, and dirs
void stack_resize(struct Stack *st)
{
	st->size_max *= 2;
	st->stack = realloc(st->stack, st->size_max*sizeof(char*));
}

void stack_push(struct Stack *st, char *str)
{
	if(st->size >= st->size_max) stack_resize(st);
	st->stack[st->size] = malloc((strlen(str)+1)*sizeof(char));
	strcpy(st->stack[st->size++], str);
}

void stack_pop(struct Stack *st, char **str)
{
	*str = malloc((strlen(st->stack[--st->size])+1)*sizeof(char)); 
	strcpy(*str, st->stack[st->size]);
	free(st->stack[st->size]);
}

void stack_print(struct Stack *st)
{
	int i;
	char *cwd = getcwd(NULL, 0);
	printf("%s\n", cwd);
	for(i = st->size - 1; i >= 0; i--)
	{
		printf("%s\n", st->stack[i]);
	}
	free(cwd);
}

void cleanup(struct Stack *st, struct Command *cmd)
{
	int i;
	if(st != NULL)
	{
		for(i = st->size; i >= 0; i--)
			free(st->stack[i]);
		free(st->stack);
	}
	if(cmd != NULL)
		free(cmd);
}

int main(void)
{
	char cmd[CMDLINE_MAX];
	char dudcmd[CMDLINE_MAX];
	struct Stack dir_st = {0, 2, malloc(2*sizeof(char*))};

	while(1) 
	{
		/* Print prompt */
		printf("sshell@ucd$ ");
		fflush(stdout);

		/* Get command line */
		fgets(cmd, CMDLINE_MAX, stdin);
		/* Print command line if stdin is not provided by terminal */
		if(!isatty(STDIN_FILENO))
		{
			printf("%s", cmd);
			fflush(stdout);
		}

		/* Remove trailing newline from command line */
		char *nl = strchr(cmd, '\n');
		if(nl)
			*nl = '\0';
		strcpy(dudcmd, cmd);

		// Parse setup
		int arg_iter = 1;
		int task_iter = 0;
		int input_iter = 0;
		int parse_error = 0;
		struct Command *tasks;
		tasks = malloc(sizeof(struct Command));
		tasks[0].args[0] = cmd;
		int j;
		for(j = 0; j < 3; j++) 
			tasks[0].fd_redir[j] = j;

		//Parser
		for( ; input_iter < CMDLINE_MAX; input_iter++)
		{
			if(arg_iter > MAX_ARGS && !(cmd[input_iter] == '>' || cmd[input_iter] == '<' || cmd[input_iter] == '|'))
			{
				fprintf(stderr, "Error: too many process arguments\n");
				parse_error = TRUE;
				break;
			}
			else if(cmd[input_iter] == ' ') //current arguement is complete
			{
				cmd[input_iter] = '\0';
				while(cmd[input_iter+1] == ' ') input_iter++; //check for extra spaces
				if(cmd[input_iter+1] == '\0')
				{
					tasks[task_iter].args[arg_iter] = NULL;
					break;
				}
				tasks[task_iter].args[arg_iter++] = cmd + input_iter + 1;
			}
			else if(cmd[input_iter] == '>' || cmd[input_iter] == '<') //file redirect
			{
				if(arg_iter <= 1) 
				{
					fprintf(stderr, "Error: missing command\n");
					parse_error = TRUE;
					break;
				}

				//manage different redirect types
				int redir_type = (cmd[input_iter] == '>') ? 1 : 0;
				int file_flags = redir_type ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY;
				int file_mode = redir_type ? 0644 : 0444;

				//cant input redirect if this in a pipe
				if(task_iter >= 1 && redir_type == 0)
				{
					fprintf(stderr, "Error: mislocated input redirection\n");
					parse_error = TRUE;
					break;
				}

				//if there were two redirects, close the previous
				if(tasks[task_iter].fd_redir[redir_type] != redir_type)
					close(tasks[task_iter].fd_redir[redir_type]);

				//this catches the case where there was no space before this
				if(cmd[input_iter-1] != ' ' && cmd[input_iter-1] != '\0') 
				{
					arg_iter++;
					cmd[input_iter] = '\0';
				}

				while(cmd[++input_iter] == ' '); //find start of filename
				//check if character is valid
				if(cmd[input_iter] == '\0' || cmd[input_iter] == '>' || cmd[input_iter] == '<' || cmd[input_iter] == '|')
				{
					fprintf(stderr, "Error: no %sput file\n", redir_type ? "out" : "in");
					parse_error = TRUE;
					break;
				}

				// get filename
				char* filename = cmd + input_iter;
				while(!(cmd[input_iter] == ' ' || cmd[input_iter] == '\0' || cmd[input_iter] == '>' || cmd[input_iter] == '<' || cmd[input_iter] == '|')) 
					input_iter++;
				int line_end = (cmd[input_iter] == '\0');
				int meta_char = !line_end && !(cmd[input_iter] == ' ');
				if(meta_char) //cant terminate string since it would destroy meta character
				{
					int length = (cmd + input_iter) - filename;
					char* new_filename = malloc((length+1)*sizeof(char));
					strncpy(new_filename, filename, length);
					new_filename[length+1] = '\0';
					filename = new_filename;
				}
				else if(!line_end) //terminate string and check for spaces
				{
					cmd[input_iter] = '\0';
					while(cmd[input_iter+1] == ' ') input_iter++;
					line_end = (cmd[input_iter+1] == '\0'); //may have reached the end of line
				}

				//open file
				int fd = open(filename, file_flags, file_mode);
				if(fd<0) 
				{
					fprintf(stderr, "Error: cannot open %sput file\n", redir_type ? "out" : "in");
					if(meta_char) free(filename);
					parse_error = TRUE;
					break;
				}
				else tasks[task_iter].fd_redir[redir_type] = fd;

				//bookeeping
				if(line_end)
				{
					tasks[task_iter].args[arg_iter-1] = NULL;
					break;
				}
				else if(meta_char) 
				{
					free(filename);
					input_iter--;
				}
				tasks[task_iter].args[arg_iter-1] = cmd + input_iter + 1;
			}
			else if(cmd[input_iter] == '|') //piping
			{
				/* Error catching */
				if(arg_iter <= 1) //not enough arguements
				{
					fprintf(stderr, "Error: missing command\n");
					parse_error = TRUE;
					break;
				}
				else if(tasks[task_iter].fd_redir[1] != 1) //cant pipe with output redir
				{
					fprintf(stderr, "Error: mislocated output redirection\n");
					parse_error = TRUE;
					break;
				}
				/* parsing */
				cmd[input_iter] = '\0';
				if(cmd[input_iter-1] == '\0' || cmd[input_iter-1] == ' ')
					tasks[task_iter].args[arg_iter-1] = NULL;
				else
					tasks[task_iter].args[arg_iter+1] = NULL;
				while(cmd[++input_iter] == ' ');
				if(cmd[input_iter] == '\0')
				{
					fprintf(stderr, "Error: missing command\n");
					parse_error = TRUE;
					break;
				}
				/* make a new task */
				int fd[2];
				pipe(fd);
				tasks[task_iter].fd_redir[1] = fd[1];
				tasks = realloc(tasks, (++task_iter+1)*sizeof(struct Command)); //make new task
				for(j = 0; j < 3; j++) tasks[task_iter].fd_redir[j] = j;
				tasks[task_iter].fd_redir[0] = fd[0];
				tasks[task_iter].args[0] = cmd + input_iter;
				arg_iter = 1;
			}
			else if(cmd[input_iter] == '\0')
			{
				tasks[task_iter].args[arg_iter] = NULL;
				break;
			}
		}
		if(parse_error) 
		{
			//close all opened files
			int i, j;
			for(i = 0; i < task_iter + 1; i++)
				for(j = 0; j < 3; j++)
					if(tasks[i].fd_redir[j] > 2)
						close(tasks[i].fd_redir[j]);
			cleanup(NULL, tasks);
			continue;
		}

		//Execute
		int retval;
		if(task_iter >= 1) //piping present
		{
			int i;
			int retvals[task_iter+1];
			int pid[task_iter+1];
			for(i = 0; i < task_iter + 1; i++)
			{
				if((pid[i] = fork()) == 0)
				{
					//Setup redirects
					int j;
					for(j = 0; j < 3; j++) 
						if(tasks[i].fd_redir[j] > 2)
							dup2(tasks[i].fd_redir[j], j);

					//Close all other files
					int k;
					for(j = 0; j < task_iter + 1; j++)
						for(k = 0; k < 3; k++)
							if(tasks[j].fd_redir[k] != k)
								close(tasks[j].fd_redir[k]);
					execvp(tasks[i].args[0], tasks[i].args);
					fprintf(stderr, "Error: command not found\n");
					cleanup(&dir_st, tasks);
					exit(1);
				}
				//close files in parent
				if(tasks[i].fd_redir[0] != 0)
					close(tasks[i].fd_redir[0]);
				if(tasks[i].fd_redir[1] != 1)
					close(tasks[i].fd_redir[1]);
				
			}
			i = 0;
			while(i < task_iter + 1)
			{
				int status;
				if(waitpid(pid[i], &status, WUNTRACED | WNOHANG))
				{
					if(WIFEXITED(status))
						retvals[i++] = WEXITSTATUS(status);
					else if(WIFSIGNALED(status))
						retvals[i++] = (WTERMSIG(status) == SIGPIPE) ? 0 : WTERMSIG(status);
				}
			}
			fprintf(stderr, "+ completed '%s' ", dudcmd);
			for(i = 0; i < task_iter+1; i++) 
				fprintf(stderr, "[%d]", retvals[i]);
			fprintf(stderr, "\n");
			cleanup(NULL, tasks);
			continue;
		}
		else
		{
			/* Builtin commands */
			if(!strcmp(cmd, "exit")) 
			{
				fprintf(stderr, "Bye...\n+ completed 'exit' [0]\n");
				cleanup(&dir_st, tasks);
				break;
			}
			else if(!strcmp(tasks[0].args[0], "pwd"))
			{
				char *cwd = getcwd(NULL, 0);
				if(cwd == NULL)
				{
					printf("fatal error: getcwd\n");
					break;
				}
				else
				{
					dprintf(tasks[0].fd_redir[1],"%s\n", cwd);
					retval = 0;
				}
				free(cwd);
			}
			else if(!strcmp(tasks[0].args[0], "cd"))
			{
				retval = abs(chdir(tasks[0].args[1]));
				if(retval)
					fprintf(stderr, "Error: cannot cd into directory\n");
			}
			else if(!strcmp(tasks[0].args[0], "pushd"))
			{
				char *cwd = getcwd(NULL, 0);
				retval = abs(chdir(tasks[0].args[1]));
				if(retval)
					fprintf(stderr, "Error: no such directory\n");
				else
					stack_push(&dir_st, cwd);
				free(cwd);
			}
			else if(!strcmp(tasks[0].args[0], "popd"))
			{
				if(!dir_st.size)
				{
					fprintf(stderr, "Error: directory stack empty\n");
					retval = 1;
				}
				else
				{
					char *dir = NULL;
					stack_pop(&dir_st, &dir);
					retval = abs(chdir(dir));
					free(dir);
				}
			}
			else if(!strcmp(tasks[0].args[0], "dirs"))
			{
				stack_print(&dir_st);
				retval = 0;
			}
			/* Regular command */
			else
			{
				int pid = fork();
				if(pid > 0)
				{
					int status;
					waitpid(pid, &status, 0);
					if(WIFEXITED(status))
						retval = WEXITSTATUS(status);
				}
				else if(pid == 0)
				{
					int i;
					for(i = 0; i < 3; i++)
						if(tasks[0].fd_redir[i]>2)
							dup2(tasks[0].fd_redir[i], i);
					execvp(tasks[0].args[0], tasks[0].args); //After this point, execvp must have failed
					fprintf(stderr, "Error: command not found\n");
					cleanup(&dir_st, tasks);
					exit(1);
				}
			}
			cleanup(NULL, tasks);
			fprintf(stderr, "+ completed '%s' [%d]\n", dudcmd, retval);
		}
	}
	return EXIT_SUCCESS;
}
