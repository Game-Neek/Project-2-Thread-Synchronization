#include "BENSCHILLIBOWL.h"

#include <assert.h>
#include <stdlib.h>
#include <time.h>

bool IsEmpty(BENSCHILLIBOWL* bcb);
bool IsFull(BENSCHILLIBOWL* bcb);
void AddOrderToBack(Order **orders, Order *order);

MenuItem BENSCHILLIBOWLMenu[] = { 
    "BensChilli", 
    "BensHalfSmoke", 
    "BensHotDog", 
    "BensChilliCheeseFries", 
    "BensShake",
    "BensHotCakes",
    "BensCake",
    "BensHamburger",
    "BensVeggieBurger",
    "BensOnionRings",
};
int BENSCHILLIBOWLMenuLength = 10;

/* Select a random item from the Menu and return it */
MenuItem PickRandomMenuItem() {
    int index = rand() % BENSCHILLIBOWLMenuLength;
    return BENSCHILLIBOWLMenu[index];
}

/* Allocate memory for the Restaurant, then create the mutex and condition variables needed to instantiate the Restaurant */

BENSCHILLIBOWL* OpenRestaurant(int max_size, int expected_num_orders) {
    // Seed random once (okay here for the whole program)
    srand(time(NULL));

    BENSCHILLIBOWL *bcb = (BENSCHILLIBOWL *)malloc(sizeof(BENSCHILLIBOWL));
    if (!bcb) {
        perror("Failed to allocate BENSCHILLIBOWL");
        exit(1);
    }

    bcb->orders = NULL;
    bcb->current_size = 0;
    bcb->max_size = max_size;
    bcb->next_order_number = 1;
    bcb->orders_handled = 0;
    bcb->expected_num_orders = expected_num_orders;

    pthread_mutex_init(&bcb->mutex, NULL);
    pthread_cond_init(&bcb->can_add_orders, NULL);
    pthread_cond_init(&bcb->can_get_orders, NULL);

    printf("Restaurant is open!\n");
    return bcb;
}

/* check that the number of orders received is equal to the number handled (ie.fullfilled). Remember to deallocate your resources */

void CloseRestaurant(BENSCHILLIBOWL* bcb) {
    // At this point, all threads should be joined
    pthread_mutex_lock(&bcb->mutex);
    // Make sure all orders were handled
    assert(bcb->orders_handled == bcb->expected_num_orders);
    assert(IsEmpty(bcb));
    pthread_mutex_unlock(&bcb->mutex);

    // Destroy sync primitives
    pthread_mutex_destroy(&bcb->mutex);
    pthread_cond_destroy(&bcb->can_add_orders);
    pthread_cond_destroy(&bcb->can_get_orders);

    // Free any leftover orders (should not be any, but be safe)
    Order *curr = bcb->orders;
    while (curr != NULL) {
        Order *tmp = curr;
        curr = curr->next;
        free(tmp);
    }

    free(bcb);

    printf("Restaurant is closed!\n");
}

/* add an order to the back of queue */
int AddOrder(BENSCHILLIBOWL* bcb, Order* order) {
    pthread_mutex_lock(&bcb->mutex);

    // Wait while restaurant is full
    while (IsFull(bcb)) {
        pthread_cond_wait(&bcb->can_add_orders, &bcb->mutex);
    }

    // Assign order number and enqueue
    order->order_number = bcb->next_order_number++;
    AddOrderToBack(&bcb->orders, order);
    bcb->current_size++;

    // Signal that an order is available
    pthread_cond_signal(&bcb->can_get_orders);

    int order_num = order->order_number;
    pthread_mutex_unlock(&bcb->mutex);

    return order_num;
}

/* remove an order from the queue */
Order *GetOrder(BENSCHILLIBOWL* bcb) {
    pthread_mutex_lock(&bcb->mutex);

    // Wait while empty AND still expect more orders
    while (IsEmpty(bcb) && bcb->orders_handled < bcb->expected_num_orders) {
        pthread_cond_wait(&bcb->can_get_orders, &bcb->mutex);
    }

    // If no more orders are expected and queue is empty, we are done
    if (IsEmpty(bcb) && bcb->orders_handled >= bcb->expected_num_orders) {
        // Wake up any other waiting cooks so they can also exit
        pthread_cond_broadcast(&bcb->can_get_orders);
        pthread_mutex_unlock(&bcb->mutex);
        return NULL;
    }

    // Remove from front of queue
    Order *order = bcb->orders;
    bcb->orders = order->next;
    order->next = NULL;

    bcb->current_size--;
    bcb->orders_handled++;

    // We just made space: signal customers that they can add if needed
    pthread_cond_signal(&bcb->can_add_orders);

    pthread_mutex_unlock(&bcb->mutex);
    return order;
}

// Optional helper functions (you can implement if you think they would be useful)
bool IsEmpty(BENSCHILLIBOWL* bcb) {
    return (bcb->current_size == 0);
}

bool IsFull(BENSCHILLIBOWL* bcb) {
    return (bcb->current_size >= bcb->max_size);
}

/* this method adds order to rear of queue */
void AddOrderToBack(Order **orders, Order *order) {
    order->next = NULL;
    if (*orders == NULL) {
        *orders = order;
    } else {
        Order *curr = *orders;
        while (curr->next != NULL) {
            curr = curr->next;
        }
        curr->next = order;
    }
}
