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

typedef struct{                                   //struct para worker
    queue in;
    queue out;
    chunk (*process)(chunk);
    sem_t * sem_remaining_chunks;           
    sem_t * q_in_available_chunks;
    sem_t * q_out_available_chunks;               //semáforos para disponibilidad de chunks
    sem_t * q_in_free_spaces;                     //semáforos para controlar espacio libre 
    sem_t * q_out_free_spaces;
}workerargs;

typedef struct{                                   //struct para reader
    queue in;
    sem_t *q_in_available_chunks;
    sem_t *q_in_free_spaces;
    struct options opt;
    int fd, chunks;
}readerargs;

typedef struct{                                   //struct para writer
    int fd, chunks;
    sem_t *q_out_available_chunks;
    sem_t *q_out_free_spaces;
    queue out;
    archive ar;
}writerargs;

// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void *worker(void * arg) {
    chunk ch, res;
    workerargs *args = arg;

    while(sem_trywait(args->sem_remaining_chunks) == 0){ //intenta bloquear el semáforo, devuelve 0 si lo consigue
      
        sem_wait(args->q_in_available_chunks);    //espera que haya espacio disponible en cola de entrada
        ch = q_remove(args->in);                  //coge fragmentos cola de entrada
        sem_post(args->q_in_free_spaces);         //libera espacio en cola de entrada

        res = (args->process)(ch);                //comprimir/descomprimir
        free_chunk(ch);                           //libera memoria del fragmento que quitamos de la cola de entrada

        sem_wait(args->q_out_free_spaces);        //espera a que haya espacio disponible en cola de salida
        q_insert(args->out, res);                 //inserta resultado a cola de salida
        sem_post(args->q_out_available_chunks);
    }
    

    return NULL;
}

void * reader(void *arg){                         //lee datos del archivo y los coloca en la cola
    int offset = 0;                               //posición actual del archivo
    readerargs * args = arg;                      //args será un struct de tipo readerargs
    chunk ch;                                   
                                     
    
    for(int i = 0;i<args->chunks;i++){
         ch = alloc_chunk(args->opt.size);         //asignamos memoria para el fragmento

        offset=lseek(args->fd, 0, SEEK_CUR);      //obtener pos del archivo de entrada para saber dónde empieza el fragmento

        ch->size   = read(args->fd, ch->data, args->opt.size);          //lee el contenido, lo almacena en size y guarda bytes leidos en size
        ch->num    = i;                           //número de fragmento 
        ch->offset = offset;                      

        sem_wait(args->q_in_free_spaces);         //espera que haya espacio en cola de entrada
        q_insert(args->in, ch);                   //inserta el fragmento en cola de entrada
        sem_post(args->q_in_available_chunks);    //indica que hay nuevo fragmento disponible
    }

  return NULL;

}

void * writer(void *arg){
  writerargs * args = arg;                        //declara un puntero de tipo writerargs
  chunk ch;                                       //para almacenar los datos
  int i = 0;

  for (i = 0; i < args->chunks; i++){            
    sem_wait(args->q_out_available_chunks);       //espera a que haya chunks disponibles
    ch = q_remove(args->out);                     //extrae un chunk de la cola de salida
    sem_post(args->q_out_free_spaces);            //indica que hay un espacio libre en cola de salida

    add_chunk(args->ar, ch);                    
    free_chunk(ch);
  }

  return NULL;
}


// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
    int fd, chunks;
    char comp_file[256];
    struct stat st;
    archive ar;
    queue in, out;

    pthread_t * workerthreads = malloc(sizeof(pthread_t) * opt.num_threads); //los threads para worker
    pthread_t thread_reader;                                                 //thread para reader
    pthread_t thread_writer;                                                 //thread para writer

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
    

    //READER inicializacion struct y creacion de thread
    readerargs rargs;
    rargs.in = in;
    rargs.fd = fd;
    rargs.chunks = chunks;
    rargs.q_in_available_chunks = &in_sem;
    rargs.q_in_free_spaces = &in_free_spaces;
    rargs.opt = opt;

    pthread_create(&thread_reader,NULL,reader,&rargs);

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

    for(int i =0; i < opt.num_threads; i++){
        pthread_create(&workerthreads[i],NULL,worker, &wargs);
    }

    //WRITER
    writerargs wrargs;
    wrargs.chunks = chunks;
    wrargs.out = out;
    wrargs.q_out_available_chunks = &out_sem;
    wrargs.q_out_free_spaces = &out_free_spaces;
    wrargs.ar = ar;

    pthread_create(&thread_writer,NULL,writer,&wrargs);

    pthread_join(thread_reader,NULL);

    for (int i = 0; i < opt.num_threads;i++) { 
       pthread_join(workerthreads[i],NULL); 
    }

    pthread_join(thread_writer,NULL);

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
