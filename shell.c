#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define SH_READ_LINE_BUFSIZE 1024

#define SH_TOK_BUFSIZE 64
#define SH_TOK_REDIRECT_DELIM ">"
#define SH_TOK_PIPING_DELIM "|"
#define SH_TOK_CMD_DELIM " \t\r\n\a"

struct fds {
	int old_fd;
	int new_fd;
};

struct dups {
	struct fds in_dup_fds;
	struct fds out_dup_fds;
};

/*
 * Function Declarations
 */
void sh_loop(void);
int sh_exit(char **args);
char *sh_read_line(void);
char **sh_split(char *string, char *delim);
int sh_execute(char **args, struct dups dups);
int sh_redirect(char **cmd, struct dups dups);
int sh_piping(char **cmds); 
char *sh_trim(char *string);

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
		cmds = sh_split(line, SH_TOK_PIPING_DELIM);

		status = sh_piping(cmds);

		free(cmds);
		cmds = NULL;
		free(line);
		line = NULL;
	} while (status);
}

int sh_redirect(char **cmd, struct dups dups) 
{
	int status, out_fd;
	char **args;
	char *trim_string;
	
	args = sh_split(cmd[0], SH_TOK_CMD_DELIM);
	
	if (cmd[1] != NULL) {

		trim_string = sh_trim(cmd[1]);

		out_fd = fileno(fopen(trim_string, "w"));

		free(trim_string);
		trim_string = NULL;

		dups.out_dup_fds.old_fd = out_fd;

		status = sh_execute(args, dups);

		close(out_fd);

	} else {
		status = sh_execute(args, dups);
	}

	return status;
}

char *sh_trim(char *string)
{
	int i, j, length = strlen(string);
	char *trim_string = malloc((length + 1) * sizeof(char));

	i = 0;
	while (isspace(string[i])) {
		i++;
	}

	j = 0;
	while (string[i] != '\0') {
		trim_string[j] = string[i];
		i++, j++;
	}

	j--;

	while (isspace(trim_string[j])) {
		j--;
	}

	trim_string[j + 1] = '\0';

	return trim_string;
}

int sh_piping(char **cmds) 
{
	struct dups dups;
	int status;
	char **cmd;
	int pipe_fds[2];

	if (cmds[1] == NULL) {

		cmd = sh_split(cmds[0], SH_TOK_REDIRECT_DELIM);

		dups.in_dup_fds.new_fd = STDIN_FILENO;
		dups.in_dup_fds.old_fd = STDIN_FILENO;
		
		dups.out_dup_fds.old_fd = STDOUT_FILENO; 
		dups.out_dup_fds.new_fd = STDOUT_FILENO;

		status = sh_redirect(cmd, dups);

		free(cmd);
		cmd = NULL;

		return status;
	}

	dups.in_dup_fds.new_fd = STDIN_FILENO;
	dups.in_dup_fds.old_fd = STDIN_FILENO;

	cmd = sh_split(cmds[0], SH_TOK_REDIRECT_DELIM);

	for (int i = 1; cmds[i] != NULL; i++) {

		pipe(pipe_fds);

		dups.out_dup_fds.old_fd = pipe_fds[1];
		dups.out_dup_fds.new_fd = STDOUT_FILENO; 			

		status = sh_redirect(cmd, dups);
		
		cmd = NULL;

		if (!status) {
			free(cmd);
			cmd = NULL;
			return status;
		}

		cmd = sh_split(cmds[i], SH_TOK_REDIRECT_DELIM);

		dups.in_dup_fds.old_fd = pipe_fds[0];
		dups.in_dup_fds.new_fd = STDIN_FILENO;
		
		if (cmds[i + 2] != NULL) {
			close(pipe_fds[0]);
		}
		close(pipe_fds[1]);
	}

	dups.out_dup_fds.old_fd = STDOUT_FILENO;
	dups.out_dup_fds.new_fd = STDOUT_FILENO;

	status = sh_redirect(cmd, dups);

	close(pipe_fds[0]);

	free(cmd);
	cmd = NULL;

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

char **sh_split_cmd(char *string, char *delim)
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

		dup2(dups.in_dup_fds.old_fd, dups.in_dup_fds.new_fd);
		dup2(dups.out_dup_fds.old_fd, dups.out_dup_fds.new_fd);

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
