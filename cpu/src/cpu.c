#include <cpu.h>
#include <utils/parse.h>

int conexion_memoria;
int server_interrupt;
int server_dispatch;
int cliente_fd_dispatch;
int tam_pagina;

bool flag_interrupcion;

char *instruccion_a_ejecutar;
char *interrupcion;
char *memoria_response;
TLB *tlb;

t_log *logger_cpu;
t_config *config;

cont_exec *contexto;

REGISTER register_map[11];
const int num_register = sizeof(register_map) / sizeof(REGISTER);

pthread_mutex_t mutex_ejecucion = PTHREAD_MUTEX_INITIALIZER;

pthread_t hilo_proceso;

sem_t sem_contexto;
sem_t sem_ejecucion;
sem_t sem_instruccion;
sem_t sem_interrupcion;
sem_t sem_leer_memoria;

INSTRUCTION instructions[] = {
    {"SET", set, "Ejecutar SET", 0},
    {"MOV_IN", mov_in, "Ejecutar MOV_IN", 1},
    {"MOV_OUT", mov_out, "Ejecutar MOV_OUT", 0},
    {"SUM", sum, "Ejecutar SUM", 0},
    {"SUB", sub, "Ejecutar SUB", 0},
    {"JNZ", jnz, "Ejecutar JNZ", 0},
    {"MOV", mov, "Ejecutar MOV", 0},
    {"RESIZE", resize, "Ejecutar RESIZE", 0},
    {"COPY_STRING", copy_string, "Ejecutar COPY_STRING", 0},
    {"WAIT", WAIT, "Ejecutar WAIT", 0},
    {"SIGNAL", SIGNAL, "Ejecutar SIGNAL", 0},
    {"IO_GEN_SLEEP", io_gen_sleep, "Ejecutar IO_GEN_SLEEP", 0},
    {"IO_STDIN_READ", io_stdin_read, "Ejecutar IO_STDIN_READ", 1},
    {"IO_STDOUT_WRITE", io_stdout_write, "Ejecutar IO_STDOUT_WRITE", 1},
    {"IO_FS_CREATE", io_fs_create, "Ejecutar IO_FS_CREATE", 0},
    {"IO_FS_DELETE", io_fs_delete, "Ejecutar IO_FS_DELETE", 0},
    {"IO_FS_TRUNCATE", io_fs_trucate, "Ejecutar IO_FS_TRUNCATE", 0},
    {"IO_FS_READ", io_fs_read, "Ejecutar IO_FS_READ", 2},
    {"IO_FS_WRITE", io_fs_write, "Ejecutar IO_FS_WRITE", 2},
    {"EXIT", EXIT, "Syscall, devuelve el contexto a kernel", 0},
    {NULL, NULL, NULL, 0}};

const char *instrucciones_logicas[6] = {"MOV_IN", "MOV_OUT", "IO_STDIN_READ", "IO_STDOUT_WRITE", "IO_FS_WRITE", "IO_FS_READ"};

int main(int argc, char *argv[])
{
    int i;
    char *config_path = "../cpu/cpu.config";

    logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    config = iniciar_config(config_path);

    pthread_t hilo_id[4];

    char *ip_memoria = config_get_string_value(config, "IP_MEMORIA");
    char *puerto_memoria = config_get_string_value(config, "PUERTO_MEMORIA");
    char *puerto_dispatch = config_get_string_value(config, "PUERTO_ESCUCHA_DISPATCH");
    char *puerto_interrupt = config_get_string_value(config, "PUERTO_ESCUCHA_INTERRUPT");

    int cant_ent_tlb = config_get_int_value(config, "CANTIDAD_ENTRADAS_TLB");
    char *algoritmo_tlb = config_get_string_value(config, "ALGORITMO_TLB");

    log_info(logger_cpu, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);


    // Inicializar tlb
    tlb = inicializar_tlb(cant_ent_tlb);

    // Abrir servidores

    server_dispatch = iniciar_servidor(logger_cpu, puerto_dispatch);
    log_info(logger_cpu, "Servidor dispatch abierto");
    server_interrupt = iniciar_servidor(logger_cpu, puerto_interrupt);
    log_info(logger_cpu, "Servidor interrupt abierto");

    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_operacion("CPU IS IN DA HOUSE", conexion_memoria, MENSAJE);

    cliente_fd_dispatch = esperar_cliente(server_dispatch, logger_cpu);
    int cliente_fd_interrupt = esperar_cliente(server_interrupt, logger_cpu);

    sem_init(&sem_contexto, 1, 1);
    sem_init(&sem_ejecucion, 1, 0);
    sem_init(&sem_interrupcion, 1, 0);
    sem_init(&sem_instruccion, 1, 0);
    sem_init(&sem_leer_memoria, 1, 0);

    ArgsGestionarServidor args_dispatch = {logger_cpu, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger_cpu, cliente_fd_interrupt};
    ArgsGestionarServidor args_memoria = {logger_cpu, conexion_memoria};

    pthread_create(&hilo_id[0], NULL, gestionar_llegada_kernel, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada_kernel, &args_interrupt);
    pthread_create(&hilo_id[2], NULL, gestionar_llegada_memoria, &args_memoria);

    for (i = 0; i < 5; i++)
    {
        pthread_join(hilo_id[i], NULL);
    }

    sem_destroy(&sem_ejecucion);
    sem_destroy(&sem_interrupcion);
    sem_destroy(&sem_leer_memoria);
    liberar_conexion(conexion_memoria);
    terminar_programa(logger_cpu, config);

    return 0;
}

void Execute(RESPONSE *response, cont_exec *contexto)
{
    if (response != NULL)
    {
        for (int i = 0; instructions[i].command != NULL; i++)
        {
            if (strcmp(instructions[i].command, response->command) == 0)
            {
                instructions[i].function(response->params);
                return;
            }
        }
    }
}

RESPONSE *Decode(char *instruccion)
{
    RESPONSE *response;
    response = parse_command(instruccion);
    int index = 0;

    //Encontrar comando
    if (response != NULL)
    {
        for (int i = 0; instructions[i].command != NULL; i++)
        {
            if (strcmp(instructions[i].command, response->command) == 0)
            {
                index = instructions[i].posicion_direccion_logica;
            }
        }
    }

    //TODO: Logica de traducciones MMU
    /*
    int cant_commands = sizeof(instrucciones_logicas) / sizeof(char);
    for(int i = 0; i < cant_commands; i++) {
        if(strcmp(response->command, instrucciones_logicas[i])) {
            response->params[index] = traducirDireccionLogica(index);
        }
    }
    */

    return response;
}

void Fetch(cont_exec *contexto)
{
    t_instruccion* fetch = malloc(sizeof(t_instruccion));
    fetch->pc = strdup(string_itoa(contexto->registros->PC));
    fetch->pid = strdup(string_itoa(contexto->PID));
    fetch->marco = NULL;

    //Implementando tlb para facilitar
    char* index_marco = string_itoa(chequear_en_tlb(fetch->pid, fetch->pc));
    fetch->marco = index_marco;

    if(atoi(index_marco) != -1) {
        log_info(logger_cpu, "PID: %s - TLB HIT - Pagina: %s", fetch->pid, fetch->pc);
    } else {
        log_info(logger_cpu, "PID: %s - TLB MISS - Pagina: %s", fetch->pid, fetch->pc);
    }

    paquete_solicitud_instruccion(conexion_memoria, fetch); // Enviamos instruccion para mandarle la instruccion que debe mandarnos

    log_info(logger_cpu, "Se solicito a memoria el paso de la instruccion n°%d", contexto->registros->PC);
    
    sem_wait(&sem_instruccion);

    free(fetch->pc);
    fetch->pc = NULL;
    free(fetch->pid);
    fetch->pid = NULL;
    fetch = NULL;
}

void procesar_contexto(cont_exec* contexto)
{
    while (flag_interrupcion)
    { 
        RESPONSE *response;
        Fetch(contexto);
    
        log_info(logger_cpu, "El decode recibio %s", instruccion_a_ejecutar);

        response = Decode(instruccion_a_ejecutar);
        
        log_info(logger_cpu, "PID: %d - Ejecutando: %s - %s %s", contexto->PID, response->command, response->params[0], response->params[1]);

        if (es_motivo_de_salida(response->command))
        {
            contexto->registros->PC++;
            Execute(response, contexto);
            sem_post(&sem_contexto);
            return;
        }

        contexto->registros->PC++;
        Execute(response, contexto);
    }

    sem_wait(&sem_interrupcion);
    enviar_contexto_pcb(cliente_fd_dispatch, contexto, INTERRUPCION);
    
    pthread_mutex_lock(&mutex_ejecucion);
    log_info(logger_cpu, "Desalojando registro. MOTIVO: %s\n", interrupcion);
    pthread_mutex_unlock(&mutex_ejecucion);
    
    sem_post(&sem_contexto);
}

void *gestionar_llegada_kernel(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            recibir_mensaje(args_entrada->cliente_fd, logger_cpu, MENSAJE);
            break;
        case INTERRUPCION:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            pthread_mutex_lock(&mutex_ejecucion);
            interrupcion = list_get(lista, 0);
            flag_interrupcion = false;
            pthread_mutex_unlock(&mutex_ejecucion);
            sem_post(&sem_interrupcion);
            break;
        case CONTEXTO:
            sem_wait(&sem_contexto);
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            contexto = list_get(lista, 0);
            contexto->registros = list_get(lista, 1);
            log_info(logger_cpu, "Recibi un contexto PID: %d", contexto->PID);
            log_info(logger_cpu, "PC del CONTEXTO: %d", contexto->registros->PC);
            flag_interrupcion = true;
            procesar_contexto(contexto);
            break;
        case -1:
            log_error(logger_cpu, "el cliente se desconecto. Terminando servidor");
            return (void*)EXIT_FAILURE;
        default:
            log_warning(logger_cpu, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

void *gestionar_llegada_memoria(void *args)
{
    ArgsGestionarServidor *args_entrada = (ArgsGestionarServidor *)args;

    t_list *lista;
    while (1)
    {
        int cod_op = recibir_operacion(args_entrada->cliente_fd);
        switch (cod_op)
        {
        case MENSAJE:
            recibir_mensaje(args_entrada->cliente_fd, logger_cpu, MENSAJE);
            break;
        case RESPUESTA_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            instruccion_a_ejecutar = list_get(lista, 0);
            char* index_marco = list_get(lista, 1);
            log_info(logger_cpu, "PID: %d - FETCH - Program Counter: %d", contexto->PID, contexto->registros->PC);

            //Cargo la TLB despues de haber pedido la instruccion con exito
            if(atoi(index_marco) != -1) {
                TLBEntry* tlbentry = malloc(sizeof(TLBEntry));
                tlbentry->pid = contexto->PID;
                tlbentry->pagina = contexto->registros->PC;
                tlbentry->marco = atoi(index_marco);
                list_add(tlb->entradas, tlbentry);
            }
           
            sem_post(&sem_instruccion);
            break;
        case RESPUESTA_LEER_MEMORIA:
            lista = recibir_paquete(args_entrada->cliente_fd, logger_cpu);
            memoria_response = list_get(lista, 0);
            
            sem_post(&sem_leer_memoria);
        case -1:
            log_error(logger_cpu, "el cliente se desconecto. Terminando servidor");
            return (void*)EXIT_FAILURE;
        default:
            log_warning(logger_cpu, "Operacion desconocida. No quieras meter la pata");
            break;
        }
    }
}

// Función para inicializar el mapeo de registros
void upload_register_map()
{
    register_map[0] = (REGISTER){"PC", (uint32_t *)&contexto->registros->PC, TYPE_UINT32};
    register_map[1] = (REGISTER){"AX", (uint8_t *)&contexto->registros->AX, TYPE_UINT8};
    register_map[2] = (REGISTER){"BX", (uint8_t *)&contexto->registros->BX, TYPE_UINT8};
    register_map[3] = (REGISTER){"CX", (uint8_t *)&contexto->registros->CX, TYPE_UINT8};
    register_map[4] = (REGISTER){"DX", (uint8_t *)&contexto->registros->DX, TYPE_UINT8};
    register_map[5] = (REGISTER){"EAX", (uint32_t *)&contexto->registros->EAX, TYPE_UINT32};
    register_map[6] = (REGISTER){"EBX", (uint32_t *)&contexto->registros->EBX, TYPE_UINT32};
    register_map[7] = (REGISTER){"ECX", (uint32_t *)&contexto->registros->ECX, TYPE_UINT32};
    register_map[8] = (REGISTER){"EDX", (uint32_t *)&contexto->registros->EDX, TYPE_UINT32};
    register_map[9] = (REGISTER){"SI", (uint32_t *)&contexto->registros->SI, TYPE_UINT32};
    register_map[10] = (REGISTER){"DI", (uint32_t *)&contexto->registros->DI, TYPE_UINT32};
}

REGISTER *find_register(const char *name)
{
    upload_register_map();

    for (int i = 0; i < num_register; i++)
    {
        if (!strcmp(register_map[i].name, name))
        {
            return &register_map[i];
        }
    }
    return NULL;
}

void set(char **params)
{
    printf("Ejecutando instruccion SET\n");
    printf("Me llegaron los parametros: %s, %s\n", params[0], params[1]);

    const char *register_name = params[0];
    int new_register_value = atoi(params[1]);

    REGISTER *found_register = find_register(register_name);
    if (found_register->type == TYPE_UINT32){
        *(uint32_t *)found_register->registro = new_register_value;
        printf("Valor del registro %s actualizado a %d\n", register_name, *(uint32_t *)found_register->registro);
    }
    else if (found_register->type == TYPE_UINT8){
        *(uint8_t *)found_register->registro = (uint8_t)new_register_value;
        printf("Valor del registro %s actualizado a %d\n", register_name, *(uint8_t *)found_register->registro);
    }
    else{
        printf("Registro desconocido: %s\n", register_name);
    }
    found_register = NULL;
    free(found_register);
}

// primer parametro: destino (TARGET), segundo parametro: origen (ORIGIN)
void sum(char **params)
{
    printf("Ejecutando instruccion SUM\n");
    char* first_register = params[0];
    char* second_register = params[1];
    eliminarEspaciosBlanco(second_register);
    printf("Me llegaron los registros: %s, %s\n", first_register, second_register);

    REGISTER *register_target = find_register(first_register);
    REGISTER *register_origin = find_register(second_register);

   if (register_target->type == TYPE_UINT32 && register_origin->type == TYPE_UINT32){
        printf("Valor registro origen: %d\nValor registro destino: %d\n", *(uint32_t *)register_origin->registro, *(uint32_t *)register_target->registro);
        *(uint32_t *)register_target->registro += *(uint32_t *)register_origin->registro;
        printf("Suma realizada y almacenada en %s, valor actual: %d\n", first_register, *(uint32_t *)register_target->registro);
    }
    else if (register_target->type == TYPE_UINT8 && register_origin->type == TYPE_UINT8){
        printf("Valor registro origen: %d\nValor registro destino: %d\n", *(uint8_t *)register_origin->registro, *(uint8_t *)register_target->registro);
        *(uint8_t *)register_target->registro += *(uint8_t *)register_origin->registro;
        printf("Suma realizada y almacenada en %s, valor actual: %d\n", first_register, *(uint8_t *)register_target->registro);
    }
    else
    {
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void sub(char **params)
{
    printf("Ejecutando instruccion SUB\n");
    char* first_register = params[0];
    char* second_register = params[1];
    eliminarEspaciosBlanco(second_register);
    printf("Me llegaron los registros: %s, %s\n", first_register, second_register);

    REGISTER *register_target = find_register(first_register);
    REGISTER *register_origin = find_register(second_register);

     if (register_target->type == TYPE_UINT32 && register_origin->type == TYPE_UINT32){
        printf("Valor registro origen: %d\nValor registro destino: %d\n", *(uint32_t *)register_origin->registro, *(uint32_t *)register_target->registro);
        *(uint32_t *)register_target->registro -= *(uint32_t *)register_origin->registro;
        printf("Resta realizada y almacenada en %s, valor actual: %d\n", first_register, *(uint32_t *)register_target->registro);
    }
    else if (register_target->type == TYPE_UINT8 && register_origin->type == TYPE_UINT8){
        printf("Valor registro origen: %d\nValor registro destino: %d\n", *(uint8_t *)register_origin->registro, *(uint8_t *)register_target->registro);
        *(uint8_t *)register_target->registro -= *(uint8_t *)register_origin->registro;
        printf("Resta realizada y almacenada en %s, valor actual: %d\n", first_register, *(uint8_t *)register_target->registro);
    }
    else{
        printf("Alguno de los registros no fue encontrado\n");
    }
}

void jnz(char **params)
{
    printf("Ejecutando instruccion JNZ\n");
    printf("Me llegaron los parametros: %s\n", params[0]);

    const char *register_name = params[0];
    const int next_instruction = atoi(params[1]);

    REGISTER *found_register = find_register(register_name);

    if (found_register != NULL)
    {
        if (found_register->registro != 0)
        {
            contexto->registros->PC = next_instruction;
        }
    }
    else
    {
        printf("Registro no encontrado o puntero nulo\n");
    }
}

// TODO
void mov(char **)
{
}

void resize(char **tamanio_a_modificar)
{
    char* tamanio=tamanio_a_modificar[0];
    printf("El tamanio elegido es: %d",atoi(tamanio));
    paqueteRecurso(conexion_memoria,contexto,tamanio,REZISE);
}

void copy_string(char **)
{

}

void WAIT(char **params){
    char* name_recurso = params[0];
    printf("Pidiendo a kernel wait del recurso %s", name_recurso);
    paqueteRecurso(cliente_fd_dispatch, contexto, name_recurso, O_WAIT);
}

void SIGNAL(char **params)
{
    char* name_recurso = params[0];
    printf("Pidiendo a kernel wait del recurso %s", name_recurso);
    paqueteRecurso(cliente_fd_dispatch, contexto, name_recurso, O_SIGNAL);
}

void io_gen_sleep(char **params)
{
    printf("Ejecutando instruccion IO_GEN_SLEEP\n");
    printf("Me llegaron los parametros: %s, %s\n", params[0], params[1]);

    char *interfaz_name = params[0];
    char **tiempo_a_esperar = &params[1]; // el & es para q le pase la direccion y pueda asignarlo como char**, y asi usarlo en solicitar_interfaz (gpt dijo)
    // enviar a kernel la peticion de la interfaz con el argumento especificado, capaz no hace falta extraer cada char* de params, sino enviar todo params
    solicitar_interfaz(interfaz_name, "IO_GEN_SLEEP", tiempo_a_esperar);
}

void io_stdin_read(char ** params)
{
    printf("Ejecutando instruccion IO_STDIN_READ\n");
    printf("Me llegaron los parametros: %s, %s, %s\n", params[0], params[1], params[2]);

    char *interfaz_name = params[0];
    char *registro_direccion = params[1];
    char *registro_tamanio = params[2];
    char **args = &params[1];

    solicitar_interfaz(interfaz_name, "IO_STDIN_READ", args);
}

void mov_in(char **params)
{
    printf("Ejecutando instruccion MOV_IN\n");
    char* registro_datos = params[0];
    char* registro_direccion = params[1];

    paquete_leer_memoria(conexion_memoria, contexto->PID, registro_direccion);

    sem_wait(&sem_leer_memoria);
    
    REGISTER *found_register = find_register(found_register);

    if(found_register == NULL) {
        log_error(logger_cpu, "No se encontro el registro");
        return;
    }

    if (found_register->type == TYPE_UINT32){
        *(uint32_t *)found_register->registro = memoria_response;
        printf("Valor del registro %s actualizado a %d\n", found_register, *(uint32_t *)found_register->registro);
    }
    else if (found_register->type == TYPE_UINT8){
        *(uint8_t *)found_register->registro = (uint8_t)memoria_response;
        printf("Valor del registro %s actualizado a %d\n", found_register, *(uint8_t *)found_register->registro);
    }
    else{
        printf("Registro desconocido: %s\n", found_register);
    }
    found_register = NULL;
    free(found_register);

}

void mov_out(char **params)
{

}

void io_stdout_write(char ** params)
{
    printf("Ejecutando instruccion IO_STDOUT_WRITE\n");
    printf("Me llegaron los parametros: %s, %s, %s\n", params[0], params[1], params[2]);

    char *interfaz_name = params[0];
    char *registro_direccion = params[1];
    char *registro_tamanio = params[2];
    char **args = &params[1];

    solicitar_interfaz(interfaz_name, "IO_STDOUT_WRITE", args);
}

void io_fs_create(char**){
    
}

void io_fs_delete(char**){
    
}

void io_fs_trucate(char**){
    
}

void io_fs_read(char**){
    
}

void io_fs_write(char**){
    
}

void EXIT(char **params)
{
    log_info(logger_cpu, "Se finalizo la ejecucion de las instrucciones. Devolviendo contexto a Kernel...\n");
    enviar_contexto_pcb(cliente_fd_dispatch, contexto, CONTEXTO);
}

void solicitar_interfaz(char *interfaz_name, char *solicitud, char **argumentos)
{
    SOLICITUD_INTERFAZ* aux = malloc(sizeof(SOLICITUD_INTERFAZ));
    aux->nombre = strdup(interfaz_name);
    aux->solicitud = strdup(solicitud);
    aux->args = malloc(sizeof(argumentos));

    int argumentos_a_copiar = sizeof(aux->args) / sizeof(aux->args[0]);  
    for (int i = 0; i < argumentos_a_copiar; i++)
    {
        aux->args[i] = strdup(argumentos[i]);
    }
    paqueteIO(cliente_fd_dispatch, aux, contexto);
}

const char *motivos_de_salida[11] = {"EXIT", "IO_GEN_SLEEP", "IO_STDIN_WRITE", "IO_STDOUT_READ", "WAIT", "SIGNAL", "IO_FS_CREATE", "IO_FS_DELETE", "IO_FS_TRUNCATE", "IO_FS_WRITE", "IO_FS_READ"};

bool es_motivo_de_salida(const char *command)
{
    int num_commands = sizeof(motivos_de_salida) / sizeof(motivos_de_salida[0]);
    for (int i = 0; i < num_commands; i++)
    {
        if (strcmp(motivos_de_salida[i], command) == 0)
        {
            flag_interrupcion = false;
            return true;
        }
    }
    return false;
}

char* traducirDireccionLogica(int direccionLogica) {
    int numeroPagina = floor(direccionLogica / tam_pagina);
    int desplazamiento = direccionLogica - numeroPagina * tam_pagina;

    TABLA_PAGINA* tabla_pagina = contexto->registros->PTBR;
    PAGINA* pag = list_get(tabla_pagina->paginas, numeroPagina);

    char *s1 = string_itoa(pag->marco);
    char *s2 = string_itoa(desplazamiento);
    
    char* direccionFisica = strcat(s1, s2);

    return direccionFisica;
}

char* mmu (char* direccion_logica){
    // Direcciones de 12 bits -> 1 | 360
    int direccion_logica_int = atoi(direccion_logica);
    return traducirDireccionLogica(direccion_logica_int);
}


TLB *inicializar_tlb(int entradas) {
    TLB *tlb = malloc(sizeof(TLBEntry) * entradas);
    tlb->entradas = list_create();
    return tlb;
}

bool es_pid_pag(char* pid, char* pag, void* data) {
    TLBEntry* a_buscar = (TLBEntry*)data;
    return (a_buscar->pid == atoi(pid) && a_buscar->pagina == atoi(pag));
}


int chequear_en_tlb(char* pid, char* pagina) {
    bool es_pid_pag_aux(void* data){
        return es_pid_pag(pid, pagina, data);
    };

    TLBEntry* tlbentry = list_find(tlb->entradas, es_pid_pag_aux);

    if(tlbentry != NULL) {
        return tlbentry->marco;
    }

    return -1;
}