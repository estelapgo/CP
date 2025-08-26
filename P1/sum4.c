#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"
#include <string.h>

struct nums {
	long *increase;
	long *decrease;
	long total;
	long diff;
    int cnt;
    pthread_mutex_t *mutex;
    pthread_mutex_t mutex_cnt;
};

struct args {
	int thread_num;		// application defined thread #
	long iterations;	// number of operations
    int size;
	struct nums *nums;	// pointer to the counters (shared with other threads)
};

struct thread_info {
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

// Threads run on this function
void *decrease_increase(void *ptr)
{
	struct args *args = ptr;
	struct nums *n = args->nums;
    long pos_in;
    long pos_dec;

	while(args->iterations--) {
        if (n->cnt == 0) break;
        else {
            pthread_mutex_lock(&n->mutex_cnt);
            n->cnt--;
            pthread_mutex_unlock(&n->mutex_cnt);
        }

        pos_in = rand() % args->size;
        do{
             pos_dec = rand() % args->size;
        }while(pos_in==pos_dec);
       
        if(&n->mutex[pos_in] < &n->mutex[pos_dec]){
            pthread_mutex_lock(&n->mutex[pos_in]);
            pthread_mutex_lock(&n->mutex[pos_dec]);
         
        } else {           
            pthread_mutex_lock(&n->mutex[pos_dec]);
            pthread_mutex_lock(&n->mutex[pos_in]);
            
        }
        
		n->decrease[pos_dec]--;
		n->increase[pos_in]++;

		long diff = n->total - (n->decrease[pos_dec] + n->increase[pos_in]);
		if (diff != n->diff) {
			n->diff = diff;
			 printf("Thread %d increasing pos %ld = %ld decreasing pos %ld = %ld diff %ld\n",
			       args->thread_num, pos_in, n->increase[pos_in], pos_dec, n->decrease[pos_dec], diff);
                   
		} else {
            printf("Thread %d increasing pos %ld = %ld decreasing pos %ld = %ld diff %ld\n",
			       args->thread_num, pos_in, n->increase[pos_in], pos_dec, n->decrease[pos_dec], diff);
        }
        
        pthread_mutex_unlock(&n->mutex[pos_dec]);
        pthread_mutex_unlock(&n->mutex[pos_in]);
        usleep(1);
        
    }
    return NULL;
}

void *move_increase(void *ptr){

    struct args *args = ptr;
	struct nums *n = args->nums;
    long pos_in;
    long pos_dec;

    while(args->iterations--){
        if (n->cnt == 0) break;
        else {
            pthread_mutex_lock(&n->mutex_cnt);
            n->cnt--;
            pthread_mutex_unlock(&n->mutex_cnt);
        }

        pos_in = rand() % args->size;

        do{
             pos_dec = rand() % args->size;
        }while(pos_in==pos_dec);
       
       
        if(&n->mutex[pos_in] < &n->mutex[pos_dec]){
            pthread_mutex_lock(&n->mutex[pos_in]);
            pthread_mutex_lock(&n->mutex[pos_dec]);
         
        } else {           
            pthread_mutex_lock(&n->mutex[pos_dec]);
            pthread_mutex_lock(&n->mutex[pos_in]);
            
        }

		n->increase[pos_dec]--;
		n->increase[pos_in]++;

        printf("Thread %d increasing pos %ld decreasing pos %ld in increments array\n", args->thread_num, pos_in, pos_dec);

        pthread_mutex_unlock(&n->mutex[pos_dec]);
        pthread_mutex_unlock(&n->mutex[pos_in]);
        usleep(1);
    }
    return NULL;

}

void print_array(struct nums nums, int size) {
    long total = 0;
    long suma = 0;

    for(int i = 0; i < size; i++) {
        suma = nums.increase[i] + nums.decrease[i];
        total += suma;
        printf("Increase[%d] = %ld", i, nums.increase[i]);
        printf("\tDecrease[%d] = %ld\n", i, nums.decrease[i]);
    }

    
    printf("Increase array: [  ");
    for(int i = 0; i < size; i++) {
        printf("%ld  ", nums.increase[i]);
      
    }
    printf("]\n");

    printf("Decrease array: [  ");
    for(int i = 0; i < size; i++) {
        printf("%ld  ", nums.decrease[i]);
      
    }
    printf("]\n");

    printf("Total suma: %ld\n\n", total);

}

void print_increase(struct nums nums, int size) {
    long total = 0;
    long suma = 0;

    printf("Increase array: [  ");

    for(int i = 0; i < size; i++) {
        suma = nums.increase[i] + nums.decrease[i];
        total += suma;
        printf("%ld  ", nums.increase[i]);
      
    }

    printf("]");
    printf("\nTotal suma: %ld\n", total);
}


// start opt.num_threads threads running on decrease_incresase
struct thread_info *start_threads(struct options opt, struct nums *nums)
{
    int i;
    struct thread_info *threads;
 
    printf("creating %d threads\n", opt.num_threads);
  
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    nums->cnt = opt.iterations;
  
    // Create num_thread threads running decrease_increase
    for (i = 0; i < opt.num_threads; i++) {
      
        threads[i].args = malloc(sizeof(struct args));

        threads[i].args->thread_num = i;
        threads[i].args->nums       = nums;
        threads[i].args->iterations = opt.iterations;
        threads[i].args->size = opt.size;

        if (0 != pthread_create(&threads[i].id, NULL, decrease_increase, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}

struct thread_info *start_threads2(struct options opt, struct nums *nums, struct args *args)
{
    int i;
    struct thread_info *threads;
 
    printf("creating %d threads\n", opt.num_threads);
  
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    nums->cnt = opt.iterations;
  
    // Create num_thread threads running decrease_increase
    for (i = 0; i < opt.num_threads; i++) {
      
        threads[i].args = &args[i];
        threads[i].args->thread_num = i;
        threads[i].args->nums       = nums;
        threads[i].args->iterations = opt.iterations;
        threads[i].args->size = opt.size;

        if (0 != pthread_create(&threads[i].id, NULL, move_increase, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}


// wait for all threads to finish, print totals, and free memory
void wait(struct options opt, struct nums *nums, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

    print_array(*nums, opt.size);

    for (int i = 0; i < opt.num_threads; i++)
        free(threads[i].args);

    free(threads);
}


void wait_increase(struct options opt, struct nums *nums, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].id, NULL);

   
    print_increase(*nums, opt.size);
    free(threads);
}


void initarrays(struct nums *nums, long size){

    nums->mutex = malloc(sizeof(pthread_mutex_t)*size);

    for(int i = 0; i < size; i++){
        nums->increase[i] = 0;
        nums->decrease[i] = nums->total;
        pthread_mutex_init(&nums->mutex[i],NULL);
    }
        
}

int main (int argc, char **argv)
{
    struct options opt;
    struct nums nums;
    struct thread_info *thrs;
    struct thread_info *thrs2;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 4;
    opt.iterations   = 100000;
    opt.size         = 10;

    read_options(argc, argv, &opt);

    struct args args[opt.num_threads];

    for(int i = 0; i < opt.num_threads; i++){
        args[i].thread_num = i;
        args[i].nums       = &nums;
        args[i].iterations = opt.iterations;
        args[i].size = opt.size; 
    } 

    nums.total = opt.iterations * opt.num_threads;
    nums.increase = malloc(sizeof(long)*opt.size);
    nums.decrease = malloc(sizeof(long)*opt.size);
    nums.diff = 0;
    nums.cnt = opt.iterations;

    memset(nums.increase,0,sizeof(long)*opt.size);
    memset(nums.decrease,0,sizeof(long)*opt.size);
    
    initarrays(&nums, opt.size);

    pthread_mutex_init(&nums.mutex_cnt,NULL);
    
    thrs = start_threads(opt, &nums);
    wait(opt, &nums, thrs);

    thrs2 = start_threads2(opt, &nums, args);
    wait_increase(opt, &nums, thrs2);

    pthread_mutex_destroy(nums.mutex);
    pthread_mutex_destroy(&nums.mutex_cnt);

    free(nums.mutex);
    free(nums.increase);
    free(nums.decrease);

    return 0;
}