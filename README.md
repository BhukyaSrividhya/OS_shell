# OS_shell
🐚 Custom Command Shell in C
📌 Project Description

This project implements a custom Linux command shell written in C, designed to replicate essential functionalities of a Unix shell. The shell continuously accepts user input, parses commands, manages processes, and executes system programs using low-level OS system calls.

The implementation focuses on process control, signal handling, command parsing, and I/O redirection, while keeping the code modular, readable, and efficient.

🔧 Key Functionalities Implemented
1️⃣ Interactive Shell Loop

The shell runs inside an infinite loop and terminates only when the user enters the exit command.

Before each command, the shell prints a prompt showing the current working directory followed by $.

User input is read using getline() to safely handle variable-length input.

2️⃣ Command Parsing

The input line is analyzed to determine the execution mode:

Single command

Parallel execution (&&)

Sequential execution (##)

Output redirection (>)

Parsing is handled using strsep() to split commands and arguments while avoiding complex parsing logic.

Parsed commands are stored in a centralized ShellState structure, which maintains:

List of commands

Total number of commands

Execution mode

Output redirection target (if any)

3️⃣ Command Execution

External commands are executed by:

Creating a child process using fork()

Replacing the child process image using execvp()

Synchronizing using waitpid()

The parent process waits for the child to complete before continuing (except in parallel execution).

4️⃣ Built-in cd Command

The cd command is handled inside the shell process (not using exec) since it modifies the shell’s working directory.

Supports:

cd <path>

cd ..

cd (defaults to HOME directory)

Implemented using the chdir() system call.

5️⃣ Parallel Command Execution (&&)

Commands separated by && are executed simultaneously.

Each command is executed in its own child process.

The shell tracks all spawned processes and waits for all of them to finish before accepting new input.

cd commands are executed in the parent process to ensure directory changes persist.

6️⃣ Sequential Command Execution (##)

Commands separated by ## are executed one after another.

The shell waits for each command to complete before starting the next.

Supports mixing normal commands and cd commands within the sequence.

7️⃣ Output Redirection (>)

Supports redirecting standard output of a command to a file.

Implementation steps:

Open or create the target file using open()

Redirect STDOUT to the file using dup2()

Execute the command using execvp()

Only standard output is redirected; error output remains unchanged.

8️⃣ Signal Handling

The shell process ignores:

SIGINT (Ctrl + C)

SIGTSTP (Ctrl + Z)

Child processes restore default signal behavior, allowing them to be interrupted or stopped by the user.

This ensures the shell itself remains active while child commands respond normally to signals.

9️⃣ Error Handling

Invalid or unsupported commands print:

Shell: Incorrect command


Execution-time errors (e.g., file not found) are handled by the underlying system and displayed naturally.

Empty input lines are safely ignored.

🧠 Design Choices

Used modular functions for each execution mode (single, parallel, sequential, redirection).

Avoided unnecessary process creation to improve efficiency.

Maintained a clear separation between parsing, execution, and signal handling.

Focused on readability and maintainability with descriptive variable names and comments.
