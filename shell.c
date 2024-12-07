#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SH_READ_LINE_BUFSIZE 1024

#define SH_TOK_BUFSIZE 64
#define SH_TOK_DELIM " \t\r\n\a"

/*
 * Function Declarations
 */
void sh_loop(void);
int sh_exit(char **args);
char *sh_read_line(void);
char **sh_split_line(char *line);
char **sh_split_cmd(char *cmd);
int sh_execute(char **args);

int main(int argc, char **argv)
{
	sh_loop();

	return EXIT_SUCCESS;
}

void sh_loop(void)
{
	char *line;
	char **cmds;
	char **args;
	int status;

	do {
		printf("$ ");

		line = sh_read_line();
		cmds = sh_split_line();

		do {
			args = sh_split_cmd(cmd);
			status = sh_execute(args);

			free(args);
		} while (status);

		free(line);
		free(cmds);
	} while (status);
}

char *sh_read_line(void)
{
	char *line = NULL;
	ssize_t bufsize = 0;

	if (getline(&line, &bufsize, stdin) == -1) {
		if (feof(stdin)) {
			exit(EXIT_SUCCESS);
		} else {
			perror("readline");
			exit(EXIT_FAILURE);
		}
	}

	return line;
}

char **sh_split_line(char *line)
{
	int bufsize = SH_TOK_BUFSIZE, position = 0;
	char **cmds = malloc(bufsize * sizeof(char *));
	char *cmd;

	if (!cmds) {
		fprintf(stderr, "sh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	int j = 0;
	for (int i = 0; line[i] != '\0'; i++) {
		if (line[i] == '>') {
			
		} else if (line[i] == '<') {

		} else if (line[i] == '|') {

		}

		strncpy(cmd, line + j, i - j);
		cmds[i] = cmd;
	}
}

char **sh_split_cmd(char *cmd)
{
	int bufsize = SH_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char *));
	char *token;

	if (!tokens) {
		fprintf(stderr, "sh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(cmd, SH_TOK_DELIM);
	while (token != NULL) {
		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += SH_TOK_BUFSIZE;
			tokens = realloc(tokens, bufsize * sizeof(char *));
			if (!tokens) {
				fprintf(stderr, "sh: allocation error\n");
				exit(EXIT_FAILURE);
			}
		}

		token = strtok(NULL, SH_TOK_DELIM);
	}

	tokens[position] = NULL;
	return tokens;
}

int sh_launch(char **args)
{
	pid_t pid, wpid;
	int status;

	pid = fork();
	if (pid == 0) {
		// Child process
		if (execvp(args[0], args) == -1) {
			perror("sh");
		}
		exit(EXIT_FAILURE);
	} else if (pid < 0) {
		// Error forking
		perror("sh");
	} else {
		// Parent process
		do {
			wpid = waitpid(pid, &status, WUNTRACED);
		} while (!WIFEXITED(status) && !WIFSIGNALED(status));
	}

	return 1;
}

/*
 * Function Declaratios for builtin shell commands
 */
int sh_cd(char **args);
int sh_exit(char **args);

/*
 * List of builtin commands, followed by their corresponding functions
 */
char *builtin_str[] = {
	"cd",
	"exit"
};

int (*builtin_func[]) (char **) = {
	&sh_cd,
	&sh_exit
};

int sh_num_builtins() 
{
	return sizeof(builtin_str) / sizeof(char *);
}

/*
 * Builtin function implementations
 */
int sh_cd(char **args)
{
	if (args[1] == NULL) {
		fprintf(stderr, "sh: expected arguments to \"cd\"\n");
	} else {
		if (chdir(args[1]) != 0) {
			perror("sh");
		}
	}
	
	return 1;
}

int sh_exit(char **args)
{
	return 0;
}

int sh_execute(char **args) 
{
	int i;

	if (args[0] == NULL) {
		// An empty command was entered
		return 1;
	}

	for (i = 0; i < sh_num_builtins(); i++) {
		if (strcmp(args[0], builtin_str[i]) == 0) {
			return (*builtin_func[i])(args);
		}
	}
	
	return sh_launch(args);
}
