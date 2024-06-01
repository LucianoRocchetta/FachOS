#include <memoria.h>
#include <unistd.h>

int cliente_fd_cpu;
int cliente_fd_kernel;
int retardo_respuesta;
t_log *logger_memoria;
t_config *config_memoria;

t_list *pseudocodigo;
t_queue *procesos_activos;

sem_t instrucciones;

char *path;
char *path_instructions;

pthread_t hilo[3];

int enlistar_pseudocodigo(char *path_instructions, char *path, t_log *logger, t_list *pseudocodigo)
{
    char instruccion[30];
    
    char *full_path = strdup(path_instructions);
    strcat(full_path, path);

    

    FILE *f = fopen(full_path, "rb");

    if (f == NULL)
    {
        log_error(logger_memoria, "No se pudo abrir el archivo de %s (ERROR: %s)", full_path, strerror(errno));
        return EXIT_FAILURE;
    }

    while (!feof(f))
    {
        inst_pseudocodigo *inst_a_lista = malloc(sizeof(inst_pseudocodigo));
        char* linea_instruccion = fgets(instruccion, sizeof(instruccion), f);
        inst_a_lista->instruccion = strdup(linea_instruccion);
        list_add(pseudocodigo, inst_a_lista);
        linea_instruccion = NULL;
    }

    iterar_lista_e_imprimir(pseudocodigo);

    free(full_path);
    full_path = NULL;

    log_info(logger_memoria, "INSTRUCCIONES CARGADAS CORRECTAMENTE.\n");
    fclose(f);

    return EXIT_SUCCESS;
}

void enviar_instrucciones_a_cpu(char *program_counter, int retardo_respuesta)
{
    int retardo_en_segundos = (retardo_respuesta / 1000);
    
    sleep(retardo_en_segundos);

    int pc = atoi(program_counter);

    if (!list_is_empty(pseudocodigo))
    {
        inst_pseudocodigo *inst_a_mandar  = list_get(pseudocodigo, pc);
        log_info(logger_memoria, "Enviaste la instruccion n°%d: %s a CPU exitosamente", pc, inst_a_mandar->instruccion);
        paqueteDeMensajes(cliente_fd_cpu, inst_a_mandar->instruccion, INSTRUCCION);
    }
}

void iterar_lista_e_imprimir(t_list *lista)
{
    t_list_iterator *lista_a_iterar = list_iterator_create(lista);
    if (lista_a_iterar != NULL)
    {
        printf(" Lista de instrucciones : [ ");
        while (list_iterator_has_next(lista_a_iterar))
        {
            inst_pseudocodigo *elemento_actual = list_iterator_next(lista_a_iterar);
            if (list_iterator_has_next(lista_a_iterar))
            {
                printf("%s <- ", elemento_actual->instruccion);
            }
            else
            {
                printf("%s", elemento_actual->instruccion);
            }
        }
        printf(" ]\tElementos totales: %d\n", list_size(lista));
    }
    list_iterator_destroy(lista_a_iterar);
}

int main(int argc, char *argv[])
{
    int i, server_memoria;

    char *path_config = "../memoria/memoria.config";
    char *puerto_escucha;

    sem_init(&instrucciones, 1, 0);

    logger_memoria = iniciar_logger("memoria.log", "memoria-log", LOG_LEVEL_INFO);
    log_info(logger_memoria, "Logger Creado.");

    config_memoria = iniciar_config(path_config);
    puerto_escucha = config_get_string_value(config_memoria, "PUERTO_ESCUCHA");
    retardo_respuesta = config_get_int_value(config_memoria, "RETARDO_RESPUESTA"); 

    path_instructions = config_get_string_value(config_memoria, "PATH_INSTRUCCIONES");
    log_info(logger_memoria, "Utilizando el path the instrucciones: %s", path_instructions);

    sem_init(&instrucciones, 0, 0);

    pseudocodigo = list_create();

    server_memoria = iniciar_servidor(logger_memoria, puerto_escucha);
    log_info(logger_memoria, "Servidor a la espera de clientes");

    cliente_fd_cpu = esperar_cliente(server_memoria, logger_memoria);
    cliente_fd_kernel = esperar_cliente(server_memoria, logger_memoria);
    // int cliente_fd_tres = esperar_cliente(server_memoria, logger_memoria);

    ArgsGestionarServidor args_sv1 = {logger_memoria, cliente_fd_cpu};
    ArgsGestionarServidor args_sv2 = {logger_memoria, cliente_fd_kernel};
    // ArgsGestionarServidor args_sv3 = {logger_memoria, cliente_fd_tres};

    pthread_create(&hilo[0], NULL, gestionar_llegada_memoria_cpu, &args_sv1);
    pthread_create(&hilo[1], NULL, gestionar_llegada_memoria_kernel, &args_sv2);
    // pthread_create(&hilo[2], NULL, gestionar_llegada, &args_sv3);

    for (i = 0; i < 3; i++)
    {
        pthread_join(hilo[i], NULL);
    }

    return 0;
}

void *gestionar_llegada_memoria_cpu(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;
        case INSTRUCCION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char *program_counter = list_get(lista, 0);
            log_info(logger_memoria, "Me solicitaron la instruccion n°%s", program_counter);
            sem_post(&instrucciones);
            enviar_instrucciones_a_cpu(program_counter, retardo_respuesta);
            break;
        case -1:
            log_error(logger_memoria, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(logger_memoria, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_memoria_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            lista = recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger, MENSAJE);
            break;
        case CREAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, args_entrada->logger);
            char* instrucciones = list_get(lista, 0);
            log_info(logger_memoria, "-Asignando espacio para nuevo proceso-\n...\n");
            pcb *new = crear_pcb(instrucciones);
            log_info(logger_memoria, "-Espacio asignado-");
            peticion_de_espacio_para_pcb(cliente_fd_kernel, new, CREAR_PROCESO);
            break;
        case FINALIZAR_PROCESO:
            lista = recibir_paquete(args_entrada->cliente_fd, args_entrada->logger);
            pcb *a_eliminar = list_get(lista, 0);
            a_eliminar->path_instrucciones = list_get(lista, 1);
            a_eliminar->estadoActual = list_get(lista, 2);
            a_eliminar->estadoAnterior = list_get(lista, 3);
            a_eliminar->contexto = list_get(lista, 4);
            a_eliminar->contexto->registros = list_get(lista, 5);

            destruir_pcb(a_eliminar);

            paqueteDeMensajes(cliente_fd_kernel, "Succesful delete. Coming back soon!\n", FINALIZAR_PROCESO);
            break;
        case PATH:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_memoria);
            char *path = list_get(lista, 0);
            log_info(logger_memoria, "PATH RECIBIDO: %s", path);
            if (!list_is_empty(pseudocodigo))
            {
                log_info(logger_memoria, "BORRANDO LISTA...\n");
                list_clean_and_destroy_elements(pseudocodigo, destruir_instrucciones);
                enlistar_pseudocodigo(path_instructions, path, logger_memoria, pseudocodigo);
            }
            else
            {
                enlistar_pseudocodigo(path_instructions, path, logger_memoria, pseudocodigo);
            }
            break;
        case -1:
            log_error(logger_memoria, "el cliente se desconecto. Terminando servidor");
            return (void *)EXIT_FAILURE;
        default:
            log_warning(logger_memoria, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

pcb *crear_pcb(char* instrucciones)
{
    pcb *pcb_nuevo = malloc(sizeof(pcb));

    eliminarEspaciosBlanco(instrucciones);
    pcb_nuevo->path_instrucciones = strdup(instrucciones);

    pcb_nuevo->contexto = malloc(sizeof(cont_exec));
    pcb_nuevo->contexto->registros = malloc(sizeof(regCPU));
    
    // Implementacion de tabla vacia de paginas 

    return pcb_nuevo;
}

void destruir_pcb(pcb *elemento)
{
    free(elemento->contexto->registros);
    elemento->contexto->registros = NULL;
    free(elemento->contexto);
    elemento->contexto = NULL;
    free(elemento->estadoAnterior);
    elemento->estadoAnterior = NULL;
    free(elemento->estadoActual);
    elemento->estadoActual = NULL;
    free(elemento->path_instrucciones);
    elemento->path_instrucciones = NULL;
    free(elemento);
    elemento = NULL;
}

void destruir_instrucciones(void* data){
    inst_pseudocodigo *elemento = (inst_pseudocodigo*)data;
    elemento->instruccion = NULL;
    free(elemento);
    elemento = NULL;
}