#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 20

#define COMPRESS 1
#define DECOMPRESS 0

typedef struct{
    queue in;
    queue out;
    chunk (*process)(chunk);
    sem_t * sem_remaining_chunks;
    sem_t * q_in_available_chunks;
    sem_t * q_out_available_chunks;
    sem_t * q_in_free_spaces;
    sem_t * q_out_free_spaces;
}workerargs;

// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void *worker(void * arg) {
    chunk ch, res;
    workerargs *args = arg;

    while(sem_trywait(args->sem_remaining_chunks) == 0){ 
        sem_wait(args->q_in_available_chunks);    
        ch = q_remove(args->in);                  
        sem_post(args->q_in_free_spaces);         

        res = (args->process)(ch);                
        free_chunk(ch);                          

        sem_wait(args->q_out_free_spaces);       
        q_insert(args->out, res);                 
        sem_post(args->q_out_available_chunks);
    }
    return NULL;
}
    


// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
    int fd, chunks,i,offset;
    char comp_file[256];
    struct stat st;
    archive ar;
    queue in, out;
    chunk ch;

    pthread_t * workerthreads = malloc(sizeof(pthread_t) * opt.num_threads); //los threads para worker

    //semaforos para In y Out
    sem_t in_sem;
    sem_t out_sem;
    sem_t in_free_spaces;
    sem_t out_free_spaces;

    //inicializacion de los semaforos
    sem_init(&in_sem,0,0);
    sem_init(&out_sem,0,0);
    sem_init(&in_free_spaces,0,opt.queue_size);
    sem_init(&out_free_spaces,0,opt.queue_size);

    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }

    fstat(fd, &st);
    chunks = st.st_size/opt.size+(st.st_size % opt.size ? 1:0);

    if(opt.out_file) {
        strncpy(comp_file,opt.out_file,255);
    } else {
        strncpy(comp_file, opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }

    ar = create_archive_file(comp_file);

    in  = q_create(opt.queue_size);
    out = q_create(opt.queue_size);
    
    //READER
    for(i=0; i<chunks; i++) {
        ch = alloc_chunk(opt.size);

        offset=lseek(fd, 0, SEEK_CUR);

        ch->size   = read(fd, ch->data, opt.size);
        ch->num    = i;
        ch->offset = offset;

        q_insert(in, ch);
        sem_post(&in_sem);
    }

    //WORKERS
    sem_t sem_remaining_chunks;
    sem_init(&sem_remaining_chunks,0,chunks);

    workerargs wargs;
    wargs.in = in;
    wargs.out = out;
    wargs.process = zcompress;
    wargs.sem_remaining_chunks = &sem_remaining_chunks;
    wargs.q_in_available_chunks = &in_sem;
    wargs.q_out_available_chunks = &out_sem;
    wargs.q_in_free_spaces = &in_free_spaces;
    wargs.q_out_free_spaces = &out_free_spaces;


    //WRITER
    for(int i =0; i < opt.num_threads; i++){
        pthread_create(&workerthreads[i],NULL,worker, &wargs);
    }

    for(i=0; i<chunks; i++) {
        ch = q_remove(out);

        add_chunk(ar, ch);
        free_chunk(ch);
        sem_post(&out_free_spaces);
    }

    for (int i = 0; i < opt.num_threads;i++) { 
     pthread_join(workerthreads[i],NULL); 
    }

    close_archive_file(ar);
    close(fd);

    q_destroy(in);
    q_destroy(out);

    sem_destroy(&in_sem);
    sem_destroy(&out_sem);
    sem_destroy(&in_free_spaces);
    sem_destroy(&out_free_spaces);
    sem_destroy(&sem_remaining_chunks);

    free(workerthreads);
}


// Decompress file taking chunks of size opt.size from the input file

void decomp(struct options opt) {
    int fd, i;
    char uncomp_file[256];
    archive ar;
    chunk ch, res;

    if((ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    }

    for(i=0; i<chunks(ar); i++) {
        ch = get_chunk(ar, i);

        res = zdecompress(ch);
        free_chunk(ch);

        lseek(fd, res->offset, SEEK_SET);
        write(fd, res->data, res->size);
        free_chunk(res);
    }

    close_archive_file(ar);
    close(fd);
}

int main(int argc, char *argv[]) {
    struct options opt;

    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}
