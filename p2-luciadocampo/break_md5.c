#include <sys/types.h>
#include <openssl/md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <unistd.h>

//76a2173be6393254e72ffa4d6df1030a : passwd
//35bc8cec895861697a0243c1304c7789 : patata
//131521d14cf392762cda6b5ea9297e57 : ayyyyy
//72b302bf297a228a75730123efef7c41 : banana
//b373870b9139bbade83396a49b1afc9a : helloo
//063bab69dfbb1537755d634dcc0fe4e3 : vuelos
//3c9c03d6008a5adf42c2a55dd4a1a9f2 : javier

//./break_md5 72b302bf297a228a75730123efef7c41 b373870b9139bbade83396a49b1afc9a
//./break_md5 72b302bf297a228a75730123efef7c41 b373870b9139bbade83396a49b1afc9a 063bab69dfbb1537755d634dcc0fe4e3
//./break_md5 b373870b9139bbade83396a49b1afc9a 35bc8cec895861697a0243c1304c7789 131521d14cf392762cda6b5ea9297e57
//./break_md5 72b302bf297a228a75730123efef7c41 b373870b9139bbade83396a49b1afc9a 3c9c03d6008a5adf42c2a55dd4a1a9f2 063bab69dfbb1537755d634dcc0fe4e3

#define PASS_LEN 6
#define NUM_THREADS 4

int aux=0; //SE INICIALIZA FUERA POR QUE EN EL BUCLE A VECES SE REINICIALIZABA SOLO. CON EL AUX AQUÍ RESUELVE BIEN. VARIABLE GLOBAL COMPARTIDA.

struct args { //estructura de los argumentos
    pthread_t id;  //nos lo piden con un hilo
    long pos;
    long *cont; 
    pthread_mutex_t *mutex; //mutex que usamos compartido
    bool *final;
    char **clave; //array de las claves (el doble asterisco es de las claves)
    int *argc;  //cuenta los argumentos que se le pasan por parametro
};

long ipow(long base, int exp)
{
    long res = 1;
    for (;;)
    {
        if (exp & 1)
            res *= base;
        exp >>= 1;
        if (!exp)
            break;
        base *= base;
    }

    return res;
}

long pass_to_long(const char *str) {
    long res = 0;

    for(int i=0; i < PASS_LEN; i++)
        res = res * 26 + str[i]-'a';

    return res;
}

void long_to_pass(long n, unsigned char *str) {  // str should have size PASS_SIZE+1
    for(int i=PASS_LEN-1; i >= 0; i--) {
        str[i] = n % 26 + 'a';
        n /= 26;
    }
    str[PASS_LEN] = '\0';
}

int hex_value(char c) {
    if (c>='0' && c <='9')
        return c - '0';
    else if (c>= 'A' && c <='F')
        return c-'A'+10;
    else if (c>= 'a' && c <='f')
        return c-'a'+10;
    else return 0;
}

void hex_to_num(char *str, unsigned char *hex) {
    for(int i=0; i < MD5_DIGEST_LENGTH; i++)
        hex[i] = (hex_value(str[i*2]) << 4) + hex_value(str[i*2 + 1]);
}

void *break_pass(void *ptr) { //pasamos la funcion a void para pasarle el struct de argumentos
    struct args *threads = ptr;
    int aux2=(*threads->argc)-1;  //-1 PORQUE POR TERMINAL ./break_md5 cuenta como ARGUMENTO.

    unsigned char res[MD5_DIGEST_LENGTH];
    unsigned char hex[MD5_DIGEST_LENGTH*2+1];
    unsigned char *pass = malloc((PASS_LEN + 1) * sizeof(char));
    long bound = ipow(26, PASS_LEN); // we have passwords of PASS_LEN
    // lowercase chars =>
    //     26 ^ PASS_LEN  different cases

    for(long i=threads->pos; i <= bound; i+=(NUM_THREADS-1)) {
        if(*(threads->final)==true){ //si final es true el pass se cierra
            free(pass);
            return NULL;
        }
        long_to_pass(i, pass);
        MD5(pass, PASS_LEN, res);

        for (int j = 1; j < (*(threads->argc)) ; j++) { //recorremos desde la 1 porque 0 es breakmd5 hasta los argumentos que se le pasan por pantalla que van a ser las claves
            hex_to_num(threads->clave[j], hex);
            if (0 == memcmp(res, hex, MD5_DIGEST_LENGTH)) { //si la clave es igual a la que está probando el programa:
                aux = aux + 1;						//le sumamos un contador para la condicion de parada
                printf("\n%s: %s\n", threads->clave[j], pass); //se imprime la clave descifrada
                if (aux == aux2) {     //condicion de parada: cuando se acaben los argumentos
                    *(threads->final) = true; //pasa al if de arriba
                }
            }
        }
        pthread_mutex_lock(threads->mutex);
        *(threads->cont)+=1;  //para que la cuenta de contraseñas probadas por el programa esté controlada y lo haga de uno en uno
        pthread_mutex_unlock(threads->mutex);
    }
    free(pass);
    return NULL;
}

void *progress_bar(void *ptr){
    struct args *thread = ptr;
    long barra;
    long max = ipow(26,PASS_LEN); //creamos un long maximo con el ipow de lo que va a descifrar: PASS_LENGHT^26
    long aux1,aux2;  //ej2
    while (1){
        if(*thread->final){ //cuando final es true para
            return NULL;
        }
        pthread_mutex_lock(thread->mutex);
        barra = (*(thread->cont) * 100) / max; //el contador que cuenta las passwords que cuenta por sec, se multiplica por 100 y se divide el max para que vea el progreso del 0 al 100
        pthread_mutex_unlock(thread->mutex);
        aux1=*thread->cont; 
        usleep(1000); //para que haga la media que sino pilla siempre los mismos valores
        aux2=(*thread->cont-aux1) * 8; //calculo  de las pruebas por segundo(media)
        printf("\r (%ld%%) %ld/sec",barra,aux2);
        printf(" [");
        for(long i=0;i<barra;i++){
            printf("$");
        }
        printf("]");
        usleep(50);
        fflush(stdout); //vacia stdout
    }
}

//programa multithread (EJ3)
struct args *start_threads(char **argv,bool *final,long *cont,pthread_mutex_t *mutex,int *argc){ //inicializar los threads
    struct args *threads;

    printf("Creating %d threads\n",NUM_THREADS);
    threads = malloc(sizeof(struct args) * NUM_THREADS);

    if(threads == NULL){
        printf("Not enough memory\n");
        exit(1);
    }

    for (int i = 0; i < NUM_THREADS-1; ++i) { 
        threads[i].clave = argv; //clave es el argumento que se le pasa
        threads[i].pos = i;      //los threads con la posicion
        threads[i].final = final;
        threads[i].mutex = mutex;
        threads[i].cont = cont; //cuenta de las claves que va probando en tiempo real
        threads[i].argc = argc;

        if(0!= pthread_create(&threads[i].id,NULL,break_pass,&threads[i])){
            printf("Could not create thread %d",i);
            exit(1);
        }

    }
	//para que se usen todos los threads menos uno para descifrar la clave y usar el ultimo para la barra de progreso
    threads[NUM_THREADS-1].clave=argv;
    threads[NUM_THREADS-1].pos=0;
    threads[NUM_THREADS-1].final=final;
    threads[NUM_THREADS-1].mutex=mutex;
    threads[NUM_THREADS-1].cont=cont;
    threads[NUM_THREADS-1].argc=argc;

    if(0!=pthread_create(&threads[NUM_THREADS-1].id,NULL,progress_bar,&threads[NUM_THREADS-1])){
        printf("Could not create progress bar thread %d\n",NUM_THREADS-1);
        exit(1);
    }
    return threads;
}

void wait2(struct args *threads){
    for (int i = 0; i <= NUM_THREADS; i++) {
        pthread_join(threads[i].id,NULL);
    }
    free(threads);
}

int main(int argc, char *argv[]) {
    struct args *threads;
    bool final=false;
    long cont = 0;
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex,NULL);

    if(argc < 2) {
        printf("Use: %s string\n", argv[0]);
        exit(0);
    }

    threads= start_threads(argv, &final, &cont, &mutex, &argc);
    wait2(threads);
    return 0;
}
