# SSHELL: A Simple Shell
##### Authors: _Ryan Stear, SID: 917196751; Pranav Kharche, SID: 918322472_

## Overview
The program SSHELL provides the basic functionality of a Shell: a service that
interprets input from the user in the form of command-line arguments and
executes them. Additionally, SSHELL provides a variety of essential built-in
commands for managing the user's working environment, supports the use of
common meta-characters for managing input and output redirection, and maintains
a stable runtime environment by properly handling errors.

### Architecture
The implementation of SSHELL adheres to this general architecture:
1. The program prompts the user for input.
2. The program parses the user's input.
3. The program executes the user's input and prints a completion message
containing the exit code(s) for the program(s) specified in the user input.
4. The program prompts the user for input and repeats from step 2).

## Implementation

### Initialization and Parsing
Upon launching the program, the user is greeted by the SSHELL prompt, indicating
to the user that the program is ready to receive input. However, this input
must be parsed by SSHELL. User input falls into two general categories:
* The user has issued a single command (single program).
* The user has issued a pipeline of commands (multiple programs).
In each of these categories, SSHELL identifies the individual programs and their
respective arguments, opens all necessary files for input and output
redirection and piping, and then loads each program into a data structure
called "struct Command" (henceforth referred to as a 'Command Structure'). In
the case of a pipeline, a dynamically allocated array of such Command
Structures is used.

Command Structures have two main components:
* A statically allocated array of character pointers with sufficient size for
the maximum number of arguments of an individual program. (called 'args')
* A statically allocated array of integers of size three to store the input,
output, and error file descriptors for the program. (called 'fd_redir';
initialized to be {0, 1, 2} for the default file descriptors of standard input,
standard output, and standard error)

The elements of the first field, the pointer array, are set to the address of
the first character of their corresponding argument in the command line (e.g.
args[0] is the program itself, args[1] is the first argument, and so on). The
first blank space after an un-interrupted sequence of characters is set to be a
null-byte to terminate each string in 'args'.

Upon identifying a meta-character (>, <, or |), the parser recognizes the
immediately following argument to be either a file or new program, for
input/output redirection and piping, respectively. In the case of input/output
redirection, SSHELL will attempt to open the file specified by the following
argument, and set the appropriate element of 'fd_redir' to the value of the new
file descriptor. In the case of the pipe, a new element is created in the
Command Structure array, and a pipe is created using the pipe() system call.
The output file descriptor of the current task is set to the 'write' end of the
pipe and the input file descriptor of the new task is set to the 'read' end of
the pipe.

### Executing User Input
As mentioned before, there are two general categories for user input: either the
user has issued a single command or the user has issued a pipeline of commands.
SSHELL handles each of these cases separately.

In the case of the user issuing a single command, our program first checks
whether the issued input is supported as a built-in command (see Built-In
Commands). If the command is built-in, the SSHELL simply executes the required
functions and system calls. If the command is not built-in, the SSHELL fork()s;
the child process executes the dup2() system call on each file descriptor stored
in 'fd_redir' that is not equal to the default values and then executes the
execvp() system call using 'args[0]' and 'args' as the two parameters. This
replaces the child process with the program specified by 'args[0]'. The input of
this process is read from the file descriptor specified by 'fd_redir[0]' and
the output of this process is written to the file descriptor specified by
'fd_redir[1]'. The parent process simply waits for the completion of the child
process and collects its exit status. The parent then prints a completion
message including the collected exit status, after which the parent (the shell)
again prompts the user for more input.

In the case of the user issuing a pipeline of commands, our program immediately
enters a loop to fork() N times to generate N child processes, where N is the
number of programs in the pipeline. Each child process executes the dup2()
system call as in the case of the single command. This time, the children must
additionally close all pipe file descriptors. After this has been performed,
each child process executes the execvp() system call as in the case of the
single command. The parent process (the shell), now enters a second loop,
waiting sequentially for each child process to either exit(), return from
main(), or receive a SIGPIPE termination signal, indicating that the pipe from
which the child is reading input has reached EOF. After each process has
completed and each exit status has been collected, the shell prints a completion
message including the collected exit codes for each program in the pipeline.

### Built-In Commands
The following built-in commands are provided in SSHELL:
1. exit: Exits the shell. This command is implemented by simply printing
"Bye..." and breaking the input-prompt loop of the shell.
2. cd <dir>: Changes the current working directory to <dir>. This command is
implemented by executing the chdir() system call on the specified directory.
3. pwd: Prints the current working directory. This command is implemented by
executing the getcwd() system call and printing the output.
4. pushd <dir>: Pushes the current working directory into the directory stack
(see Notes on the Directory Stack) and changes the current working directory to
<dir>. This command is implemented by calling the SSHELL function stack_push()
and executing the chdir() system call.
5. popd: Pops the top directory from the stack and changes the current working
directory to this directory. This command is implemented by calling the SSHELL
function stack_pop() and executing the chdir() system call on its output.
6. dirs: Prints the contents of the stack, starting with the current working
directory. This command is implemented by executing the getcwd() system call,
printing the output, and then printing each of the elements of the directory
stack.

### Notes on the Directory Stack
To implement the directory stack, SSHELL includes a data structure called
"struct Stack" (henceforth referred to as a 'Stack') to manage the storing of
directories. A Stack has the following data members:
* An integer value for the size of the stack (called 'size')
* An integer value for the maximum size of the stack (called 'size_max')
* A double character pointer to store the directories as strings (called
'stack')
The Stack is also supported by four "member functions":
1. stack_resize(): doubles the maximum size of the stack if the current 'size'
is greater than or equal to the 'size_max'.
2. stack_push(): pushes a new string into the stack (dynamically allocates
enough space for the string in the 'stack' data member)
3. stack_pop(): pops the top element of the stack
4. stack_print(): prints the elements of the stack from top to bottom

## Error Catching

### Parsing Errors
During the process of parsing the command line, SSHELL also checks for illegal
input syntax. There are six possibilities for illegal syntax:
1. *The user has entered too many process arguments.*
This is detected by checking the number of arguements during every loop. The
variable arg_iter will become too big with too many arguements. There is a
special case to check if the extra arguements are actually redirects or pipes
2. *The input is missing a command.*
If a meta character or end of line is encountered and the number of arguements
has not increased, then there is no command given as the first arguement. 
3. *An input or output redirection meta-character is missing a file.*
After seeing the file redirect meta character, the program iterates through
spaces to find the start of the filename. If the first character is a meta
character or \0, then there was no filename given.
4. *A specified file cannot be opened.*
This error only happens if the 'open' syscall fails and returns a negative
value. It does not check why it failed but simply stops parsing and prompts the
user again.
5. *An input redirection meta-character has been improperly placed in a
pipeline.*
This is found if an input redirect is found and a pipe is present.
6. *An output redirection meta-character has been improperly placed in a
pipeline.*
This is found if a pipe is found and an output redirect is present.

### Delete this ??
To handle syntax discrepancies 1), 2), and 5), our program maintains an integer
value representing the total number of arguments for the current
program and an integer value representing the total number of Command Structures
created from the user input (henceforth referred to as 'arg_iter' and
'task_iter', respectively, and initialized to one and zero, respectively).
This catches all of the aforementioned potential syntax discrepancies:
* The parser errs if 'arg_iter' exceeds the maximum number of allowed arguments
* The parser errs if a meta-character is found before 'arg_iter' is greater than
one.
* The parser errs if an input redirection meta character was encountered when
'task_iter' is greater than or equal to one.

### Launching Errors
There are 2 types of launching errors that SSHELL manages:
1. built-in command fails
2. user command fails to exec.
Built in command errors are managed within the code of the command. They check
for the error, print the error message, and set the appropriate return value.
cd and pushd catch an error when 'chdir' fails and returns a bad value. popd
errors when it checks the size of the directory stack and finds that it is
empty.
User commands fails to execute if the command does not exist on the system. In
this case, the 'execvp' function will return rather than change the process.
When that happens, the program will print the error message and then exit with
return value 1. Since this happens in a child process, the shell is preserved
