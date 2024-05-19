#include <utils/parse.h>

bool is_valid_command(const char *command, void *structure) {
    COMMAND *commands = (COMMAND*)structure;
    for (int i = 0; commands[i].command != NULL; i++) {
        if (strcmp(commands[i].command, command) == 0) {
            return true;
        }
    }
    return false;
}

COMMAND* find_command(char* command, void *structure) {
    COMMAND *commands = (COMMAND*)structure;
    if (commands == NULL) {
        return NULL; // Si la lista de comandos es nula, no hay comandos para buscar
    }
    for (int i = 0; commands[i].command != NULL; i++) {
        if (strcmp(commands[i].command, command) == 0) {
            return &commands[i];
        }
    }
    return NULL; // Si el comando no se encuentra, devuelve NULL
}

RESPONSE* parse_command(char* input, void *structure) {
    COMMAND *commands = (COMMAND*)structure;
    RESPONSE *response = malloc(sizeof(RESPONSE));
    char command_name[100];
    char input_copy[100];

    strcpy(input_copy, input);

    // Tokenizar string por espacios (Ahorra los whiles)
    char *token = strtok(input_copy, " ");

    // Compruebo que el comando exista en la estructura.
    if(!is_valid_command(token, commands)) {
        printf("Comando invalido: %s\n", token);
        return 0;
    }
    printf("Comando valido: %s\n", token);
    strcpy(command_name, token);

    // Agarro los parametros
    int params_max = 3;
    char *params[params_max];
    int index = 0;
    while ((token = strtok(NULL, " ")) != NULL && index < params_max) {
        params[index++] = token;
    }
    params[index] = NULL; // Marcar el final del array de parámetros.

    // Asignar valores a la estructura RESPONSE
    response->command = strdup(command_name);
    response->params = malloc(sizeof(char*) * (index + 1));
    if (response->params == NULL) {
        free(response->command);
        free(response);
        return NULL;
    }
    for (int i = 0; i < index; i++) {
        response->params[i] = strdup(params[i]);
    }
    response->params[index] = NULL;


    return response;
}