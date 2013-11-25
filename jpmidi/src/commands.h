
#ifndef __commands_h__
#define __commands_h__

/* Command parsing based on GNU readline info examples. */
typedef void cmd_function_t(char *);	/* command function type */

/* Transport command table. */
typedef struct {
	char *name;			/* user printable name */
	cmd_function_t *func;		/* function to call */
	char *doc;			/* documentation  */
} command_t;

/* 
functions */
#ifdef __cplusplus
extern "C" {
#endif

extern int execute_command(char *line);
extern char *command_generator (const char *text, int state);

#ifdef __cplusplus
}
#endif

#endif /* __commands_h__ */

