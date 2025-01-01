#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define SH_READ_LINE_BUFSIZE 1024

#define SH_TOK_BUFSIZE 64
#define SH_TOK_LINE_DELIM "|"
#define SH_TOK_CMD_DELIM " \t\r\n\a"

struct fds {
	int oldfd;
	int newfd;
};

struct dups {
	struct fds indupfds;
	struct fds outdupfds;
};

/*
 * Function Declarations
 */
void sh_loop(void);
int sh_exit(char **args);
char *sh_read_line(void);
char **sh_split(char *string, char *delim);
int sh_execute(char **args, struct dups dups);
int sh_pipeline(char **cmds); 

int main(int argc, char **argv)
{
	sh_loop();

	return EXIT_SUCCESS;
}

void sh_loop(void)
{
	char *line;
	char **cmds;
	int status;

	do {
		printf("$ ");
		line = sh_read_line();
		cmds = sh_split(line, SH_TOK_LINE_DELIM);

		status = sh_pipeline(cmds);

		free(cmds);
		free(line);
	} while (status);
}

int sh_pipeline(char **cmds) 
{
	struct dups dups;
	int status;
	char **args;
	int pipe_fds[2];

	if (cmds[1] == NULL) {
		args = sh_split(cmds[0], SH_TOK_CMD_DELIM);

		dups.indupfds.oldfd, dups.indupfds.newfd = 0;
		dups.outdupfds.oldfd, dups.outdupfds.newfd = 1;

		status = sh_execute(args, dups);

		free(args);

		return status;
	}

	dups.indupfds.oldfd, dups.indupfds.newfd = 0;

	args = sh_split(cmds[0], SH_TOK_CMD_DELIM);

	for (int i = 1; cmds[i] != NULL; i++) {

		pipe(pipe_fds);

		dups.outdupfds.oldfd = pipe_fds[1];
		dups.outdupfds.newfd = 1; 			

		status = sh_execute(args, dups);
		
		args = NULL;

		if (!status) {
			free(args);
			return status;
		}

		args = sh_split(cmds[i], SH_TOK_CMD_DELIM);

		dups.indupfds.oldfd = pipe_fds[0];
		dups.indupfds.newfd = 0;
		
		if (cmds[i + 1] != NULL) {
			close(pipe_fds[0]);
		}
		close(pipe_fds[1]);
	}

	dups.outdupfds.oldfd, dups.outdupfds.newfd = 1;

	status = sh_execute(args, dups);

	close(pipe_fds[0]);

	free(args);

	return status;
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

char **sh_split(char *string, char *delim)
{
	int bufsize = SH_TOK_BUFSIZE, position = 0;
	char **tokens = malloc(bufsize * sizeof(char *));
	char *token;

	if (!tokens) {
		fprintf(stderr, "sh: allocation error\n");
		exit(EXIT_FAILURE);
	}

	token = strtok(string, delim);
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

		token = strtok(NULL, delim);
	}

	tokens[position] = NULL;
	return tokens;
}

int sh_launch(char **args, struct dups dups)
{
	pid_t pid, wpid;
	int status;

	pid = fork();
	if (pid == 0) {

		// Child process

		dup2(dups.indupfds.oldfd, dups.indupfds.newfd);
		dup2(dups.outdupfds.oldfd, dups.outdupfds.newfd);

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

int sh_execute(char **args, struct dups dups) 
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
	
	return sh_launch(args, dups);
}
