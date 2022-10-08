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

struct command
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
                char *args[MAX_ARGS+1];
                int input_iter = 0;
            	// int is_redir[3] = {0, 0, 0};
            	int fd_redir[3] = {0, 1, 2};

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

                args[0] = cmd;
                int arg_iter = 1;
                for( ; input_iter < CMDLINE_MAX; input_iter++)
                {
                        if(cmd[input_iter] == ' ') //current arguement is complete
                        {
                                cmd[input_iter] = '\0';
                                while(cmd[input_iter+1] == ' ') input_iter++; //check for extra spaces
                                if(cmd[input_iter+1] == '\0')
                                {
                                        args[arg_iter] = NULL;
                                        break;
                                }
                                args[arg_iter++] = cmd + input_iter + 1;

                        }
                        else if(cmd[input_iter] == '>' || cmd[input_iter] == '<') //file redirect
                        {
                                if(arg_iter <= 1) 
                                {
                                        fprintf(stderr, "Error: missing command\n");
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
                                int file_flags = redir_type ? (O_WRONLY | O_CREAT | O_TRUNC) : O_RDONLY;
                                int file_mode = redir_type ? 0644 : 0444;

                                cmd[input_iter] = '\0';
                                input_iter++;
                                while(cmd[input_iter] == ' ') input_iter++;
                                if(cmd[input_iter] == '\0' || cmd[input_iter] == '>' || cmd[input_iter] == '<' || cmd[input_iter] == '|')
                                {
                                        fprintf(stderr, "Error: no %sput file\n", redir_type ? "out" : "in");
                                        break;
                                }
                                char* filename = cmd + input_iter;
                                while(cmd[input_iter] != ' ' && cmd[input_iter] != '\0') input_iter++;
                                if(cmd[input_iter] == ' ')
                                {
                                        cmd[input_iter] = '\0';
                                        fd_redir[redir_type] = open(filename, file_flags, file_mode);
                                        if(fd_redir[redir_type]<0) 
                                        {
                                                fprintf(stderr, "Error: cannot open %sput file\n", redir_type ? "out" : "in");
                                                break;
                                        }
                                        args[arg_iter-1] = cmd + input_iter;
                                }
                                else if(cmd[input_iter] == '\0')
                                {
                                        fd_redir[redir_type] = open(filename, file_flags, file_mode);
                                        if(fd_redir[redir_type]<0) 
                                        {
                                                fprintf(stderr, "Error: cannot open %sput file\n", redir_type ? "out" : "in");
                                                break;
                                        }
                                        args[arg_iter-1] = NULL;
                                        break;
                                }
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
                                args[arg_iter] = NULL;
                                break;
                        }
                }

                /*int i = 0;
                while(args[i] != NULL)
                {
                        printf("%d: %s\n", i, args[i++]);
                }*/

                /* Builtin command */
                if(!strcmp(cmd, "exit")) 
                {
                        fprintf(stderr, "Bye...\n");
                        break;
                }
            	else if(!strcmp(args[0], "pwd"))
            	{
            		char *cwd = getcwd(NULL, 0);
            		if(cwd == NULL)
			{
            			printf("error\n");
				retval = 1;
			}
            		else
            		{
            		        dprintf((fd_redir[1]>0 ? fd_redir[1] : 1),"%s\n", cwd);
            			retval = 0;
            		}
			free(cwd);
            	}
            	else if(!strcmp(args[0], "cd"))
		{
            		retval = abs(chdir(args[1]));
		}
		else if(!strcmp(args[0], "pushd")) // DOES NOT CHECK FOR ERRORS (same goes for all three stack commands)
		{
			char *cwd = getcwd(NULL, 0);
			stack_push(&dir_st, cwd);
			retval = abs(chdir(args[1]));
			free(cwd);
		}
		else if(!strcmp(args[0], "popd"))
		{
			char *dir = NULL;
			stack_pop(&dir_st, &dir);
			retval = abs(chdir(dir));
			free(dir);
		}
		else if(!strcmp(args[0], "dirs"))
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
              			else
              				retval = 1;
              		}
              		else if(pid == 0)
              		{
            			int i;
            			for(i = 0; i < 3; i++)
            				if(fd_redir[i]>2)
            					dup2(fd_redir[i], i);
                        	execvp(args[0], args);
            			exit(1);
                	}
            	}
                fprintf(stdout, "Return status value for '%s': %d\n", dudcmd, retval);
        }

        return EXIT_SUCCESS;
}
