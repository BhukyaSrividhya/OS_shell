/********************************************************************************************
						   OS Lab Assignment - BT23CSE031

This is a template for assignment on writing a custom Shell.

Students may change the return types and arguments of the functions given in this template,
but do not change the names of these functions.

Though use of any extra functions is not recommended, students may use new functions if they need to,
but that should not make code unnecessorily complex to read.

Students should keep names of declared variable (and any new functions) self explanatory,
and add proper comments for every logical step.

Students need to be careful while forking a new process (no unnecessory process creations)
or while inserting the single handler code (should be added at the correct places).

Finally, keep your filename as myshell.c, do not change this name (not even myshell.cpp,
as you not need to use any features for this assignment that are supported by C++ but not by C).
*********************************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>	  // exit()
#include <unistd.h>	  // fork(), getpid(), exec()
#include <sys/wait.h> // wait()
#include <signal.h>	  // signal()
#include <fcntl.h>	  // close(), open()

// Global state management for shell operations
typedef struct
{
	char **instruction_list;
	int instruction_total;
	char redirect_target[512];
	int operation_mode;
} ShellState;

ShellState shell_context;
char *user_command = NULL;

// Utility to extract words from command string
char **extractWords(char *source, int *word_count)
{
	char **word_array = malloc(sizeof(char *) * 50);
	*word_count = 0;
	char *temp_source = strdup(source);
	char *current_word;

	while ((current_word = strsep(&temp_source, " \t\n")) != NULL)
	{
		// Skip empty strings
		if (*current_word != '\0')
		{
			word_array[(*word_count)++] = strdup(current_word);
		}
	}
	word_array[*word_count] = NULL;
	free(temp_source);
	return word_array;
}

// Clean allocated word arrays
void cleanWordArray(char **words, int count)
{
	for (int idx = 0; idx < count; idx++)
	{
		if (words[idx])
			free(words[idx]);
	}
	free(words);
}

void parseInput()
{
	// This function will parse the input string into multiple commands or a single command with arguments depending on the delimiter (&&, ##, >, or spaces).

	// Initialize shell context
	shell_context.instruction_total = 0;
	shell_context.operation_mode = 0; // 0=single, 1=parallel, 2=sequential, 3=redirect
	memset(shell_context.redirect_target, 0, sizeof(shell_context.redirect_target));

	if (shell_context.instruction_list)
	{
		for (int idx = 0; idx < 50; idx++)
		{
			if (shell_context.instruction_list[idx])
			{
				free(shell_context.instruction_list[idx]);
			}
		}
		free(shell_context.instruction_list);
	}

	shell_context.instruction_list = malloc(sizeof(char *) * 50);
	for (int idx = 0; idx < 50; idx++)
	{
		shell_context.instruction_list[idx] = NULL;
	}

	if (!user_command || strlen(user_command) == 0)
		return;

	char *work_copy = strdup(user_command);

	// Detect redirection pattern
	char *redirect_pos = strchr(work_copy, '>');
	if (redirect_pos)
	{
		shell_context.operation_mode = 3;
		*redirect_pos = '\0';
		shell_context.instruction_list[0] = strdup(work_copy);
		shell_context.instruction_total = 1;

		char *file_name = redirect_pos + 1;
		while (*file_name == ' ' || *file_name == '\t')
			file_name++;
		strcpy(shell_context.redirect_target, file_name);

		// Remove trailing spaces from filename
		int len = strlen(shell_context.redirect_target);
		while (len > 0 && (shell_context.redirect_target[len - 1] == ' ' ||
						   shell_context.redirect_target[len - 1] == '\t' ||
						   shell_context.redirect_target[len - 1] == '\n'))
		{
			shell_context.redirect_target[--len] = '\0';
		}
	}
	// Detect parallel pattern
	else if (strstr(work_copy, "&&"))
	{
		shell_context.operation_mode = 1;
		char *remaining = work_copy;
		char *segment;

		while ((segment = strsep(&remaining, "&")) != NULL)
		{
			if (strlen(segment) > 0 && shell_context.instruction_total < 50)
			{
				shell_context.instruction_list[shell_context.instruction_total++] = strdup(segment);
			}
		}
	}
	// Detect sequential pattern
	else if (strstr(work_copy, "##"))
	{
		shell_context.operation_mode = 2;
		char *remaining = work_copy;
		char *segment;

		while ((segment = strsep(&remaining, "#")) != NULL)
		{
			if (strlen(segment) > 0 && shell_context.instruction_total < 50)
			{
				shell_context.instruction_list[shell_context.instruction_total++] = strdup(segment);
			}
		}
	}
	// Single command mode
	else
	{
		shell_context.operation_mode = 0;
		shell_context.instruction_list[0] = strdup(work_copy);
		shell_context.instruction_total = 1;
	}

	free(work_copy);
}

void executeCommand()
{
	// This function will fork a new process to execute a command

	if (shell_context.instruction_total == 0 || !shell_context.instruction_list[0])
		return;

	int word_count;
	char **words = extractWords(shell_context.instruction_list[0], &word_count);

	if (word_count == 0)
	{
		cleanWordArray(words, word_count);
		return;
	}

	// Handle directory change operation
	if (strcmp(words[0], "cd") == 0)
	{
		char *target_path = (word_count > 1) ? words[1] : getenv("HOME");
		int result = chdir(target_path);
		if (result != 0)
			perror("cd");
		cleanWordArray(words, word_count);
		return;
	}

	// Create child process for command execution
	pid_t child_process = fork();

	if (child_process == 0)
	{
		// Child: restore signal defaults and execute
		signal(SIGINT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);

		int exec_result = execvp(words[0], words);
		if (exec_result == -1)
		{
			printf("Shell: Incorrect command\n");
			exit(EXIT_FAILURE);
		}
	}
	else if (child_process > 0)
	{
		// Parent: wait for completion
		int status;
		waitpid(child_process, &status, 0);
	}

	cleanWordArray(words, word_count);
}

void executeParallelCommands()
{
	// This function will run multiple commands in parallel

	if (shell_context.instruction_total == 0)
		return;

	pid_t process_pool[50];
	int active_count = 0;

	// Launch all instructions simultaneously
	for (int idx = 0; idx < shell_context.instruction_total; idx++)
	{
		if (!shell_context.instruction_list[idx])
			continue;

		int word_count;
		char **words = extractWords(shell_context.instruction_list[idx], &word_count);

		if (word_count == 0)
		{
			cleanWordArray(words, word_count);
			continue;
		}

		// Directory changes executed in parent context
		if (strcmp(words[0], "cd") == 0)
		{
			char *target_path = (word_count > 1) ? words[1] : getenv("HOME");
			int result = chdir(target_path);
			if (result != 0)
				perror("cd");
			cleanWordArray(words, word_count);
			continue;
		}

		pid_t proc_id = fork();
		if (proc_id == 0)
		{
			// Child process setup
			signal(SIGINT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);

			int exec_result = execvp(words[0], words);
			if (exec_result == -1)
			{
				printf("Shell: Incorrect command\n");
				exit(EXIT_FAILURE);
			}
		}
		else if (proc_id > 0)
		{
			process_pool[active_count++] = proc_id;
		}

		cleanWordArray(words, word_count);
	}

	// Synchronize all parallel processes
	for (int idx = 0; idx < active_count; idx++)
	{
		int status;
		waitpid(process_pool[idx], &status, 0);
	}
}

void executeSequentialCommands()
{
	// This function will run multiple commands sequentially

	if (shell_context.instruction_total == 0)
		return;

	// Process each instruction in order
	for (int idx = 0; idx < shell_context.instruction_total; idx++)
	{
		if (!shell_context.instruction_list[idx])
			continue;

		int word_count;
		char **words = extractWords(shell_context.instruction_list[idx], &word_count);

		if (word_count == 0)
		{
			cleanWordArray(words, word_count);
			continue;
		}

		// Handle directory navigation
		if (strcmp(words[0], "cd") == 0)
		{
			char *target_path = (word_count > 1) ? words[1] : getenv("HOME");
			int result = chdir(target_path);
			if (result != 0)
				perror("cd");
			cleanWordArray(words, word_count);
			continue;
		}

		pid_t proc_id = fork();
		if (proc_id == 0)
		{
			// Child execution environment
			signal(SIGINT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);

			int exec_result = execvp(words[0], words);
			if (exec_result == -1)
			{
				printf("Shell: Incorrect command\n");
				exit(EXIT_FAILURE);
			}
		}
		else if (proc_id > 0)
		{
			// Wait for current command before proceeding
			int status;
			waitpid(proc_id, &status, 0);
		}

		cleanWordArray(words, word_count);
	}
}

void executeCommandRedirection()
{
	// This function will run a single command with output redirected to an output file specified by user

	if (shell_context.instruction_total == 0 || !shell_context.instruction_list[0] ||
		strlen(shell_context.redirect_target) == 0)
		return;

	int word_count;
	char **words = extractWords(shell_context.instruction_list[0], &word_count);

	if (word_count == 0)
	{
		cleanWordArray(words, word_count);
		return;
	}

	// Directory changes don't need redirection
	if (strcmp(words[0], "cd") == 0)
	{
		char *target_path = (word_count > 1) ? words[1] : getenv("HOME");
		int result = chdir(target_path);
		if (result != 0)
			perror("cd");
		cleanWordArray(words, word_count);
		return;
	}

	pid_t proc_id = fork();
	if (proc_id == 0)
	{
		// Child: setup redirection and execute
		signal(SIGINT, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);

		// Open target file for output redirection
		int output_descriptor = open(shell_context.redirect_target,
									 O_WRONLY | O_CREAT | O_TRUNC, 0644);
		if (output_descriptor < 0)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}

		// Redirect stdout to file
		dup2(output_descriptor, STDOUT_FILENO);
		close(output_descriptor);

		int exec_result = execvp(words[0], words);
		if (exec_result == -1)
		{
			printf("Shell: Incorrect command\n");
			exit(EXIT_FAILURE);
		}
	}
	else if (proc_id > 0)
	{
		int status;
		waitpid(proc_id, &status, 0);
	}

	cleanWordArray(words, word_count);
}

int main()
{
	// Initial declarations
	char working_directory[1024];
	size_t buffer_capacity = 0;

	// Initialize shell context
	shell_context.instruction_list = NULL;

	// Configure signal handling for shell process
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);

	while (1) // This loop will keep your shell running until user exits.
	{
		// Print the prompt in format - currentWorkingDirectory$
		char *dir_result = getcwd(working_directory, sizeof(working_directory));
		if (dir_result)
		{
			printf("%s$", working_directory);
		}
		else
		{
			printf("myshell$");
		}

		// accept input with 'getline()'
		ssize_t input_length = getline(&user_command, &buffer_capacity, stdin);
		if (input_length == -1)
		{
			printf("Exiting shell...\n");
			break;
		}

		// Strip trailing newline character
		if (input_length > 0 && user_command[input_length - 1] == '\n')
		{
			user_command[input_length - 1] = '\0';
		}

		// Skip empty input lines
		if (strlen(user_command) == 0)
			continue;

		// Parse input with 'strsep()' for different symbols (&&, ##, >) and for spaces.
		parseInput();

		if (shell_context.instruction_total > 0 && shell_context.instruction_list[0]) // When user uses exit command.
		{
			int word_count;
			char **words = extractWords(shell_context.instruction_list[0], &word_count);

			if (word_count > 0 && strcmp(words[0], "exit") == 0)
			{
				cleanWordArray(words, word_count);
				printf("Exiting shell...\n");
				break;
			}
			cleanWordArray(words, word_count);
		}

		if (shell_context.operation_mode == 1)
			executeParallelCommands(); // This function is invoked when user wants to run multiple commands in parallel (commands separated by &&)
		else if (shell_context.operation_mode == 2)
			executeSequentialCommands(); // This function is invoked when user wants to run multiple commands sequentially (commands separated by ##)
		else if (shell_context.operation_mode == 3)
			executeCommandRedirection(); // This function is invoked when user wants redirect output of a single command to and output file specified by user
		else
			executeCommand(); // This function is invoked when user wants to run a single commands
	}

	// Final cleanup
	if (user_command)
		free(user_command);
	if (shell_context.instruction_list)
	{
		for (int idx = 0; idx < 50; idx++)
		{
			if (shell_context.instruction_list[idx])
				free(shell_context.instruction_list[idx]);
		}
		free(shell_context.instruction_list);
	}

	return 0;
}