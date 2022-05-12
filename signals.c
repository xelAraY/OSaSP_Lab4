#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <wait.h>
#include <stdlib.h>
#include <wchar.h>
#include <locale.h>
#include <time.h>

#define PAIRS_COUNT 9
#define PROCESSES_COUNT 9
#define NOSIG -1
#define SIGDEF -2

typedef struct proccPair {
    int create;
    int signal;
    int from;
    int to;
} Pair;

Pair pairs[] = {
    {1, NOSIG, 0, 1},
    {1, SIGUSR1, 1, 2},
    {1, SIGUSR1, 1, 3},
    {1, SIGUSR1, 1, 4},
    {1, SIGUSR1, 1, 5},
    {1, SIGUSR1, 5, 6},
    {1, SIGUSR1, 5, 7},
    {1, SIGUSR1, 5, 8},
    {0, SIGUSR1, 8, 1}
};

pid_t pids[PROCESSES_COUNT];
int nodeIndex;
int sendedSignalsCount;

void sigusr(int);
void sigterm();
int signalChildren(int index, int sig);
void outputInfo(char *status, int sigNum);

int main() {
    pids[0] = getpid();

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;

    int pgid = 0;

    for (int i = 0; i < PAIRS_COUNT; ++i) {
        if(i == 0) {
            pgid = 0;
        }

        if(nodeIndex == pairs[i].to && pairs[i].signal != NOSIG) {
            signal(pairs[i].signal, sigusr);
        }

        if(nodeIndex == pairs[i].from && pairs[i].create) {
            pid_t pid;
            switch (pid = fork()) {
                case -1: {
                    break;
                }
                case 0: {
                    nodeIndex = pairs[i].to;
                    pids[nodeIndex] = getpid();
                    i = -1;

                    signal(SIGTERM, sigterm);
                    break;
                }
                default: {
                    if(!pgid)
                        pgid = pid;
                    setpgid(pid, pgid);
                    pids[pairs[i].to] = pid;

                    break;
                }
            }
        }
    }
    if(nodeIndex == 0)
        printf("INDEX PID   PPID  STATUS SIGNAL TIME\n");

    sleep(1);

    if(nodeIndex == 1){
        int sig = signalChildren(nodeIndex, SIGDEF);
        if(sig != NOSIG) {
            sendedSignalsCount++;
        }
    }

    if(nodeIndex == 0){
        getchar();
        while(wait(NULL) != -1);
        return 0;
    }
    else {
        while(1)
            pause();
    }
}

void sigusr(int sig) {
    outputInfo("get", 1);

    if(nodeIndex == 1 && sendedSignalsCount == 101) {
        kill(getpid(), SIGTERM);
        return;
    }

    int signal = signalChildren(nodeIndex, SIGDEF);
    if(signal != NOSIG) {
        outputInfo("put", 1);
        sendedSignalsCount++;
    }
}

int tryToGetChildrenPGID(int index, pid_t *pgid, int *sig) {
    for (int i = 0; i < PAIRS_COUNT; ++i) {
        if (pairs[i].from == index) {
            *pgid = getpgid(pids[pairs[i].to]);
            if(sig != NULL)
                *sig = pairs[i].signal;
            return 1;
        }
    }

    return 0;
}

int signalChildren(int index, int sig) {
    int s;
    pid_t pgid;
    if(tryToGetChildrenPGID(index, &pgid, &s)) {
        int signal = sig == SIGDEF ? s : sig;
        kill(-pgid, signal);
        return signal;
    }

    return NOSIG;
}

void outputInfo(char *status, int sigNum) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    time_t sec = ts.tv_sec;
    int ms = ts.tv_nsec / 1e6;

    struct tm *tmp = localtime(&sec);

    printf("%-5d %-5d %-5d %6s   USR%1d %02i:%02i:%02i:%03i\n",
           nodeIndex, getpid(), getppid(), status, sigNum,
           tmp->tm_hour, tmp->tm_min, tmp->tm_sec, ms);
    fflush(stdout);
}

void sigterm() {
    printf("%-5d %-5d %-5d has terminated. Sended: %4d SIGUSR1\n",
           nodeIndex, getpid(), getppid(), sendedSignalsCount);

    pid_t pgid;
    if(tryToGetChildrenPGID(nodeIndex, &pgid, NULL)) {
        kill(-pgid, SIGTERM);
        waitpid(-pgid, NULL, 0);
    }

    exit(0);
}
