#ifndef _TASKREGS_H
#define _TASKREGS_H

#define MAXCLIENT       100


/*
 *  Connected clients information
 */
#define RB      5

typedef struct _reg_info {
        pid_t           pid;                    /* Pid or zero if free  */
        char            tty[7];                 /* Connected tty        */
        char            uname[36];              /* User name            */
        char            prg[15];                /* Program name         */
        char            city[36];               /* Users city           */
        char            doing[36];              /* What is going on     */
        time_t          started;                /* Startime connection  */
        time_t          lastcon;                /* Last connection      */
        int             altime;                 /* Alarm time           */
        unsigned        silent          : 1;    /* Do not disturb       */
        unsigned        chatting        : 1;    /* User is chatting     */
        unsigned        ismsg           : 1;    /* Message waiting      */
        int             channel;                /* Chat channel         */
        int             ptr_in;                 /* Input buffer pointer */
        int             ptr_out;                /* Output buffer ptr    */
        char            fname[RB][36];          /* Message from user    */
        char            msg[RB][81];            /* The message itself   */
} reg_info;


void	reg_init(void);
int	reg_newcon(char *);
int	reg_closecon(char *);
void	reg_check(void);
int	reg_doing(char *);
int	reg_nop(char *);
int	reg_timer(int, char *);
int	reg_tty(char *);
int	reg_user(char *);
int	reg_silent(char *);
char	*reg_ipm(char *);
int	reg_spm(char *);
char	*reg_fre(void);
char	*get_reginfo(int);

#endif

