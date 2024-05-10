#include <cpu.h>

int conexion_memoria;

t_log* logger_cpu;
t_config* config;


INSTRUCTION instructions[] = {
  { "SET", NULL, set, "Abrir archivo de comandos a ejecutar" },
  { (char*)NULL, NULL, (Function*)NULL, (char *)NULL }
};

char* Fetch(contEXEC* contexec) {
  char* instruccion;

  enviar_mensaje(contexec->path_instrucciones, conexion_memoria); // Enviamos mensaje para mandarle el path que debe abrir
  int programCounter = contexec->registro.PC;

  sem_wait(solicitacion_instrucciones);
  enviar_instruccion(string_itoa(programCounter), conexion_memoria); // Enviamos instruccion para mandarle la instruccion que debe mandarnos

  instruccion = recibir_instruccion(conexion_memoria, logger_cpu);

  programCounter++;
  contexec->registro.PC = programCounter;

  return instruccion;
}

INSTRUCTION* find_instruction (char* name){
  register int i;

  for (i = 0; instructions[i].name; i++)
    if (strcmp (name, instructions[i].name) == 0)
      return (&instructions[i]);

  return ((INSTRUCTION *)NULL);
}

int execute_line (char *line, t_log* logger)
{
  register int i = 0;
  register int j = 0;
  INSTRUCTION* instruction;
  char *param;
  char word[30];

  while (line[i] && !isspace(line[i])) {
    word[i] = line[i];
    i++;
  }
  word[i] = '\0';

  while(line[i] && line[i] != '\0') {
    param[j] = line[i];
    i++;
    j++;
  }
  param[j] = '\0';

  printf("Instruction: %s\n", word);
  printf("Param: %s\n", param);

  instruction = find_instruction(word);

  if (!instruction)
    {
      log_error(logger, "%s: No se pudo encotrar esa instrucción\n", word);
      return (-1);
    }
 
  return ((*(instruction->func)) (param));
}


bool Decode(char* instruccion) {
    // Decode primero reconoce 


    // Despues ejecuta
    return 0;
}


void procesar_contexto(contEXEC* contexto){
    char* instruccion = Fetch(contexto);

    //TODO: La funcion main del procesado del contexto
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

    sem_init(solicitacion_instrucciones, 0, 0);

    ArgsGestionarServidor args_dispatch = {logger_cpu, cliente_fd_dispatch};
    ArgsGestionarServidor args_interrupt = {logger_cpu, cliente_fd_interrupt};
    ArgsGestionarServidor args_memoria = {logger_cpu, conexion_memoria};

    pthread_create(&hilo_id[0], NULL, gestionar_llegada_cpu, &args_dispatch);
    pthread_create(&hilo_id[1], NULL, gestionar_llegada_cpu, &args_interrupt);
    pthread_create(&hilo_id[2], NULL, gestionar_llegada_cpu, &args_memoria);

    for(i = 0; i<5; i++){
        pthread_join(hilo_id[i], NULL);
    }
    
    liberar_conexion(conexion_memoria);
    terminar_programa(logger_cpu, config);
    return 0;
}

void* gestionar_llegada_cpu(void* args){
	ArgsGestionarServidor* args_entrada = (ArgsGestionarServidor*)args;

	void iterator_adapter(void* a) {
		iterator_cpu(logger_cpu, (char*)a);
	};

    t_list* lista;
	while (1) {
		log_info(logger_cpu, "Esperando operacion...");
		int cod_op = recibir_operacion(args_entrada->cliente_fd);
		switch (cod_op) {
      case MENSAJE:
        recibir_mensaje(args_entrada->cliente_fd, logger_cpu);
        break;
      case PAQUETE:   // Se recibe el paquete del contexto del PCB
        contEXEC* contexto;
        lista = recibir_paquete(args_entrada->cliente_fd);
        contexto = list_get(lista, 0);
        procesar_contexto(contexto);
        break;
      case -1:
        log_error(logger_cpu, "el cliente se desconecto. Terminando servidor");
        return EXIT_FAILURE;
      default:
        log_warning(logger_cpu,"Operacion desconocida. No quieras meter la pata");
        break;
      }
	}
}

void iterator_cpu(t_log* logger_cpu, char* value){
	log_info(logger_cpu,"%s", value);
}

// Instrucciones

int set(){
  return 0;
}