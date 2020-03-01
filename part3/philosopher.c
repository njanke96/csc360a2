#include <stdio.h> 
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <string.h>
#include <unistd.h>

#include "philosopher.h"

// a fork on the heap
struct fork {
    int in_use; // 1 true 0 false
};

// a type representing a philosopher
typedef struct philosopher {
    // fork to this philosopher's left
    struct fork *left;

    // fork to this philosopher's right
    struct fork *right;

    // this philosopher's name
    char name[64];

    // how many times eaten
    int eat_count;
} philosopher_t;

// our semaphore
sem_t semaphore;

/* Philosopher thread */
void* philo_thread(void *philo) {
    philosopher_t *phil = (philosopher_t *) philo;

    printf("%s: I'm thinking.\n", phil->name);
    sleep_random();
    
    while (phil->eat_count < 5) {
        sem_wait(&semaphore);
        // critical section

        // try to pick up both forks
        if (phil->left->in_use == 0 && phil->right->in_use == 0) {
            // pick them up and eat
            printf("%s: I'm picking up forks.\n", phil->name);
            phil->left->in_use = 1;
            phil->right->in_use = 1;

            printf("%s: Nom nom nom.\n", phil->name);
            phil->eat_count++;

            sem_post(&semaphore);
            sleep_random();
            sem_wait(&semaphore);

            // put the forks down
            printf("%s: I'm putting my forks down.\n", phil->name);
            phil->left->in_use = 0;
            phil->right->in_use = 0;

        } else {
            printf("%s: I don't have two forks, I'm going to think some more.\n", phil->name);
        }

        sem_post(&semaphore);
        sleep_random();
    }

    printf("%s: I'm full.\n", phil->name);

    return NULL;
}

int main() {
    sem_init(&semaphore, 0, 1);

    // Forks 1 - 5 (around the table clockwise)
    struct fork *fork1 = (struct fork*) malloc(sizeof(struct fork));
    fork1->in_use = 0;

    struct fork *fork2 = (struct fork*) malloc(sizeof(struct fork));
    fork2->in_use = 0;

    struct fork *fork3 = (struct fork*) malloc(sizeof(struct fork));
    fork3->in_use = 0;

    struct fork *fork4 = (struct fork*) malloc(sizeof(struct fork));
    fork4->in_use = 0;

    struct fork *fork5 = (struct fork*) malloc(sizeof(struct fork));
    fork5->in_use = 0;

    // Philosopher 1
    philosopher_t phil1;
    phil1.left = fork1;
    phil1.right = fork5;
    phil1.eat_count = 0;
    strcpy(phil1.name, "Philosopher 1");

    // Philosopher 2
    philosopher_t phil2;
    phil2.left = fork2;
    phil2.right = fork1;
    phil2.eat_count = 0;
    strcpy(phil2.name, "Philosopher 2");

    // Philosopher 3
    philosopher_t phil3;
    phil3.left = fork3;
    phil3.right = fork2;
    phil3.eat_count = 0;
    strcpy(phil3.name, "Philosopher 3");

    // Philosopher 4
    philosopher_t phil4;
    phil4.left = fork4;
    phil4.right = fork3;
    phil4.eat_count = 0;
    strcpy(phil4.name, "Philosopher 4");

    // Philosopher 5
    philosopher_t phil5;
    phil5.left = fork5;
    phil5.right = fork4;
    phil5.eat_count = 0;
    strcpy(phil5.name, "Philosopher 5");

    // start the threads
    pthread_t t1, t2, t3, t4, t5;
    pthread_create(&t1, NULL, philo_thread, (void *) &phil1);
    pthread_create(&t2, NULL, philo_thread, (void *) &phil2);
    pthread_create(&t3, NULL, philo_thread, (void *) &phil3);
    pthread_create(&t4, NULL, philo_thread, (void *) &phil4);
    pthread_create(&t5, NULL, philo_thread, (void *) &phil5);

    // wait for threads before continuing
    pthread_join(t1,NULL);
    pthread_join(t2,NULL);
    pthread_join(t3,NULL);
    pthread_join(t4,NULL);
    pthread_join(t5,NULL);

    // free memory
    free(fork1);
    free(fork2);
    free(fork3);
    free(fork4);
    free(fork5);

    sem_destroy(&semaphore);
    return 0;
}

/* Sleep for a random number of seconds up to 5 seconds */
void sleep_random() {
    int randint = (rand() % 5) + 1;
    sleep(randint);
}
