#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define CMDLINE_MAX 512

int main(void)
{
        char cmd[CMDLINE_MAX];

        while (1) {
                char *nl;
                int retval;
                char * args[20];
                int input_iter = 0;

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
                for( ; input_iter < 512; input_iter++)
                {
                        if(cmd[input_iter] == ' ')
                        {
                                cmd[input_iter] = '\0';
                                while(cmd[input_iter+1] == ' ') input_iter++;
                                if(cmd[input_iter+1] == '\0')
                                {
                                        args[arg_iter] = NULL;
                                        break;
                                }
                                args[arg_iter++] = cmd + input_iter + 1;

                        }
                        else if(cmd[input_iter] == '\0')
                        {
                                args[arg_iter] = NULL;
                                break;
                        }
                }

                int i = 0;
                while(args[i] != NULL)
                {
                        printf("%d: %s\n", i, args[i++]);
                }

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
            			execvp(args[0], args);
            		}
                fprintf(stdout, "Return status value for '%s': %d\n",
                        cmd, retval);
        }

        return EXIT_SUCCESS;
}
