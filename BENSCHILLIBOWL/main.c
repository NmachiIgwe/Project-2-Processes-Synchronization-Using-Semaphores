#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>

#include "BENSCHILLIBOWL.h"

#define BENSCHILLIBOWL_SIZE 100
#define NUM_CUSTOMERS 90
#define NUM_COOKS 10
#define ORDERS_PER_CUSTOMER 3
#define EXPECTED_NUM_ORDERS NUM_CUSTOMERS * ORDERS_PER_CUSTOMER

// global restaurant pointer
BENSCHILLIBOWL *bcb;

/* Customer thread */
void* BENSCHILLIBOWLCustomer(void* tid) {
    int customer_id = (int)(long) tid;

    for (int i = 0; i < ORDERS_PER_CUSTOMER; i++) {
        Order* order = malloc(sizeof(Order));
        order->menu_item = PickRandomMenuItem();
        order->customer_id = customer_id;
        order->next = NULL;

        int order_number = AddOrder(bcb, order);
        printf("Customer #%d placed order #%d: %s\n",
               customer_id, order_number, order->menu_item);
    }
    return NULL;
}

/* Cook thread */
void* BENSCHILLIBOWLCook(void* tid) {
    int cook_id = (int)(long) tid;
    int fulfilled = 0;

    while (1) {
        Order* order = GetOrder(bcb);
        if (order == NULL)
            break;

        printf("Cook #%d fulfilling order #%d (%s) for customer %d\n",
               cook_id, order->order_number, order->menu_item, order->customer_id);

        usleep(10000); // simulate cooking
        free(order);
        fulfilled++;
    }

    printf("Cook #%d fulfilled %d orders\n", cook_id, fulfilled);
    return NULL;
}

/* main */
int main() {
    srand(time(NULL));

    bcb = OpenRestaurant(BENSCHILLIBOWL_SIZE, EXPECTED_NUM_ORDERS);

    pthread_t customers[NUM_CUSTOMERS];
    pthread_t cooks[NUM_COOKS];

    for (int i = 0; i < NUM_CUSTOMERS; i++) {
        pthread_create(&customers[i], NULL, BENSCHILLIBOWLCustomer, (void*)(long)i);
    }

    for (int i = 0; i < NUM_COOKS; i++) {
        pthread_create(&cooks[i], NULL, BENSCHILLIBOWLCook, (void*)(long)i);
    }

    for (int i = 0; i < NUM_CUSTOMERS; i++)
        pthread_join(customers[i], NULL);

    for (int i = 0; i < NUM_COOKS; i++)
        pthread_join(cooks[i], NULL);

    CloseRestaurant(bcb);
    return 0;
}
