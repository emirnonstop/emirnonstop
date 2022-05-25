#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <unistd.h>
#include <fcntl.h>
#define PERMS 0666

struct {
    long mtype;
    char msg[1];    
} MessageFather;

struct {
    long mtype;
    long msg;    
} MessageSon ;

char *getString(int dec, int *length) {
    *length = 0;
    int capacity = 1;
    char *str = (char *)malloc(sizeof(char));
    char c ;
    while (read(dec, &c, sizeof(char))){ 
        str[(*length)++] = c;
        if (*length >= capacity) {
            capacity *= 2; 
            str = (char*)realloc(str, capacity * sizeof(char));
        }
        if (c == '\n'){ 
            break;
        }
    }
    str[*length] = '\0';
    return str;
}

int main(int argc, char *argv[]){ 
    int fdec, length, shmid, msgid;
    key_t keyMsg, keyShm;
    pid_t pid, pid1;
    char *shmaddr; 
    char *str;
    if (!(fdec = open(argv[1], O_RDONLY, 0600))){
        printf("error: cannot open file\n");
        return 0;
    }
    keyMsg = ftok("/usr", 'r');
    if (keyMsg < 0){
        printf("error: generating a key\n");
        return 0;
    }
    keyShm = ftok("/usr", 'a');
    if (keyShm < 0){
        printf("error: generating a key\n");
        return 0;
    }
    if ((pid = fork()) < 0){ 
        perror("fork");
        return 0;
    }
    if (!pid){ 
        //son 
        while (1){ 
            pid = getpid();
            msgid = msgget(keyMsg, 0666|IPC_CREAT);
            if (msgid == -1){ 
                printf("error: creating messages");
            }
            msgrcv(msgid, &MessageFather, sizeof(char) + 1, pid, 0);
            if (MessageFather.msg[0] == '0') break;
            shmid = shmget(keyShm, 256, 0666);
            if (shmid == -1){ 
                printf("error: creating a shared memory");
            }
            shmaddr = shmat(shmid, 0, 0);   
            long res = 0;
            int i = 0;
            while (*(shmaddr + i) != '\0'){ 
                if (*(shmaddr + i) == ' ') res++;
                i++;
            }
            MessageSon.msg = res;
            MessageSon.mtype = 1;
            msgsnd(msgid, &MessageSon, sizeof(MessageSon) - sizeof(long), 0);
        }
        shmdt(shmaddr);
        exit(0);
    } else { 
        if ((pid1 = fork()) < 0){ 
            perror("fork");
            return 0;
        } 
        if (!pid1){ 
            //son
            while (1){ 
                pid1= getpid();
                msgid = msgget(keyMsg, 0666|IPC_CREAT);
                if (msgid == -1){ 
                    printf("error: creating messages");
                }
                msgrcv(msgid, &MessageFather, sizeof(char) + 1, pid1, 0);
                if (MessageFather.msg[0] == '0') break;
                shmid = shmget(keyShm, 256, 0666);
                if (shmid == -1){ 
                    printf("error: creating a shared memory");
                }
                shmaddr = shmat(shmid, 0, 0);   
                long res = 0;
                int i = 0;
                while (*(shmaddr + i) != '\0'){ 
                    if ((*(shmaddr + i) >= '0') && (*(shmaddr + i) <= '9')) res++;
                    i++;
                }
                MessageSon.msg = res;
                MessageSon.mtype = 2;
                msgsnd(msgid, &MessageSon, sizeof(MessageSon) - sizeof(long), 0);
            }
            shmdt(shmaddr);
            exit(0);
            
        } else { 
            //father
            str = getString(fdec, &length);
            shmid = shmget(keyShm, 256, 0666|IPC_CREAT); //создали разделяемую память
            if (shmid == -1){ 
                printf("error: creating a shared memory");
            }
            shmaddr = shmat(shmid, 0, 0);                      //получили доступ к памяти

            msgid = msgget(keyMsg, 0666|IPC_CREAT);
            if (msgid == -1){ 
                printf("error: creating messages");
            }
            int numberOfStr = 0;
            while (strcmp(str, "\0") != 0){ 
                numberOfStr++;
                //записали в разделяемую память строку
                strcpy(shmaddr, str);
                MessageFather.mtype = pid;
                MessageFather.msg[0] = '1';
                //отрправили сообщение сыну что строка в разделяемой памяти 
                msgsnd(msgid, (&MessageFather), strlen(MessageFather.msg) + 1, 0);
                MessageFather.mtype = pid1;
                //отрправили сообщение сыну что строка в разделяемой памяти 
                msgsnd(msgid, (&MessageFather), strlen(MessageFather.msg) + 1, 0);
                //ждем сообщения, что строка обработана первым сыном 
                msgrcv(msgid, (&MessageSon), sizeof(MessageSon) - sizeof(long), 1, 0);
                printf("line:%2d || ", numberOfStr);
                printf("number of spaces:%3ld || ", MessageSon.msg);
                //ждем сообщения, что строка обработана вторым сыном
                msgrcv(msgid, (&MessageSon), sizeof(MessageSon) - sizeof(long), 2, 0);
                printf("number of digits:%3ld\n", MessageSon.msg);
                free(str);
                str = getString(fdec, &length);
            } 
            MessageFather.mtype = pid;
            MessageFather.msg[0] = '0';
            //отрправили сообщение сыну что строка в разделяемой памяти 
            msgsnd(msgid, (&MessageFather), strlen(MessageFather.msg) + 1, 0);
            wait(NULL);
            MessageFather.mtype = pid1;
            //отрправили сообщение сыну что строка в разделяемой памяти 
            msgsnd(msgid, (&MessageFather), strlen(MessageFather.msg), 0);
            wait(NULL);
            msgctl(msgid, IPC_RMID, NULL);
            shmdt(shmaddr);
            shmctl(shmid, IPC_RMID, NULL);
        }
    }
    return 0;
}