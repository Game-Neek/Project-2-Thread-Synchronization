#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>

// Global semaphore ID
int sem_id;

// union semun is needed for semctl on many systems 
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

//P() / wait operation 
void sem_wait_op(int sem_id) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op  = -1;   // decrement
    op.sem_flg = 0;
    semop(sem_id, &op, 1);
}

// V() / signal operation 
void sem_signal_op(int sem_id) {
    struct sembuf op;
    op.sem_num = 0;
    op.sem_op  = 1;    // increment
    op.sem_flg = 0;
    semop(sem_id, &op, 1);
}

void ParentProcess(int *ShmPTR);  // Dear Old Dad
void ChildProcess(int *ShmPTR);   // Poor Student

int main(int argc, char *argv[])
{
    int    ShmID;
    int   *ShmPTR;   // ShmPTR[0] = BankAccount
    pid_t  pid;

    //Create shared memory for ONE int: BankAccount 
    ShmID = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    if (ShmID < 0) {
        perror("shmget failed");
        exit(1);
    }
    printf("Server: created shared memory for BankAccount.\n");

    ShmPTR = (int *)shmat(ShmID, NULL, 0);
    if ((long)ShmPTR == -1) {
        perror("shmat failed");
        exit(1);
    }
    printf("Server: attached shared memory.\n");

    // Initialize BankAccount
    ShmPTR[0] = 0;
    printf("Initial BankAccount = %d\n", ShmPTR[0]);

    // Create a semaphore (1 mutex) 
    sem_id = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    if (sem_id < 0) {
        perror("semget failed");
        exit(1);
    }

    union semun arg;
    arg.val = 1; // binary semaphore, unlocked
    if (semctl(sem_id, 0, SETVAL, arg) < 0) {
        perror("semctl SETVAL failed");
        exit(1);
    }

    printf("Server: semaphore created and initialized.\n");
    printf("Server is about to fork a child process...\n");

    pid = fork();
    if (pid < 0) {
        perror("fork failed");
        exit(1);
    }
    else if (pid == 0) {
        // child process: Poor Student
        ChildProcess(ShmPTR);
        exit(0);  // we should never actually get here, loop is infinite
    }

    // parent process: Dear Old Dad
    ParentProcess(ShmPTR);

    // We never reach here in normal use because loops are infinite.
    // If you add a signal handler later, this is where you'd clean up.
    return 0;
}

//Parent (Dear Old Dad)
void ParentProcess(int *SharedMem)
{
    int localBalance = 0;
    int r, amount;

    srand(getpid());

    while (1) {
        sleep(rand() % 6);  // 0–5 seconds

        printf("Dear Old Dad: Attempting to Check Balance\n");

        r = rand();

        if (r % 2 == 0) {
            // even: maybe deposit
            if (localBalance < 100) {
                // Deposit Money:
                sem_wait_op(sem_id);

                // Copy from shared BankAccount into localBalance
                localBalance = SharedMem[0];

                amount = rand() % 101;   // 0–100

                if (amount % 2 == 0) {
                    localBalance += amount;
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                           amount, localBalance);

                    // copy back to shared BankAccount
                    SharedMem[0] = localBalance;
                } else {
                    printf("Dear old Dad: Doesn't have any money to give\n");
                    // BankAccount unchanged
                }

                sem_signal_op(sem_id);
            } else {
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                       localBalance);
            }
        } else {
            // odd: just check last known balance (no update)
            printf("Dear Old Dad: Last Checking Balance = $%d\n",
                   localBalance);
        }
    }
}

//Child (Poor Student)
void ChildProcess(int *SharedMem)
{
    int localBalance = 0;
    int r, need;

    srand(getpid());

    while (1) {
        sleep(rand() % 6);  // 0–5 seconds

        printf("Poor Student: Attempting to Check Balance\n");

        r = rand();

        if (r % 2 == 0) {
            // even: try to withdraw
            sem_wait_op(sem_id);

            // Copy from shared BankAccount into localBalance
            localBalance = SharedMem[0];

            need = rand() % 51;   // 0–50
            printf("Poor Student needs $%d\n", need);

            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student: Withdraws $%d / Balance = $%d\n",
                       need, localBalance);

                // copy back to shared
                SharedMem[0] = localBalance;
            } else {
                printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
                // BankAccount unchanged
            }

            sem_signal_op(sem_id);
        } else {
            // odd: just check last known balance
            printf("Poor Student: Last Checking Balance = $%d\n",
                   localBalance);
        }
    }
}
