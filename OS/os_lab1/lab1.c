#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int ready = 0;
volatile sig_atomic_t running = 1;

void handle_sigint(int sig) {
    running = 0;
}

void* provider(void* arg) {
    while (running) {
        sleep(1);
        
        pthread_mutex_lock(&lock);
        
        if (ready == 1) {
            pthread_mutex_unlock(&lock);
            continue;
        }
        
        ready = 1;
        printf("Provider: Event provided\n");
        
        pthread_cond_signal(&cond);
        
        pthread_mutex_unlock(&lock);
    }
    
    pthread_mutex_lock(&lock);
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&lock);

    return NULL;
}

void* consumer(void* arg) {
    while (1) {
        pthread_mutex_lock(&lock);
        
        while (ready == 0 && running) {
            pthread_cond_wait(&cond, &lock);
        }
        
        if (!running) {
            pthread_mutex_unlock(&lock);
            break;
        }
        
        printf("Consumer: Event consumed\n");
        ready = 0;
        
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

int main() {
    signal(SIGINT, handle_sigint);
    
    pthread_t provider_thread, consumer_thread;
    
    pthread_create(&provider_thread, NULL, provider, NULL);
    pthread_create(&consumer_thread, NULL, consumer, NULL);
    
    pthread_join(provider_thread, NULL);
    pthread_join(consumer_thread, NULL);
    
    pthread_mutex_destroy(&lock);
    pthread_cond_destroy(&cond);
}
