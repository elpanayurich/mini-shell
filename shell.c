#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>

extern char **environ;

typedef struct {
	char *nombre;
	char *valor;
} Variable;

#define MAX_VAR 20

Variable variables[MAX_VAR];
int actual_var = 0;

void tokenizar_comando(char *input);

Variable
copiar_variable(const Variable * origen)
{
	Variable copia;

	if (origen->nombre) {
		copia.nombre = strdup(origen->nombre);
	} else {
		copia.nombre = NULL;
	}

	if (origen->valor) {
		copia.valor = strdup(origen->valor);
	} else {
		copia.valor = NULL;
	}
	return copia;
}

void
copiar_variables(Variable * destino, const Variable * origen, size_t num_vars)
{
	for (size_t i = 0; i < num_vars; i++) {
		destino[i] = copiar_variable(&origen[i]);
	}
}

void
liberar_variable(Variable * var)
{
	if (var->nombre)
		free(var->nombre);
	if (var->valor)
		free(var->valor);
}

void
liberar_variables(Variable * vars, size_t num_vars)
{
	for (size_t i = 0; i < num_vars; i++) {
		liberar_variable(&vars[i]);
	}
}

int
character_counter(char *input, char caracter)
{
	int contador = 0;

	for (int i = 0; input[i] != '\0'; i++) {
		if (input[i] == caracter) {
			contador++;
		}
	}
	return contador;
}

int
comprobar_in_or_out(char *input)
{
	char caracter = '<';
	int contador1 = character_counter(input, caracter);

	caracter = '>';
	int contador2 = character_counter(input, caracter);

	if (contador1 == 0 && contador2 != 0) {
		return 1;
	} else if (contador1 != 0 && contador2 == 0) {
		return 2;
	} else {
		return 3;
	}
}

char *
buscar_en_path(const char *comando)
{
	char *path = getenv("PATH");

	if (!path)
		return NULL;

	char *path_dup = strdup(path);

	if (!path_dup) {
		perror("error en strdup");
		return NULL;
	}

	char *dir = strtok(path_dup, ":");
	static char ruta_completa[4096];

	while (dir) {
		snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", dir,
			 comando);

		if (access(ruta_completa, X_OK) == 0) {
			free(path_dup);
			return ruta_completa;
		}

		dir = strtok(NULL, ":");
	}

	free(path_dup);
	return NULL;
}

void
cleanup_and_exit(int exit_code)
{
	for (int i = 0; i < MAX_VAR; i++) {
		free(variables[i].nombre);
		free(variables[i].valor);
	}
	exit(exit_code);
}

int
tiene_variables(const char *str)
{
	while (*str) {
		if (*str == '$') {
			return 1;
		}
		str++;
	}
	return 0;
}

int
tiene_espacios(const char *str)
{
	while (*str) {
		if (*str == ' ') {
			return 1;
		}
		str++;
	}
	return 0;
}

int
blank_space(char *input)
{
	for (int i = 0; input[i] != '\0'; i++) {
		if (!isspace((unsigned char)input[i]))
			return 0;
	}
	return 1;
}

int
comprobador_redireccion(char *input)
{
	return character_counter(input, '<') + character_counter(input,
								 '>') > 0;
}

char *
read_input(void)
{
	int buffer_size = 2048;
	char *input = malloc(buffer_size);

	if (!input) {
		fprintf(stderr, "error: malloc failed\n");
		cleanup_and_exit(EXIT_FAILURE);
	}

	int i = 0;
	int c;

	while ((c = getchar()) != EOF) {
		if (i >= buffer_size - 1) {
			buffer_size *= 2;
			char *temp = realloc(input, buffer_size);

			if (!temp) {
				free(input);
				fprintf(stderr, "error: realloc failed\n");
				cleanup_and_exit(EXIT_FAILURE);
			}
			input = temp;
		}
		input[i++] = (char)c;
		if (c == '\n')
			break;
	}

	if (c == EOF && i == 0) {
		free(input);
		return NULL;
	}

	input[i] = '\0';
	return input;
}

void
ejecutar_comando(char *comando, char *argumentos[], int ampersant)
{
	pid_t pid;
	int status;

	if (access(comando, X_OK) == 0) {
		pid = fork();
		if (pid == 0) {
			if (ampersant) {
				int dev_null = open("/dev/null", O_RDONLY);

				if (dev_null == -1) {
					perror("open");
					exit(EXIT_FAILURE);
				}
				dup2(dev_null, STDIN_FILENO);
				close(dev_null);
			}

			execve(comando, argumentos, environ);
			perror("error en el exec");
			exit(EXIT_FAILURE);

		} else if (pid > 0) {
			if (!ampersant) {
				waitpid(pid, &status, 0);
			}
		} else {
			perror("error en el fork");
		}
		return;
	}

	char *ruta_comando = buscar_en_path(comando);

	if (ruta_comando) {
		pid = fork();
		if (pid == 0) {
			if (ampersant) {
				int dev_null = open("/dev/null", O_RDONLY);

				if (dev_null == -1) {
					perror("open");
					exit(EXIT_FAILURE);
				}
				dup2(dev_null, STDIN_FILENO);
				close(dev_null);
			}
			execve(ruta_comando, argumentos, environ);
			perror("error en el exec");
			exit(EXIT_FAILURE);
		} else if (pid > 0) {
			if (!ampersant) {
				waitpid(pid, &status, 0);
			}
		} else {
			perror("error en el fork");
		}
	} else {
		fprintf(stderr, "error: comando no encontrado\n");
	}
}

void
replace_with_null(char *str)
{
	for (int i = 0; i < strlen(str); i++) {
		if (str[i] == '<') {
			str[i] = '\0';
			break;
		}
	}
}

void
caso_out(char *comando, char *argumentos[], int redir, int size, int varios,
	 int ampersant)
{
	int fd;
	int copia_stdout = dup(STDOUT_FILENO);

	char *archivo = NULL;
	int i;

	for (i = 0; i < size; i++) {
		if (argumentos[i] != NULL) {
			archivo = argumentos[i];
		}

		if (argumentos[i] == NULL) {
			break;
		}
	}

	argumentos[i - 2] = NULL;
	argumentos[i - 1] = NULL;

	size_t total_size = 0;

	for (int j = 0; j < i - 2; j++) {
		total_size += strlen(argumentos[j]) + 1;
	}

	char *result = malloc(total_size);

	if (!result) {
		perror("error en malloc");
		cleanup_and_exit(EXIT_FAILURE);
	}

	result[0] = '\0';

	for (int j = 0; j < i - 2; j++) {
		strcat(result, argumentos[j]);
		if (j < i - 1)
			strcat(result, " ");
	}

	fd = open(archivo, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	if (fd == -1) {
		perror("open");
		return;
	}

	if (dup2(fd, STDOUT_FILENO) == -1) {
		perror("dup2");
		close(fd);
		return;
	}

	close(fd);
	if (varios == 0) {
		ejecutar_comando(comando, argumentos, ampersant);
	} else {
		tokenizar_comando(result);
	}

	if (dup2(copia_stdout, STDOUT_FILENO) == -1) {
		perror("dup2");
		return;
	}
	close(copia_stdout);
	free(result);
}

void
caso_in(char *comando, char *argumentos[], int redir, int size, int ampersant)
{
	int fd;
	int copia_stdin = dup(STDIN_FILENO);

	char *archivo = NULL;
	int i;

	for (i = 0; i < size; i++) {
		if (argumentos[i] != NULL) {
			archivo = argumentos[i];
		}

		if (argumentos[i] == NULL) {
			break;
		}
	}

	argumentos[i - 2] = NULL;
	argumentos[i - 1] = NULL;

	fd = open(archivo, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return;
	}

	if (dup2(fd, STDIN_FILENO) == -1) {
		perror("dup2");
		close(fd);
		return;
	}

	close(fd);

	ejecutar_comando(comando, argumentos, ampersant);

	if (dup2(copia_stdin, STDIN_FILENO) == -1) {
		perror("dup2");
		return;
	}
	close(copia_stdin);
}

int
verify_ending_ampersant(char *argumentos[], int i)
{
	if (strcmp(argumentos[i - 1], "&") == 0) {
		argumentos[i - 1] = NULL;
		return 1;
	}
	return 0;
}

void
tokenizar_comando(char *input)
{
	char *copy = strdup(input);

	if (!copy) {
		perror("error en strdup");
		cleanup_and_exit(EXIT_FAILURE);
	}

	char *token = strtok(copy, " \t\n");

	if (!token) {
		free(copy);
		return;
	}

	char *comando = token;

	if (strcmp(comando, "exit") == 0) {
		free(copy);
		cleanup_and_exit(EXIT_SUCCESS);
	} else if (strcmp(comando, "cd") == 0) {
		token = strtok(NULL, " \t\n");
		const char *dir = token ? token : getenv("HOME");

		if (chdir(dir) != 0) {
			perror("error al cambiar de directorio");
		}
		free(copy);
		return;
	}

	char *argumentos[100];
	int i = 0;

	while (token) {
		argumentos[i++] = token;
		token = strtok(NULL, " \t\n");
	}
	argumentos[i] = NULL;
	int size = sizeof(argumentos) / sizeof(argumentos[0]);

	int ampersant = verify_ending_ampersant(argumentos, i);

	int redir = comprobador_redireccion(input);

	if (redir == 1) {
		if (comprobar_in_or_out(input) == 1) {
			caso_out(comando, argumentos, redir, size, 0,
				 ampersant);

		} else if (comprobar_in_or_out(input) == 2) {
			caso_in(comando, argumentos, redir, size, ampersant);

		} else if (comprobar_in_or_out(input) == 3) {
			caso_out(comando, argumentos, redir, size, 1,
				 ampersant);
		}
	} else {
		ejecutar_comando(comando, argumentos, ampersant);
	}

	free(copy);
}

int
check_equal(char *input)
{
	for (int i = 0; input[i] != '\0'; i++) {
		if (input[i] == '=') {
			return 1;
		}
	}
	return 0;
}

void
nueva_variable(char *input)
{
	char *copy = strdup(input);

	if (!copy) {
		perror("error en strdup");
		cleanup_and_exit(EXIT_FAILURE);
	}

	char *token = strtok(copy, "=");

	if (!token) {
		free(copy);
		return;
	}

	char *variable = token;

	token = strtok(NULL, "=");
	if (!token) {
		free(copy);
		return;
	}

	char *valor = token;

	int existe = 0;

	for (int i = 0; i < MAX_VAR; i++) {
		if (variables[i].nombre
		    && strcmp(variables[i].nombre, variable) == 0) {
			printf
			    ("la variable ya existe (crea una nueva variable)\n");
			existe = 1;
		}
	}

	if (existe == 0) {
		variables[actual_var].nombre = strdup(variable);
		variables[actual_var].valor = strdup(valor);
		actual_var++;
	}

	free(copy);
}

char *
sustituir_variables(char *input)
{
	if (!input)
		return NULL;

	char *copy = strdup(input);

	if (!copy) {
		perror("error en strdup");
		cleanup_and_exit(EXIT_FAILURE);
	}

	Variable destino[MAX_VAR] = { 0 };
	copiar_variables(destino, variables, actual_var);

	char *token = strtok(copy, " \t\n");

	if (!token) {
		free(copy);
		return NULL;
	}

	char *argumentos[100];
	int i = 0;

	while (token) {
		argumentos[i++] = token;
		token = strtok(NULL, " \t\n");
	}
	argumentos[i] = NULL;

	size_t total_size = 0;

	for (int j = 0; j < i; j++) {
		int is_variable = tiene_variables(argumentos[j]);

		if (is_variable) {
			int existe = 0;

			for (int k = 0; k < actual_var; k++) {
				if (variables[k].nombre
				    && strcmp(variables[k].nombre,
					      argumentos[j] + 1) == 0) {
					argumentos[j] = destino[k].valor;
					existe = 1;
					break;
				}
			}
			if (!existe) {
				free(copy);
				return strdup("error");
			}
		}
		total_size += strlen(argumentos[j]) + 1;
	}

	char *result = malloc(total_size);

	if (!result) {
		perror("error en malloc");
		free(copy);
		cleanup_and_exit(EXIT_FAILURE);
	}

	result[0] = '\0';

	for (int j = 0; j < i; j++) {
		strcat(result, argumentos[j]);
		if (j < i - 1)
			strcat(result, " ");
	}

	free(copy);
	liberar_variables(destino, MAX_VAR);

	return result;
}

int
main(void)
{
	int igualdad;
	int error;

	while (1) {
		error = 0;
		fputs("mini-shell ~ ", stdout);

		char *input = read_input();

		if (input == NULL) {
			cleanup_and_exit(EXIT_SUCCESS);
		}

		igualdad = check_equal(input);

		if (tiene_variables(input)) {
			input = sustituir_variables(input);
			if (strcmp(input, "error") == 0) {
				fprintf(stderr,
					"error de variable que no existe\n");
				error = 1;
			}
		}

		if (strlen(input) > 0 && !blank_space(input) && igualdad == 0
		    && error == 0) {
			char *copia = strdup(input);

			tokenizar_comando(copia);
			free(copia);
		}

		if (igualdad == 1 && error == 0) {
			if (tiene_espacios(input)) {
				fprintf(stderr,
					"la variable no puede tener espacios\n");
			} else {
				nueva_variable(input);
			}
		}
		free(input);
	}
	cleanup_and_exit(EXIT_SUCCESS);
	return 0;
}
