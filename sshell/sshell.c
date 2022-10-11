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

void stack_resize(struct Stack *st)
{
	st->size_max *= 2;
	st->stack = realloc(st->stack, st->size_max*sizeof(char*));
}

void stack_push(struct Stack *st, char *str)
{
	if(st->size >= st->size_max) stack_resize(st);
	st->stack[st->size] = malloc(strlen(str)*sizeof(char));
	strcpy(st->stack[st->size++], str);
}

void stack_pop(struct Stack *st, char **str)
{
	*str = malloc(strlen(st->stack[--st->size])*sizeof(char)); 
	strcpy(*str, st->stack[st->size]);
	free(st->stack[st->size]);
}

void stack_print(struct Stack *st)
{
	int i;
	char cwd[PATH_MAX];
	getcwd(cwd, sizeof(cwd));
	printf("%s\n", cwd);
	for(i = st->size - 1; i >= 0; i--)
	{
		printf("%s\n", st->stack[i]);
	}
}

int main(void)
{
        char cmd[CMDLINE_MAX];
	char dudcmd[CMDLINE_MAX];
	struct Stack dir_st = {0, 2, malloc(2*sizeof(char*))};

        while(1) 
	{
                char *nl;
                int retval;
		struct Command *tasks;

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
                nl = strchr(cmd, '\n');
                if(nl)
                        *nl = '\0';
		strcpy(dudcmd, cmd);

		int j;
                int arg_iter = 1;
		int task_iter = 0;
		int input_iter = 0;
		int parse_error = 0;
		tasks = malloc(sizeof(struct Command));
		tasks[0].args[0] = cmd;
		for(j = 0; j < 3; j++) tasks[0].fd_redir[j] = j;
                for( ; input_iter < CMDLINE_MAX; input_iter++)
                {
			if(arg_iter >= 16)
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
                                int redir_type;
                                if(cmd[input_iter] == '<') redir_type = 0;
                                else if(cmd[input_iter-1] != '2') redir_type = 1;
                                else //stderr
                                {
                                        redir_type = 2;
                                        cmd[input_iter-1] = '\0';
                                }
				if(task_iter >= 1 && redir_type == 0) //compress these two error checks into a function
				{
					fprintf(stderr, "Error: mislocated input redirection");
					parse_error = TRUE;
					break;
				}
                                int file_flags = redir_type ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY;
                                int file_mode = redir_type ? 0644 : 0444;

                                cmd[input_iter] = '\0';
                                input_iter++;
                                while(cmd[input_iter] == ' ') input_iter++;
                                if(cmd[input_iter] == '\0' || cmd[input_iter] == '>' || cmd[input_iter] == '<' || cmd[input_iter] == '|')
                                {
                                        fprintf(stderr, "Error: no %sput file\n", redir_type ? "out" : "in");
					parse_error = TRUE;
                                        break;
                                }
                                char* filename = cmd + input_iter;
                                while(cmd[input_iter] != ' ' && cmd[input_iter] != '\0') input_iter++;
                                if(cmd[input_iter] == ' ')
                                {
					if(task_iter >= 1 && redir_type == 1)
					{
						fprintf(stderr, "Error: mislocated output redirection");
						parse_error = TRUE;
						break;
					}
                                        cmd[input_iter] = '\0';
                                        tasks[task_iter].fd_redir[redir_type] = open(filename, file_flags, file_mode);
                                        if(tasks[task_iter].fd_redir[redir_type]<0) 
                                        {
                                                fprintf(stderr, "Error: cannot open %sput file\n", redir_type ? "out" : "in");
						parse_error = TRUE;
                                                break;
                                        }
                                        tasks[task_iter].args[arg_iter-1] = cmd + input_iter;
                                }
                                else if(cmd[input_iter] == '\0')
                                {
                                        tasks[task_iter].fd_redir[redir_type] = open(filename, file_flags, file_mode);
                                        if(tasks[task_iter].fd_redir[redir_type]<0) 
                                        {
                                                fprintf(stderr, "Error: cannot open %sput file\n", redir_type ? "out" : "in");
						parse_error = TRUE;
                                                break;
                                        }
                                        tasks[task_iter].args[arg_iter-1] = NULL;
                                        break;
                                }
                        }
			else if(cmd[input_iter] == '|') //piping
			{
				/* Error catching */
				if(arg_iter <= 1)
				{
					fprintf(stderr, "Error: missing command\n");
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
			// else if(cmd[input_iter] == '<') //input redirect
			// {
			// 	cmd[input_iter] = '\0';
			// 	input_iter++;
			// 	while(cmd[input_iter] == ' ') input_iter++;
			// 	char* filename = cmd + input_iter;
			// 	while(cmd[input_iter] != ' ' && cmd[input_iter] != '\0') input_iter++;
			// 	if(cmd[input_iter] == ' ')
			// 	{
			// 		cmd[input_iter] = '\0';
			// 		fd_redir[0] = open(filename, O_RDONLY, 0444);
			// 		if(fd_redir[0]) is_redir[0] = 1; 
			// 		args[arg_iter-1] = cmd + input_iter;
			// 	}
			// 	else if(cmd[input_iter] == '\0')
			// 	{
			// 		fd_redir[0] = open(filename, O_RDONLY, 0444);
			// 		if(fd_redir[0]) is_redir[0] = 1;
			// 		args[arg_iter-1] = NULL;
			// 		break;
			// 	}
			// }
                        else if(cmd[input_iter] == '\0')
                        {
                                tasks[task_iter].args[arg_iter] = NULL;
                                break;
                        }
                }
		if(parse_error) continue;
                /*int i = 0;
                while(args[i] != NULL)
                {
                        printf("%d: %s\n", i, args[i++]);
                }*/
		if(task_iter >= 1) //piping present
		{
			int i;
			int retvals[task_iter+1];
			int pid[task_iter+1];
			for(i = 0; i < task_iter + 1; i++)
			{
				if((pid[i] = fork()) == 0)
				{
					int j;
					for(j = 0; j < 3; j++)
						if(tasks[i].fd_redir[j] > 2)
							dup2(tasks[i].fd_redir[j], j);
					execvp(tasks[i].args[0], tasks[i].args);
					fprintf(stderr, "Error: command not found\n");
					exit(1);
				}
				if(tasks[i].fd_redir[0] != 0)
					close(tasks[i].fd_redir[0]);
				if(tasks[i].fd_redir[1] != 1)
					close(tasks[i].fd_redir[1]);
			}
			for(i = 0; i < task_iter + 1; i++)
			{
				int status;
				//fprintf(stderr, "Waiting on PID %d\n", pid[i]);
				waitpid(pid[i], &status, 0);
				//if(tasks[i].fd_redir[1] != STDOUT_FILENO)
				//	close(tasks[i].fd_redir[1]);
				//if(tasks[i].fd_redir[0] != STDIN_FILENO)
				//	close(tasks[i].fd_redir[0]);
				if(WIFEXITED(status))
				{
					retvals[i] = WEXITSTATUS(status);
				//	fprintf(stderr, "PID %d exited with code %d\n", pid[i], retvals[i]);
				}
			}
			fprintf(stderr, "+ completed '%s' ", dudcmd);
			for(i = 0; i < task_iter+1; i++) fprintf(stderr, "[%d]", retvals[i]);
			fprintf(stderr, "\n");
			continue;
		}
		else
		{
                	/* Builtin command */
			if(!strcmp(cmd, "exit")) 
			{
				fprintf(stderr, "Bye...\n");
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
			else if(!strcmp(tasks[0].args[0], "pushd")) // DOES NOT CHECK FOR ERRORS (same goes for all three stack commands)
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
					fprintf(stderr, "Error: directory stack empty\n");
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
			else
			{
				/* Regular command */
				int pid = fork();
				//printf("PID: %d\n", pid);
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
					exit(1);
				}
			}
			fprintf(stdout, "Return status value for '%s': %d\n", dudcmd, retval);
		}
        }

        return EXIT_SUCCESS;
}
