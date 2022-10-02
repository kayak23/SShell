#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 20

int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;
                int retval;
                char * args[MAX_ARGS];
                int input_iter = 0;
		int is_redir[3] = {0, 0, 0};
		int fd_redir[3] = {-1, -1, -1};

                /* Print prompt */
                printf("sshell$ ");
                fflush(stdout);

                /* Get command line */
                fgets(cmd, CMDLINE_MAX, stdin);

                /* Print command line if stdin is not provided by terminal */
                if (!isatty(STDIN_FILENO))
                {
                        printf("%s", cmd);
                        fflush(stdout);
                }

                /* Remove trailing newline from command line */
                nl = strchr(cmd, '\n');
                if (nl)
                        *nl = '\0';

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
                        else if(cmd[input_iter] == '>') //output redirect
                        {
                                if(cmd[input_iter-1] == '2'); //std error
                                input_iter++;
                                while(cmd[input_iter] == ' ') input_iter++;
                                char* filename = cmd + input_iter;
                                while(cmd[input_iter] != ' ' && cmd[input_iter] != '\0') input_iter++;
                                if(cmd[input_iter] == ' ')
                                {
                                        cmd[input_iter] = '\0';
                                        fd_redir[1] = open(filename, O_WRONLY);
                                        if(fd_redir[1]) is_redir[1] = 1;
                                }
                                else if(cmd[input_iter] == '\0')
                                {
                                        fd_redir[1] = open(filename, O_WRONLY);
                                        if(fd_redir[1]) is_redir[1] = 1;
                                }
                        }
                        
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
                if (!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        break;
                }

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
			for(i = 0; i < 3; i++) 
				if(is_redir[i])
					dup2(fd_redir, i);
            		execvp(args[0], args);
            	}
                fprintf(stdout, "Return status value for '%s': %d\n",
                        cmd, retval);
        }

        return EXIT_SUCCESS;
}
