#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define DEBUG 0 
#define INPUT_SIZE 6000
#define NOTIFICATION_SIZE 64
#define PROCESSES_COUNT 3  
#define SIGNALS_COUNT 64 
#define PIPES_COUNT 2     
#define FIRST_PIPE 0
#define SECOND_PIPE 1
#define MODE_INTERACTIVE 0  
#define MODE_FILE 1 
#define SIGNAL_STOP 20 
#define SIGNAL_RESUME 18
#define SIGNAL_END 15
#define NOTIFICATION_STOP "stop"
#define NOTIFICATION_RESUME "resume"
#define SHM_SEM_LOCK 3
#define READ 0
#define WRITE 1
char* sharedMemory;
int shmid;
void createSharedMemory(int size) {
    key_t key;

    if ((key = ftok(".", 'c')) == -1)  
        errx(1, "Blad tworzenia klucza!");
    if ((shmid = shmget(key, sizeof(char) * size, IPC_CREAT | 0666)) < 0) 
        errx(2, "Blad tworzenia segmentu pamieci dzielonej!");
    if ((sharedMemory = shmat(shmid, NULL, 0)) == (char*)-1)
        errx(3, "Blad przylaczania pamieci dzielonej!");

    if (DEBUG) {
        fprintf(stderr, "\nShared memory id: %d\n", shmid);
    }
}

void readFromSharedMemory(char* data) {
    //printf("Czytam z Shared memory, %s PID:%d\n", data, getpid());
    strcpy(data, sharedMemory); 
}

void writeToSharedMemory(char* data) {
    //printf("Zapisuje z Shared memory, %s PID:%d\n", data, getpid());
    strcpy(sharedMemory, data); 
}

void removeSharedMemory() {
    shmdt(sharedMemory);    
    shmctl(shmid, IPC_RMID, NULL); 
}
int sem[PROCESSES_COUNT + 1]; 

union semun {
    int val;
    struct semid_ds* buf;
    unsigned short int* array;
    struct seminfo* __buf;
} ctl;

void semlock(int semIndex) {
   // printf("Lockuje Semafor PID:%d\n", getpid());
    int semid = sem[semIndex];

    struct sembuf opr;
    opr.sem_num = 0;
    opr.sem_op = -1;
    opr.sem_flg = 0;
    while (semop(semid, &opr, 1) == -1) {
        ;
    }
}

void semunlock(int semIndex) {
    int semid = sem[semIndex];
    //printf("Odblokowuje Semafor PID:%d\n", getpid());
    struct sembuf opr;
    opr.sem_num = 0;
    opr.sem_op = 1;
    opr.sem_flg = 0;
    while (semop(semid, &opr, 1) == -1) {
        if (errno != EINTR) {
            warn("Blad blokowania semafora!");
            break;
        }
    }
}

int createSem(int seed) { 
    key_t key;
    int semid;
    if ((key = ftok(".", seed)) == -1) errx(1, "Blad tworzenia klucza!");
    if ((semid = semget(key, 1, IPC_CREAT | 0600)) == -1) errx(2, "Blad tworzenia semafora!");
    ctl.val = 0; 
    if (semctl(semid, 0, SETVAL, ctl) == -1) errx(3, "Blad ustawiania semafora!");
    return semid;
}

void createSemaphores() {
    int processIndex;
    for (processIndex = 0; processIndex < PROCESSES_COUNT + 1; processIndex++) {
        sem[processIndex] = createSem(processIndex);
    }
}

void removeSemaphores() {
    int processIndex;
    for (processIndex = 0; processIndex < PROCESSES_COUNT + 1; processIndex++) {
        semctl(sem[processIndex], 0, IPC_RMID, ctl); 
    }
}
struct mymsgbuf {
    long type;
    char data[INPUT_SIZE];
} msg;

int queue;

void createQueue() {
    key_t key = ftok(".", 'm');
    if ((queue = msgget(key, IPC_CREAT | 0660)) == -1) perror("Otwieranie_kolejki");
}

void removeQueue() {
    msgctl(queue, IPC_RMID, 0);
}

void writeToQueue1(char* data) {
    msg.type = 1;
    strcpy(msg.data, data);
    if (msgsnd(queue, &msg, sizeof(msg), 0) == -1) perror("Wysylanie");
}
void writeToQueue2(char* data) {
    msg.type = 2;
    strcpy(msg.data, data);
    if (msgsnd(queue, &msg, sizeof(msg), 0) == -1) perror("Wysylanie");
}
void readFromQueue1(char* data) {
     if (msgrcv(queue, &msg, sizeof(msg), 0, 0) == -1) perror("Odbieranie");
    if(msg.type==1)
    {strcpy(data, msg.data);}
}
void readFromQueue2(char* data) {
     if (msgrcv(queue, &msg, sizeof(msg), 0, 0) == -1) perror("Odbieranie");
    if(msg.type==2)
    {strcpy(data, msg.data);}
}
FILE* PIDs_file; 
int pids[PROCESSES_COUNT];

int next() {
    int pid = getpid();
    return (pid == pids[0]) + 2 * (int)(pid == pids[1]);
}
int previous() { 
    int pid = getpid();
    return (pid == pids[2]) + 2 * (int)(pid == pids[0]);
}
int getSavedPid(int processIndex) {
    return pids[processIndex];
}

void savePid(int processIndex, int pid) {
    pids[processIndex] = pid;
}

void exportPids() { 
    if (PIDs_file = fopen("PID_info.txt", "w")) {
        int i;
        for (i = 0; i < PROCESSES_COUNT; i++) fprintf(PIDs_file, "%d ", pids[i]);
        fclose(PIDs_file);
    }
    else {
        fprintf(stderr, "Plik przechowujacy PIDy nie zostal utworzony!\n");
    }
}

void getPids() { 
    if (PIDs_file = fopen("PID_info.txt", "r")) {
        int i;
        for (i = 0; i < PROCESSES_COUNT; i++) fscanf(PIDs_file, "%d", &pids[i]);
        fclose(PIDs_file);
    }
    else {
        fprintf(stderr, "Uwaga! Nie znaleziono pliku PID_info.txt");
    }
}

void removePidsFile() {
    system("rm PID_info.txt");
}

int mode;
int closing=0;
int self;   
int halted = 0; 

char charactersCount[INPUT_SIZE];
char data[INPUT_SIZE];  
int sum;               

char signalRecieved[NOTIFICATION_SIZE];
char signalToSend[NOTIFICATION_SIZE];

void cleanUp() {
    removePidsFile();
    removeSemaphores();
    removeSharedMemory();
    removeQueue();
    kill(getpid(), SIGKILL); 
}

void mainHandler(int signal) { 
    if (signal == SIGNAL_END || closing ==1) {
        fprintf(stderr, "\nTu proces macierzysty: %d, Koncze dzialanie\n", getpid());

        int processIndex;
        for (processIndex = 0; processIndex < PROCESSES_COUNT;processIndex++) {
            kill(getSavedPid(processIndex), SIGKILL);
        }
        cleanUp(); 
    }
}
void childHandler(int signal) {
  int a,b;
    a=getpid();
    b=getppid();

     if (signal == SIGNAL_END && pids[PROCESSES_COUNT-1] == a) {
        closing=1;        
        kill(getppid(), SIGTERM);
    
        return;

    }
    else if (((signal == SIGNAL_RESUME && halted) || (signal == SIGNAL_STOP && !halted))&& pids[PROCESSES_COUNT-1] == a) {
        semlock(SHM_SEM_LOCK); 
        strcpy(signalToSend, signal == SIGNAL_STOP ? NOTIFICATION_STOP : NOTIFICATION_RESUME);
     writeToSharedMemory(signalToSend); 

     //   printf("Zapisuje sygnal w shared memory jestem w PID: %d\n", getpid());
       //     sleep(2);
       kill(getSavedPid(next()), SIGNAL_RESUME);      
       kill(getSavedPid(previous()), SIGNAL_RESUME); 

        halted = signal == SIGNAL_STOP;  
        if (signal == SIGNAL_STOP) fprintf(stderr, "Wstrzymuje prace! PPID:%d\n", getpid());
        if (signal == SIGNAL_RESUME) fprintf(stderr, "Wznawiam prace! PPID:%d\n", getpid());

        semunlock(SHM_SEM_LOCK);  

    }
    else if (signal == SIGNAL_RESUME) {
        semlock(SHM_SEM_LOCK);

      readFromSharedMemory(signalRecieved);

    // printf("Czytam sygnal z shared memory jestem w PID: %d\n", getpid());
        //sleep(2);
        halted = !strcmp(signalRecieved, NOTIFICATION_STOP);  

        semunlock(SHM_SEM_LOCK);
    }

}


int mode = -1;              // tryb odczytu danych
char sourceFileHandle[64];  // nazwa pliku w trybie odczytu z pliku
FILE* sourceFile;           // wskaźnik na plik do odczytu danych
long readOffset = 0;
int kropka = 1;

int jakiTryb() {
    return mode;
}



void wybierzTryb() {  // Wybór trybu pracy
    fprintf(stderr, "1. Terminal\n2. Odczyt z pliku\n");

    mode = -1;

    while ((mode != MODE_INTERACTIVE) && (mode != MODE_FILE)) {
        fprintf(stderr, "Wybierz tryb pracy\n");
        mode = getchar() - 48 - 1;  // konwersja znaku do liczby
        getchar();                  // wyczyszczenie entera
    }

    if (mode == MODE_FILE) {
        fprintf(stderr, "\nPodaj sciezke pliku z rozszerzeniem\n");
        scanf("%s", sourceFileHandle);  // wczytanie nazwy pliku
        getchar();                      // wyczyszczenie entera
    }
}

void readStdin(char* data) {
    fprintf(stderr, "\nPodaj dane:\n");
    fgets(data, INPUT_SIZE + 1, stdin);                         // wczytanie strumienia wejścia do bufora
    if (strstr(data, ".") != NULL) {
        kropka = 1;
    }

    if (strlen(data) == 1) fgets(data, INPUT_SIZE + 1, stdin);  // strlen(data)==1 oznacza, że bufor został w całości zapełniony
    data[strcspn(data, "\n")] = '\0';                           // usunięcie entera (ukradzione ze stackoverflow)
}

void readSourceFile(char* data) {
    if (sourceFile = fopen(sourceFileHandle, "r")) {
        fseek(sourceFile, readOffset, SEEK_SET);

        if (fgets(data, INPUT_SIZE + 1, sourceFile) == NULL) {
            fprintf(stderr, "\n[P1:%d] Koniec pliku!\n", getpid());
            readOffset = -1;
            wybierzTryb();
        }
        readOffset += strlen(data) == 0 ? 1 : strlen(data);
        fclose(sourceFile);
    } else {
        fprintf(stderr, "Nie udało sie otworzyc pliku\n");
        kill(getSavedPid(1), SIGNAL_END);  // sygnał zakończenia pracy
    }
       data[strcspn(data, "\r\n")] = '\0'; 
}

void (*trybOdczytu[2])(char*) = {readStdin, readSourceFile,};  // wskaźniki funkcji odczytu wejscia


void reading() {                                //wypisywanie
    while (1) {
       semlock(self);
        if (kropka){
            wybierzTryb();
            kropka = 0;
        }
        trybOdczytu[jakiTryb()](data); 
    
        while (halted) pause();                 //wstrzymanie
        if (DEBUG) {fprintf(stderr, "[P1:%d] Dane: %s\n\n", getpid(), data);}
         printf("Proces 1 [Pobieram znaki]PPID:%d \nOdczytalem: %s\n", getpid(), data); 
        writeToQueue1(data);

      semunlock(next());
    }
}

void processing() {                             //przetwarzanie danych wejsciowych
  while (1) {
        semlock(self);                          //blokowanie dostepu do pamieci dzielonej
        while (halted) pause();
        readFromQueue1(data);             //odczyt z pamieci dzielonej
        sprintf(charactersCount, "%d", (int)strlen(data));  //wypisanie ilosci znakow
        fprintf(stderr, "Proces 2 [Zliczam znaki]PPID:%d \nOdczytalem: %s\n\n", getpid(), charactersCount);   //wypisanie ilosci znakow do pliku
        writeToQueue2(charactersCount);   //zapis do pamieci dzielonej

        semunlock(next());                      //odblokowanie dostepu do pamieci dzielonej
    }
}

void logging() {
    while (1) {
        semlock(self);                          //blokowanie dostepu do pamieci dzielonej
        while (halted) pause();
        
        readFromQueue2(charactersCount);  //zapis ilosci znakow do pamieci dzielonej

        if (DEBUG) {
            fprintf(stderr, "[P3:%d] Dane: %s\n", getpid(), charactersCount);   //zapis ilosci znakow do pliku
        }

        printf("Proces 3 [Wyswietlam zliczanie]PPID:%d \nOdczytalem: %s\n\n",getpid(), charactersCount);       //wypisanie ilosci znakow
        fflush(stdout);                         //oproznienie bufora stdout - umozliwia natychmiastowe wypisanie na ekranie
        semunlock(next());                      //odblokowanie dostepu do pamieci dzielonej
    }
}


void createProcesses() {
    void (*processes[PROCESSES_COUNT])(void) = { reading, processing, logging };  
    int processIndex;
    for (processIndex = 0; processIndex < PROCESSES_COUNT; processIndex++) {
        
        savePid(processIndex, fork());         
        if (getSavedPid(processIndex) == 0) {  
            
            self = processIndex;

            
            fprintf(stderr, "Proces %d PID:%d PPID:%d\n", self + 1, getpid(), getppid());

            
            int i;
            for (i = 1; i <= SIGNALS_COUNT; i++) signal(i, childHandler);

            
            semlock(self);      
            getPids();         
            semunlock(next());  

            processes[self](); 
        }
    }
}

int main() {
    fprintf(stderr, "Proces macierzysty PID:%d PPID:%d\n", getpid(), getppid());
    /*printf("Czy chcesz czytac z\n0.Klawiatury\n1.Pliku\n");
    int wybor;    
    scanf("%d", &wybor);
    mode = (wybor==0 ? MODE_INTERACTIVE : MODE_FILE);  
    printf("%d\n",mode);*/
   
    createQueue(); 
    createSemaphores();
    createSharedMemory(INPUT_SIZE);
    semunlock(SHM_SEM_LOCK);
    int i;
    for (i = 1; i <= SIGNALS_COUNT; i++) signal(i, mainHandler);
    createProcesses();
    exportPids();
    semunlock(0);

    while (1) pause();
    return 0;
}
