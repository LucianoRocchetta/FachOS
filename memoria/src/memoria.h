#ifndef MEMORIA_H_
#define MEMORIA_H_

#include <utils/utils.h>
//STRUCTS
typedef struct{
    char* instruccion;
}inst_pseudocodigo;
typedef struct {
    unsigned int marcos;
    bool bit_validacion;
}PAGINA;

typedef struct{
    PAGINA* paginas;
}TABLA_PAGINA;
typedef struct {
    void* data;
} MARCO_MEMORIA;

typedef struct {
    MARCO_MEMORIA *marcos;
    int numero_marcos;
} MEMORIA;
typedef struct{
    int pid;
    TABLA_PAGINA* tabla_pagina;
}TABLAS;

//MEMORIA
void resetear_memoria(MEMORIA*);

//PAGINADO
uint32_t* inicializar_tabla_pagina();
void lista_tablas(TABLA_PAGINA*);
void destruir_pagina(void*);
void destruir_tabla(int);
void tradurcirDireccion();
void guardar_en_memoria(MEMORIA*,t_list*);

//PSEUDOCODIGO
int enlistar_pseudocodigo(char* path_instructions, char* ,t_log*, t_list*);
void iterar_lista_e_imprimir(t_list*);

//CONEXIONES
void* gestionar_llegada_memoria_cpu(void*);
void* gestionar_llegada_memoria_kernel(void*);
void enviar_instrucciones_a_cpu(char*, int);

//PROCESOS
pcb* crear_pcb(char* instrucciones);
void destruir_pcb(pcb*);
void destruir_instrucciones(void*);

#endif