#ifndef UTILS_H_
#define UTILS_H_

#include<utils/utils.h>

#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<netdb.h>
#include<string.h>
#include<errno.h>
#include<pthread.h>
#include<semaphore.h>

#include<sys/types.h>
#include<sys/stat.h>
#include<sys/socket.h>
#include<sys/file.h>

#include<commons/txt.h>
#include<commons/string.h>
#include<commons/log.h>
#include<commons/config.h>
#include<commons/collections/list.h>
#include<commons/collections/queue.h>
#include<commons/process.h>
#include<commons/memory.h>

#include<readline/readline.h>
#include<readline/history.h>

typedef enum operaciones{
	MENSAJE,
	CONTEXTO,
	INSTRUCCION,
	INTERRUPCION,
	CREAR_PROCESO,
	FINALIZAR_PROCESO,
	CARGAR_INSTRUCCIONES,
	DESCARGAR_INSTRUCCIONES,
	NUEVA_IO,
	SOLICITUD_IO,
	DESCONECTAR_IO,
	DESCONECTAR_TODO,
	DESBLOQUEAR_PID,
	IO_GENERICA,
	IO_STDIN,
	IO_STDOUT,
	IO_DIALFS,
	RECURSO,
}op_code;

typedef struct{
	int size;
	void* stream;
} t_buffer;

typedef struct{
  char* nombre;
  int instancia;
}t_recurso;

typedef struct{
	op_code codigo_operacion;
	t_buffer* buffer;
} t_paquete;

typedef enum SALIDAS{
	FIN_INSTRUCCION,
	QUANTUM,
	INTERRUPTED,
	IO
}MOTIVO_SALIDA;

typedef enum INTERFACES{
  GENERICA,
  STDIN,
  STDOUT,
  DIAL_FS
}TIPO_INTERFAZ;

typedef enum ESTADO_INTERFAZ{
	LIBRE,
	OCUPADA
}estados_interfaz;

typedef struct registroCPU{
	uint32_t PC;		// Program counter
	uint8_t AX;			// registro númerico de propósito general
	uint8_t BX;			// registro númerico de propósito general
	uint8_t CX;			// registro númerico de propósito general
	uint8_t DX;			// registro númerico de propósito general
	uint32_t EAX;		// registro númerico de propósito general
	uint32_t EBX;		// registro númerico de propósito general
	uint32_t ECX;		// registro númerico de propósito general
	uint32_t EDX;		// registro númerico de propósito general
	uint32_t SI;		// dirección lógica de memoria de origen desde donde se va a copiar un string
	uint32_t DI;		// dirección lógica de memoria de destino desde donde se va a copiar un string
	uint32_t PTBR;      // Page Table Base Register. Almacena el puntero hacia la tabla de pagina de un proceso.
    uint32_t PTLR;      // Page Table Length Register. Sirve para delimitar el espacio de memoria de un proceso.
}regCPU;

typedef struct contexto{
	int PID;
	int quantum;
	regCPU* registros;
	MOTIVO_SALIDA motivo;
}cont_exec;

typedef struct pcb{
	cont_exec* contexto;
	char* estadoAnterior;
	char* estadoActual;
	char* path_instrucciones;
}pcb;

typedef struct SOLICITUD_INTERFAZ{
  char* nombre;
  char* solicitud;
  char** args;
  char* pid;
} SOLICITUD_INTERFAZ;

typedef struct NEW_INTERFACE{
	char* nombre;
    TIPO_INTERFAZ tipo;
    char** operaciones;
}DATOS_INTERFAZ;

typedef struct {
    DATOS_INTERFAZ* datos;
    t_config *configuration;
	estados_interfaz estado;	// creo que es reemplazable con un semaforo inicializado en 1
	SOLICITUD_INTERFAZ* solicitud;		// esto no deberia estar en la interfaz
} INTERFAZ;

typedef struct {
	char* pid;
	char* nombre;
}desbloquear_io;

typedef struct interfaz_con_hilo{
	INTERFAZ* interfaz;
	pthread_t hilo_interfaz;
}INTERFAZ_CON_HILO;

// FUNCIONES UTILS 

t_log* iniciar_logger(char* log_path, char* log_name, t_log_level log_level);
t_config* iniciar_config(char* config_path);
void terminar_programa(t_log* logger, t_config* config);
void eliminarEspaciosBlanco(char*);
bool es_nombre_de_interfaz(char*, void*);
bool es_nombre_de_interfaz_io(char*, void*);
void buscar_y_desconectar(char*, t_list*, t_log*);
void buscar_y_desconectar_io(char*, t_list*, t_log*);	// es para desconectar un INTERFAZ_CON_HILO, ya que es distinto entre IO y kernel
void destruir_interfaz(void*);
void destruir_interfaz_io(void*);
void liberar_memoria(char**, int); 
void eliminar_io_solicitada(SOLICITUD_INTERFAZ* io_solicitada);

// FUNCIONES CLIENTE

int crear_conexion(char* ip, char* puerto);
void enviar_operacion(char* mensaje, int socket_cliente, op_code);
t_paquete* crear_paquete(op_code);
void agregar_a_paquete(t_paquete* paquete, void* valor, int tamanio);
void enviar_paquete(t_paquete* paquete, int socket_cliente);
void liberar_conexion(int socket_cliente);
void eliminar_paquete(t_paquete* paquete);

// PAQUETES DE DATOS (Repiten logica pero cada una es para mandar datos distinto hacia o desde lugares distintos)

void enviar_solicitud_io(int , SOLICITUD_INTERFAZ*, op_code);
void paqueteDeMensajes(int, char*, op_code);
void paqueteDeDesbloqueo(int conexion, desbloquear_io *solicitud);
void paqueteRecurso(int, char*, int);
void peticion_de_espacio_para_pcb(int, pcb*, op_code);
void peticion_de_eliminacion_espacio_para_pcb(int, pcb*, op_code);
void paqueteIO(int, SOLICITUD_INTERFAZ*, cont_exec*);
void paquete_nueva_IO(int, INTERFAZ*);
void enviar_contexto_pcb(int, cont_exec*, op_code);

// FUNCIONES SERVER
typedef struct {
    t_log* logger;
    char* puerto_escucha;
} ArgsAbrirServidor;

extern t_log* logger;

typedef struct gestionar{
	t_log* logger;
	int cliente_fd;
}ArgsGestionarServidor;

typedef struct gestionar_interfaz{
	t_log* logger;
	int cliente_fd;
	INTERFAZ interfaz;
}ArgsGestionarHiloInterfaz;

typedef struct consola{
	t_log* logger;	
}ArgsLeerConsola;

void* recibir_buffer(int*, int);
int iniciar_servidor(t_log* logger, char* puerto_escucha);
int esperar_cliente(int server_fd, t_log* logger);
t_list* recibir_paquete(int, t_log*);
void* recibir_mensaje(int, t_log*, op_code);
int recibir_operacion(int);

#endif