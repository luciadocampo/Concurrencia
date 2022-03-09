//Lucia Docampo Rodriguez: lucia.docampo@udc.es
//David Twine Padin: david.twine.padin@udc.es

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include "options.h"

#define MAX_AMOUNT 20
#define FALSE 0  //son para que termine(en el main)
#define TRUE 1

struct bank {
    int num_accounts;        // number of accounts
    int *accounts;           // balance array
    pthread_mutex_t *mutex;  // added mutex to the struct
    int iter1;
    int iter2;			   //bank es el unico campo que comparten los threads 
    pthread_mutex_t count;  //lo inicializamos ahí porque es la unica zona compartida en comun con el bank, con el que podamos hacer la cuenta en total
};

struct args {
    int          thread_num;  // application defined thread #
    int          delay;       // delay between operations
    int	         iterations;  // number of operations
    int          net_total;   // total amount deposited by this thread
    struct bank *bank;        // pointer to the bank (shared with other threads)
};

struct thread_info { //hicimos dos más exclusivos
    pthread_t    id;    // id returned by pthread_create()
    struct args *args;  // pointer to the arguments
};

struct auxiliar_args{   // struct auxiliar para los argumentos del thread auxiliar.
    int delay;
    int final; 
    struct bank *bank;
};

struct auxiliar_thread_info{  // struct auxiliar para la info del thread auxiliar.
    pthread_t id;
    struct auxiliar_args *args;
};

// Threads run on this function
void *deposit(void *ptr)
{
    struct args *args =  ptr;
    int amount, account, balance;

    while(args->iterations--) { //ej4
        if(args->bank->iter1 == 0) break;
        else{
            pthread_mutex_lock(&args->bank->count);
            args->bank->iter1 --;
            pthread_mutex_unlock(&args->bank->count);
        }

        amount  = rand() % MAX_AMOUNT;
        account = rand() % args->bank->num_accounts;

        printf("Thread %d depositing %d on account %d\n",
               args->thread_num, amount, account);

        pthread_mutex_lock(&args->bank->mutex[account]); //para que no se mezclen los datos de las cuentas
        balance = args->bank->accounts[account];
        if(args->delay)
            usleep(args->delay); // Force a context switch

        balance += amount;
        if(args->delay) usleep(args->delay);

        args->bank->accounts[account] = balance;
        if(args->delay) usleep(args->delay);

        args->net_total += amount;
        pthread_mutex_unlock(&args->bank->mutex[account]); //

    }
    return NULL;
}

void *transfer(void *ptr){

    struct args *args = ptr;
    int amount, account1, account2, balance1, balance2;
	//while ej 4
    while(args->iterations--) {
        if(args->bank->iter2 == 0) break; //iter 2 es 0 para que salga el mismo numero 
        else{
            pthread_mutex_lock(&args->bank->count);
            args->bank->iter2 --;
            pthread_mutex_unlock(&args->bank->count);
        }
        do{
            account1= rand() % args->bank->num_accounts;
            account2 = rand() % args ->bank->num_accounts;
        }while(account1==account2); 

        if(account1 < account2){ //para evitar el interbloqueo
            pthread_mutex_lock(&args->bank->mutex[account1]);
            pthread_mutex_lock(&args->bank->mutex[account2]);
        } else{
            pthread_mutex_lock(&args->bank->mutex[account2]);
            pthread_mutex_lock(&args->bank->mutex[account1]);
        }

        if(args->bank->accounts[account1]==0){
            amount=0;
        } else{
            amount= rand() % args->bank->accounts[account1];
            if(amount==0){
                amount = rand() % args->bank->accounts[account1];
            }
        }
        printf("Thread %d transfering %d from account %d to account %d\n",
               args ->thread_num, amount,account1 , account2);

        balance1 = args->bank->accounts[account1]; //apartado 3
        balance2 = args->bank->accounts[account2];

        if(args->delay)
            usleep(args->delay);
        balance1-=amount;
        balance2+=amount;

        if(args->delay)
            usleep(args->delay);
        args->bank->accounts[account1] = balance1;
        args->bank->accounts[account2] = balance2;

        pthread_mutex_unlock(&args->bank->mutex[account1]);
        pthread_mutex_unlock(&args->bank->mutex[account2]);
    }
    return NULL;
}

void *aux_thread(void *ptr){

    struct auxiliar_args *args = ptr;
    int bank_total;
    int balance1;

    while (!args->final){ //para que empiece a la vez de las trasnferencias || mientras los args no sean finales va recorriendo las cuentas con el balance que tienen
        bank_total=0;
        for (int i = 0; i < args->bank->num_accounts; ++i) {
            pthread_mutex_lock(&args->bank->mutex[i]); //a medida que pasan las cuentas le enchufo un lock
        }
        for (int i = 0; i < args->bank->num_accounts; ++i) { //comprueba todo el rato el dinero que hay en  las cuentas
            balance1 = args->bank->accounts[i];
            bank_total+=balance1;
        }
        for (int i = 0; i < args->bank->num_accounts; ++i) {
            pthread_mutex_unlock(&args->bank->mutex[i]); //unlock de todas las cuentas
        }
        printf("\nTotal en la cuenta: %d\n",bank_total);
        usleep(args->delay);
    }
    return NULL;
}

// start opt.num_threads threads running on deposit.
struct thread_info *start_threads(struct options opt, struct bank *bank) {  //START_THREADS DEPOSITO
    int i;
    struct thread_info *threads;

    printf("creating %d deposit threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads); //reserva mem depositos

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    bank->iter1 = opt.iterations; //se iguala al numero de iteraciones que le pasas al bank

    // Create num_thread threads running swap()
    for (i = 0; i < opt.num_threads; i++) {
        //DEPOSIT
        threads[i].args = malloc(sizeof(struct args));
        threads[i].args->thread_num = i;
        threads[i].args->net_total = 0;
        threads[i].args->bank = bank;
        threads[i].args->delay = opt.delay;
        threads[i].args->iterations = opt.iterations;

        if (0 != pthread_create(&threads[i].id, NULL, deposit, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    return threads;
}

//INICIALIZAMOS LOS THREADS DE TRANSFERENCIA
struct thread_info *start_threads_2(struct options opt, struct bank *bank)  //START_THREADS TRANSFERENCIAS  (no necesita el neto total de deporsito)
{
    int i;
    struct thread_info *threads;

    printf("creating %d transfer threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);

    if (threads == NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    bank->iter2 = opt.iterations;

    for (i = 0; i < opt.num_threads; i++) {
        //TRANSFER
        threads[i].args = malloc(sizeof(struct args));
        threads[i].args->thread_num = i;
        threads[i].args->bank = bank;
        threads[i].args->delay = opt.delay;
        threads[i].args->iterations = opt.iterations;

        if (0 != pthread_create(&threads[i].id, NULL, transfer, threads[i].args)) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }
    return threads;
}

//INICIALIZAMOS EL THREAD AUXILIAR QUE RECORRE LAS TRANSFERENCIAS
struct auxiliar_thread_info *start_aux_thread(struct options opt, struct bank *bank){

    struct auxiliar_thread_info *thread;

    printf("Creating aux thread\n");
    thread = malloc(sizeof(struct auxiliar_thread_info));

    if(thread == NULL){
        printf("Not enough memory\n");
        exit(1);
    }

    thread->args = malloc(sizeof(struct auxiliar_args));
    thread->args->bank = bank;
    thread->args->delay = opt.delay;
    thread->args->final = FALSE;

    if(0!=pthread_create(&thread->id,NULL,aux_thread,thread->args)){
        printf("Could not create aux thread\n");
        exit(1);
    }
    return thread;
}

// Print the final balances of accounts and threads
void print_balances(struct bank *bank, struct thread_info *thrs, int num_threads) {
    int total_deposits=0, bank_total=0;
    printf("\nNet deposits by thread\n");

    for(int i=0; i < num_threads; i++) {
        printf("%d: %d\n", i, thrs[i].args->net_total);
        total_deposits += thrs[i].args->net_total;
    }
    printf("Total: %d\n", total_deposits);

    printf("\nAccount balance\n");
    for(int i=0; i < bank->num_accounts; i++) {
        printf("%d: %d\n", i, bank->accounts[i]);
        bank_total += bank->accounts[i];
    }
    printf("Total: %d\n", bank_total);
}

// wait for all threads to finish, print totals, and free memory
void wait2(struct options opt, struct bank *bank, struct thread_info *threads) {
    // Wait for the threads to finish
    for (int i = 0; i < opt.num_threads; i++) {
        pthread_join(threads[i].id, NULL);
    }
}

// allocate memory, and set all accounts to 0
void init_accounts(struct bank *bank, int num_accounts) {
    bank->num_accounts = num_accounts;
    bank->accounts     = malloc(bank->num_accounts * sizeof(int));
    bank->mutex        = malloc(bank->num_accounts * sizeof(pthread_mutex_t));
    bank->iter1         = 0;//es para hacer el reparto de iteraciones entre dep y transf
    bank->iter2         = 0;//se inicializa a 0 para evitar fallos

    for(int i=0; i < bank->num_accounts; i++) {
        bank->accounts[i] = 0;
        pthread_mutex_init(&bank->mutex[i], NULL);
    }
}
//free que no son threads
void free_cosas(struct bank *bank){ //para tenerlo bien organizado en el main
    for (int i = 0; i < bank->num_accounts; ++i) {
        pthread_mutex_destroy(&bank->mutex[i]);
    }
    free(bank->accounts);
    free(bank->mutex);
}

void free_threads(struct options opt,struct thread_info *thr,struct thread_info *thr2,struct auxiliar_thread_info *aux_thr){
    for (int i = 0; i < opt.num_threads; ++i) {
        free(thr[i].args);
        free(thr[i].args); //esto no lo tenia
    }
    free(thr); //se me olvidó hacer el free de threads de transferencia
    free(thr2);//esto no lo tenia
    free(aux_thr->args);
    free(aux_thr);
}

int main (int argc, char **argv)
{
    struct options      opt;
    struct bank         bank;
    struct thread_info *thrs1;
    struct thread_info *thrs2;
    struct auxiliar_thread_info *aux_thread;

    srand(time(NULL));

    // Default values for the options
    opt.num_threads  = 5;
    opt.num_accounts = 10;
    opt.iterations   = 100;
    opt.delay        = 10;

    read_options(argc, argv, &opt);

    init_accounts(&bank, opt.num_accounts);

    thrs1 = start_threads(opt, &bank); // Se crean los threads de depósito y transferencia.
    wait2(opt, &bank, thrs1);

    thrs2 = start_threads_2(opt,&bank);
    aux_thread = start_aux_thread(opt,&bank); // Se crea el thread auxiliar para comprobar que las transferencias están bien hechas.
    wait2(opt, &bank, thrs2);  // Ponemos wait2 porque David usa mac.

    aux_thread->args->final = TRUE; //para que se pare el hilo auxiliar
    pthread_join(aux_thread->id,NULL); 

    print_balances(&bank,thrs1,opt.num_threads);  // Imprime los "nets deposits" y el total en cada cuenta + dinero total entre las cuentas.
    //este TOTAL es el mismo total que debería imprimir el hilo auxiliar repetidamente.
 
 	//LIBERACIONES DE MEMORIA
    free_threads(opt,thrs1,thrs2,aux_thread);
    free_cosas(&bank);

    return 0;
}
