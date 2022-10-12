# SSHELL: A Simple Shell
##### Authors: _Ryan Stear, SID: 917196751; Pranav Kharche, SID: 918322472_

## Overview
The program sshell provides the basic functionality of a Shell: a service that
interprets input from the user in the form of command-line arguments and
executes them. Additionally, this program provides a variety of essential
built-in commands for managing the user's working environment, supports the use
of common meta-characters for managing input and output redirection, and
maintains a stable runtime environment by properly handling errors.

### Architecture
The implementation of sshell adheres to this general architecture:
1. The program prompts the user for input.
2. The program parses the user's input and checks for invalid input.
3. The program executes the user's input and prints a completion message
containing the exit code(s) for the program(s) specified in the user input.
4. The program prompts the user for input and repeats from step 1).

## Implementation

### Initialization and Parsing
Upon launching the program, the user is greeted by the sshell prompt, indicating
to the user that the program is ready to receive input. However, this input
must be parsed by sshell. User input falls into two general categories:
* The user has issued a single command (single program).
* The user has issued a pipeline of commands (multiple programs).
In each of these categories, sshell identifies the individual programs and their
respective arguments, creates all necessary file descriptors for input and
output redirection and piping, and then loads each program into a data structure
called "struct Command" (henceforth referred to as a 'Command Structure').
Command Structures have two main components:
* A statically allocated character array with sufficient size for the maximum
number of arguments of an individual program.
* A statically allocated integer array of size three to store the input, output,
and error file descriptors for the program.
In the case of a pipeline of commands, our program generates a dynamically
allocated array of Command Structures to organize the pipeline.
During the process of parsing the command line, sshell also checks for illegal
input syntax. There are six possibilities for illegal syntax:
1. The user has entered too many process arguments.
2. A meta-character is missing a command.
3. An input or output redirection meta-character is missing a file.
4. A specified file cannot be opened.
5. An input redirection meta-character has been improperly placed in a
pipeline.
6. An output redirection meta-character has been improperly placed in a
pipeline.
To handle these syntax discrepancies 1), 2), and 5), our program maintains an
integer value representing the total number of arguments for the current
program and an integer value representing the total number of Command Structures
created from the user input (henceforth referred to as 'arg_iter' and
'task_iter', respectively, and initialized to one and zero, respectively).
This catches all of the aforementioned potential syntax discrepancies:
* The parser errs if 'arg_iter' exceeds the maximum number of allowed arguments
* The parser errs if a meta-character is found before 'arg_iter' is greater than
one.
* The parser errs if an input redirection meta character was encountered when
'task_iter' is greater than or equal to one.
