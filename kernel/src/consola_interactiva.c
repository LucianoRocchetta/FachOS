#include <kernel.h>

extern char *getwd ();
extern char *xmalloc ();

COMMAND commands[] = {
  { "EJECUTAR_SCRIPT", ejecutar_script, "Abrir archivo de comandos a ejecutar" },
  { "INICIAR_PROCESO", iniciar_proceso, "Crea PCB en estado NEW" },
  { "FINALIZAR_PROCESO", finalizar_proceso, "Finaliza proceso del sistema" },
  { "DETENER_PLANIFICACION", detener_planificacion, "Pausar planificacion a corto y largo plazo" },
  { "INICIAR_PLANIFICACION", iniciar_planificacion, "Reanuda la planificacion a corto y largo plazo" },
  { "MULTIPROGRAMACION", multiprogramacion, "Modifica el grado de multiprogramacion por el valor dado" },
  { "PROCESO_ESTADO", proceso_estado, "Lista procesos por estado en la consola" },
  { (char*)NULL, (Function*)NULL, (char *)NULL }
};


/* When non-zero, this global means the user is done using this program. */
int done;

char* dupstr (char* s){
  char *r;

  r = xmalloc (strlen(s) + 1);
  strcpy (r, s);
  return (r);
}

/* Execute a command line. */
int execute_line (char *line, t_log* logger)
{
  register int i = 0;
  register int j = 0;
  COMMAND *command;
  char param[25];
  char word[25];

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

  printf("Comand: %s\n", word);
  printf("Param: %s\n", param);

  /* Isolate the command word. */
  /*
  i = 0;
  while (line[i] && whitespace (line[i]))
    i++;
  word = line + i;

  while (line[i] && !whitespace (line[i]))
    i++;

  if (line[i])
    line[i++] = '\0';
  */

  command = find_command(word);

  if (!command)
    {
      log_error(logger, "%s: No se pudo encotrar ese comando\n", word);
      return (-1);
    }

  /* Get argument to command, if any. */
  /*
  while (whitespace (line[i]))
    i++;

  word = line + i;

  /* Call the function. */
  return ((*(command->func)) (param));
}

/* Look up NAME as the name of a command, and return a pointer to that
   command.  Return a NULL pointer if NAME isn't a command name. */
COMMAND* find_command (char* name){
  register int i;

  for (i = 0; commands[i].name; i++)
    if (strcmp (name, commands[i].name) == 0)
      return (&commands[i]);

  return ((COMMAND *)NULL);
}

/* Strip whitespace from the start and end of STRING.  Return a pointer
   into STRING. */
char* stripwhite (char* string){
  register char *s, *t;

  for (s = string; whitespace (*s); s++);
  
  if (*s == 0){
    return (s);
  }else{
    t = s + strlen (s) - 1;
    while (t > s && whitespace (*t)){
      t--;
    }
    *++t = '\0';
    return s;
  }

}