#include <cpu.h>

int conexion_memoria;

contEXEC contexto;

t_log* logger_cpu;
t_config* config;


char* Fetch(contEXEC* contexec) {

    int PC = contexec->registro.PC;
    char* envio = string_new();
    string_n_append(&envio, string_itoa(PC),2);
    string_n_append(&envio, contexec->path_instrucciones , strlen(contexec->path_instrucciones));
    enviar_mensaje(envio, conexion_memoria);   
    PC++;
    contexec->registro.PC = PC;
    return recibir_instruccion(conexion_memoria, logger_cpu);
}


void* procesar_contexto(void* args){
    char* instruccion;

    instruccion = Fetch(contexto);
}


int main(int argc, char* argv[]) {   
    int i;
    char* config_path = "../cpu/cpu.config";
    
    logger_cpu = iniciar_logger("../cpu/cpu.log", "cpu-log", LOG_LEVEL_INFO);
    log_info(logger_cpu, "logger para CPU creado exitosamente.");

    config = iniciar_config(config_path);

    pthread_t hilo_id[4];

    // Get info from cpu.config
    char* ip_memoria = config_get_string_value(config,"IP_MEMORIA");
    char* puerto_memoria = config_get_string_value(config,"PUERTO_MEMORIA");
    char* puerto_dispatch = config_get_string_value(config,"PUERTO_ESCUCHA_DISPATCH");
    char* puerto_interrupt = config_get_string_value(config,"PUERTO_ESCUCHA_INTERRUPT");
    //char* cant_ent_tlb = config_get_string_value(config,"CANTIDAD_ENTRADAS_TLB");
    //char* algoritmo_tlb = config_get_string_value(config,"ALGORITMO_TLB");
    log_info(logger_cpu, "%s\n\t\t\t\t\t%s\t%s\t", "INFO DE MEMORIA", ip_memoria, puerto_memoria);

    // Abrir servidores
    int server_dispatch = iniciar_servidor(logger_cpu, puerto_dispatch);
    log_info(logger_cpu, "Servidor dispatch abierto");
    int server_interrupt = iniciar_servidor(logger_cpu, puerto_interrupt);
    log_info(logger_cpu, "Servidor interrupt abierto");
  
    conexion_memoria = crear_conexion(ip_memoria, puerto_memoria);
    enviar_mensaje("CPU IS IN DA HOUSE", conexion_memoria);

    int cliente_fd_dispatch = esperar_cliente(server_dispatch, logger_cpu);
    int cliente_fd_interrupt = esperar_cliente(server_interrupt, logger_cpu);

    ArgsGestionarServidor args_dispatch = {logger_cpu, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger_cpu, cliente_fd_interrupt};
    ArgsGestionarServidor args_memoria = {logger_cpu, conexion_memoria};

    pthread_create(&hilo_id[0], NULL, gestionar_llegada, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada, &args_interrupt);
    pthread_create(&hilo_id[2], NULL, gestionar_llegada, &args_memoria);
    pthread_create(&id_hilo[3], NULL, procesar_contexto, NULL);

    for(i = 0; i<5; i++){
        pthread_join(hilo_id[i], NULL);
    }
    
    liberar_conexion(conexion_memoria);
    terminar_programa(logger_cpu, config);
    return 0;
}

void* gestionar_llegada(void* args){
	ArgsGestionarServidor* args_entrada = (ArgsGestionarServidor*)args;

	void iterator_adapter(void* a) {
		iterator(args_entrada->logger_cpu, (char*)a);
	};

    t_list* lista;
	while (1) {
		log_info(args_entrada->logger_cpu, "Esperando operacion...");
		int cod_op = recibir_operacion(args_entrada->cliente_fd);
		switch (cod_op) {
		case MENSAJE:
			recibir_mensaje(args_entrada->cliente_fd, args_entrada->logger_cpu);
			break;
		case INSTRUCCION:
			instruccion = recibir_instruccion(args_entrada->cliente_fd, args_entrada->logger_cpu);
			break;
		case PAQUETE:
			lista = recibir_paquete(args_entrada->cliente_fd);
            (void*)contexto = list_get(lista, 0);
			break;
		case -1:
			log_error(args_entrada->logger_cpu, "el cliente se desconecto. Terminando servidor");
			return EXIT_FAILURE;
		default:
			log_warning(args_entrada->logger_cpu,"Operacion desconocida. No quieras meter la pata");
			break;
		}
	}
}

void iterator(t_log* logger_cpu, char* value){
	log_info(logger_cpu,"%s", value);
}
