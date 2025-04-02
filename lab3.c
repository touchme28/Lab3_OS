#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <mntent.h>
#include <signal.h>

#include <errno.h>

int pipe_fd[2];
int flag1;
int flag2;

void* proc1(void* arg) {
    int* flag1 = (int*) arg;
    //printf("Starting the proc1 for writing to buffer...\n");

    char buffer[256];
    FILE *filep;
    struct mntent *mnt;

    filep = setmntent("/etc/mtab", "r");
    if (filep == NULL) {
        perror("setmntent");
        return NULL;
    }

    while (*flag1 == 0) {
        mnt = getmntent(filep);
        //reseting the getmntent if 
        if (mnt == NULL) {
            rewind(filep);
            continue;
        }

        //record in buffer
        snprintf(buffer, sizeof(buffer), "%s %s %s %s\n",
                 mnt->mnt_fsname, mnt->mnt_dir, mnt->mnt_type, mnt->mnt_opts);
        //+1 cuz of '\0' 
        ssize_t rv = write(pipe_fd[1], buffer, strlen(buffer) + 1);
        if (rv == -1) {
            perror("write");
        }

        sleep(1);
    }

    //printf("End of the proc1.\n");
    endmntent(filep);
}

void* proc2(void* arg) {
    int* flag2 = (int*) arg;
    //printf("Starting the proc2 for reading the buffer...\n");

    char buffer[256];
    while (*flag2 == 0) {
        //cleansing buffer
        memset(buffer, 0, sizeof(buffer));
        size_t rv = read(pipe_fd[0], buffer, sizeof(buffer));
        if (rv == -1) {
            if ( errno == EAGAIN || errno == EWOULDBLOCK){
                usleep(100000);
                perror("read");
                continue;
            } else {
                perror("read");
                break;
            }
        } else {
            printf("Received: %s", buffer);
        }
    }

    //printf("End of the proc2.\n");
}

void handle_sigint(int sig) {
    printf("\nget SIGINT; %d\n", sig);
    flag1 = 1;
    flag2 = 1;
    exit(0);
}

int main(int argc, char *argv[]) {
    pthread_t id1, id2;
    flag1 = 0; flag2 = 0;

    if (argc != 2) {
        printf("You need to write: %s 1/2/3\n", argv[0]);
        return 1;
    }

    switch (atoi(argv[1])) {
        case 1:
            int rv_pipe = pipe(pipe_fd);
            if (rv_pipe == -1) {
                perror("pipe");
                return 1;
            }
            break;
        case 2:
            int rv_pipe2 = pipe2(pipe_fd, O_NONBLOCK);
            if (rv_pipe2 == -1) {
                perror("pipe2");
                return 1;
            }
            break;
        case 3:
            int rv_pipe_fcntl = pipe(pipe_fd);
            if (rv_pipe_fcntl == -1) {
                perror("pipe");
                return 1;
            }
            fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK);
            fcntl(pipe_fd[1], F_SETFL, O_NONBLOCK);
            break;
        default:
            printf("Invalid argument\n");
            return 1;
    }

    signal(SIGINT, handle_sigint);

    pthread_create(&id1, NULL, proc1, &flag1);
    pthread_create(&id2, NULL, proc2, &flag2);

    getchar();

    flag1 = 1;
    flag2 = 1;

    pthread_join(id1, NULL);
    pthread_join(id2, NULL);

    close(pipe_fd[0]);
    close(pipe_fd[1]);

    return 0;
}