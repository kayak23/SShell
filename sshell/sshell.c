#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

#define CMDLINE_MAX 512
#define MAX_ARGS 20
#define PATH_MAX 4096

int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;
                int retval;
                char * args[MAX_ARGS];
                int input_iter = 0;
            		int is_redir[3] = {0, 0, 0};
            		int fd_redir[3] = {0, 1, 2};

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
                                //if(cmd[input_iter-1] == '2'); //std error
                                cmd[input_iter] = '\0';
                                input_iter++;
                                while(cmd[input_iter] == ' ') input_iter++;
                                char* filename = cmd + input_iter;
                                while(cmd[input_iter] != ' ' && cmd[input_iter] != '\0') input_iter++;
                                if(cmd[input_iter] == ' ')
                                {
                                        cmd[input_iter] = '\0';
                                        fd_redir[1] = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                        if(fd_redir[1]) is_redir[1] = 1;
                                        args[arg_iter-1] = cmd + input_iter;
                                }
                                else if(cmd[input_iter] == '\0')
                                {
                                        fd_redir[1] = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                                        if(fd_redir[1]) is_redir[1] = 1;
                                        args[arg_iter-1] = NULL;
                                        break;
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
                if(!strcmp(cmd, "exit")) {
                        fprintf(stderr, "Bye...\n");
                        break;
                }
            		else if(!strcmp(args[0], "pwd"))
            		{
            			char cwd[PATH_MAX];
            			if(getcwd(cwd, sizeof(cwd)) == NULL)
            				retval = 1;
            			else
            			{
            				dprintf(fd_redir[1], "%s\n", cwd);
            				retval = 0;
            			}
            		}
            		else if(!strcmp(args[0], "cd"))
            			retval = abs(chdir(args[1]));
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
            					if(is_redir[i])
            						dup2(fd_redir[i], i);
                        			execvp(args[0], args);
            				exit(1);
                		}
            		}
                            fprintf(stdout, "Return status value for '%s': %d\n",
                                    cmd, retval);
        }

        return EXIT_SUCCESS;
}
