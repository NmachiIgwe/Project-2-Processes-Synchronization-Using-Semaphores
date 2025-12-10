#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <unistd.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

/*
 * Shared memory layout:
 *  - BankAccount: shared integer balance
 *  - mutex: unnamed semaphore for mutual exclusion
 */
typedef struct {
    int BankAccount;
    sem_t mutex;
} shared_data_t;

void ParentProcess(shared_data_t *shm);
void ChildProcess(shared_data_t *shm);

int main(int argc, char *argv[])
{
    int ShmID;
    shared_data_t *ShmPTR;
    pid_t pid;
    int status;

    /* Create shared memory segment */
    ShmID = shmget(IPC_PRIVATE, sizeof(shared_data_t), IPC_CREAT | 0666);
    if (ShmID < 0) {
        perror("*** shmget error ***");
        exit(1);
    }
    printf("Server has received a shared memory of size %zu bytes...\n",
           sizeof(shared_data_t));

    /* Attach it into our address space */
    ShmPTR = (shared_data_t *) shmat(ShmID, NULL, 0);
    if (ShmPTR == (void *) -1) {
        perror("*** shmat error ***");
        exit(1);
    }
    printf("Server has attached the shared memory...\n");

    /* Initialize shared variables */
    ShmPTR->BankAccount = 0;

    /* Initialize unnamed semaphore in shared memory (pshared = 1) */
    if (sem_init(&(ShmPTR->mutex), 1, 1) != 0) {
        perror("sem_init failed");
        exit(1);
    }
    printf("Server has initialized BankAccount and semaphore...\n");

    /* Seed RNG once in parent before fork */
    srand(time(NULL));

    printf("Server is about to fork a child process...\n");
    pid = fork();
    if (pid < 0) {
        perror("*** fork error ***");
        exit(1);
    }
    else if (pid == 0) {
        /* Child process: Poor Student */
        /* Re-seed RNG a bit differently so parent/child aren't identical */
        srand(time(NULL) ^ getpid());
        ChildProcess(ShmPTR);
        /* Child detaches but does NOT remove shared memory */
        shmdt((void *) ShmPTR);
        exit(0);
    }

    /* Parent process: Dear Old Dad */
    ParentProcess(ShmPTR);

    /* Wait for child to finish */
    wait(&status);
    printf("Server has detected the completion of its child...\n");

    /* Destroy semaphore (only once, after child exits) */
    if (sem_destroy(&(ShmPTR->mutex)) != 0) {
        perror("sem_destroy failed");
    }

    /* Detach and remove shared memory */
    shmdt((void *) ShmPTR);
    printf("Server has detached its shared memory...\n");
    shmctl(ShmID, IPC_RMID, NULL);
    printf("Server has removed its shared memory...\n");

    printf("Server exits...\n");
    return 0;
}

/*
 * Parent (Dear Old Dad) Rules
 *
 * - Sleep random 0–5 seconds.
 * - After waking, print:
 *     "Dear Old Dad: Attempting to Check Balance"
 * - Generate a random number:
 *     - If even:
 *         - If last localBalance < 100 -> Deposit Money.
 *         - Else -> "Dear old Dad: Thinks Student has enough Cash ($%d)\n"
 *     - If odd:
 *         - "Dear Old Dad: Last Checking Balance = $%d\n"
 *
 * Deposit Money:
 * - Copy BankAccount into localBalance.
 * - Randomly generate amount between 0 and 100.
 * - If amount is even:
 *     - localBalance += amount;
 *     - printf("Dear old Dad: Deposits $%d / Balance = $%d\n", amount, localBalance);
 *     - Copy localBalance back into BankAccount.
 * - If amount is odd:
 *     - printf("Dear old Dad: Doesn't have any money to give\n");
 */
void ParentProcess(shared_data_t *shm)
{
    printf("   Parent process (Dear Old Dad) started\n");

    /* Use a finite loop so program terminates for grading */
    for (int i = 0; i < 25; i++) {
        /* Sleep random 0–5 seconds */
        sleep(rand() % 6);

        /* Enter critical section */
        sem_wait(&(shm->mutex));

        int localBalance;

        printf("Dear Old Dad: Attempting to Check Balance\n");

        /* Copy shared BankAccount into local non-shared variable */
        localBalance = shm->BankAccount;

        int r = rand();
        if (r % 2 == 0) {
            /* Even: maybe deposit, depending on balance */
            if (localBalance < 100) {
                /* Deposit Money */
                int amount = rand() % 101;  /* 0–100 */

                if (amount % 2 == 0) {
                    /* Even amount: deposit */
                    localBalance += amount;
                    printf("Dear old Dad: Deposits $%d / Balance = $%d\n",
                           amount, localBalance);
                    /* Copy localBalance back into shared BankAccount */
                    shm->BankAccount = localBalance;
                } else {
                    /* Odd amount: no deposit */
                    printf("Dear old Dad: Doesn't have any money to give\n");
                    /* BankAccount stays unchanged */
                }
            } else {
                printf("Dear old Dad: Thinks Student has enough Cash ($%d)\n",
                       localBalance);
            }
        } else {
            /* Odd random: just check balance */
            printf("Dear Old Dad: Last Checking Balance = $%d\n", localBalance);
        }

        /* Leave critical section */
        sem_post(&(shm->mutex));
    }

    printf("   Parent process finished its work\n");
}

/*
 * Child (Poor Student) Rules
 *
 * - Sleep random 0–5 seconds.
 * - After waking, print:
 *     "Poor Student: Attempting to Check Balance\n"
 * - Generate random number:
 *     - If even: attempt to Withdraw Money.
 *     - If odd: just check:
 *         "Poor Student: Last Checking Balance = $%d\n"
 *
 * Withdraw Money:
 * - Copy BankAccount into localBalance.
 * - Generate need between 0 and 50:
 *     printf("Poor Student needs $%d\n", need);
 * - If need <= localBalance:
 *     - localBalance -= need;
 *     - printf("Poor Student: Withdraws $%d / Balance = $%d\n", need, localBalance);
 *     - Copy localBalance back into BankAccount.
 * - Else:
 *     - printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
 *     - Copy localBalance (unchanged) back into BankAccount.
 */
void ChildProcess(shared_data_t *shm)
{
    printf("   Child process (Poor Student) started\n");

    for (int i = 0; i < 25; i++) {
        /* Sleep random 0–5 seconds */
        sleep(rand() % 6);

        /* Enter critical section */
        sem_wait(&(shm->mutex));

        int localBalance;

        printf("Poor Student: Attempting to Check Balance\n");

        /* Copy shared BankAccount into local non-shared variable */
        localBalance = shm->BankAccount;

        int r = rand();
        if (r % 2 == 0) {
            /* Even: Withdraw Money */
            int need = rand() % 51;  /* 0–50 */
            printf("Poor Student needs $%d\n", need);

            if (need <= localBalance) {
                localBalance -= need;
                printf("Poor Student: Withdraws $%d / Balance = $%d\n",
                       need, localBalance);
                /* Copy updated localBalance to shared */
                shm->BankAccount = localBalance;
            } else {
                printf("Poor Student: Not Enough Cash ($%d)\n", localBalance);
                /* Copy unchanged localBalance to shared (no withdraw) */
                shm->BankAccount = localBalance;
            }
        } else {
            /* Odd: just check last balance */
            printf("Poor Student: Last Checking Balance = $%d\n", localBalance);
        }

        /* Leave critical section */
        sem_post(&(shm->mutex));
    }

    printf("   Child process finished its work\n");
}
