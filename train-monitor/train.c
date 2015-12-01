#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>


// start of monitor

volatile int count;
volatile int blocked;
volatile int train_avl;
volatile int train_waits;
pthread_mutex_t cs_mtx;
pthread_cond_t queue;
pthread_cond_t trip;
pthread_cond_t aboard;

void enter_train(int n) {
	if (pthread_mutex_lock(&cs_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// only (n+1)th passenger will return to this point
	while(1) {
		// this while is needed to prevent to guarantee there will be no
		// problems if our monitor is OPEN
		while (!train_avl) {
			blocked++;
			if (pthread_cond_wait(&queue, &cs_mtx)) {
				perror("pthread_cond_wait");
				exit(1);
			}
			blocked--;
		}

		count++;
		// (n+t)th passenger signals train_avl
		if (count == (n+1)) {
			train_avl = 0;	//train should not be available for more passengers
			if (train_waits) {
				printf("Passenger: Waking up train\n");
				if (pthread_cond_signal(&aboard)) {
					perror("pthread_cond_signal");
					exit(1);
				}
			}
		}
		else {
			// all passengers except (n+1)th should execute the loop only once
			break;
		}
	}

	// waiting for trip to end
	printf("Passenger: I'm in\n");
	if (pthread_cond_wait(&trip, &cs_mtx)) {
		perror("pthread_cond_wait");
		exit(1);
	}

	if (pthread_mutex_unlock(&cs_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}
}


void all_in(int n) {
	if (pthread_mutex_lock(&cs_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// train waits for (n+1)th passenger's signal
	if (count <= n) {
		train_waits = 1;
		printf("Train: I am sleeping...\n");
		if (pthread_cond_wait(&aboard, &cs_mtx)) {
			perror("pthread_cond_signal");
			exit(1);
		}
	}

	count = 0;
	train_waits = 0;

	printf("Train: The journey begins!!!\n");
	if (pthread_mutex_unlock(&cs_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}
}



void all_out_return(int n) {
	int i;

	if (pthread_mutex_lock(&cs_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// trip has ended. signal passengers to leave train
	printf("Train: End of trip. Signaling passengers to get off.\n");
	for(i=0; i<n; i++) {
		if (pthread_cond_signal(&trip)) {
			perror("pthread_cond_signal");
			exit(1);
		}
	}

	// train becomes available for new passengers
	printf("Train: All passengers down. Returning...\n");
	train_avl = 1;
	sleep(rand()%3+1);

	// unblock up to n passengers in queue
	printf("Train: Waiting for new passengers.\n");
	i = 0;
	while ((i<n) && (blocked>0)) {
		if (pthread_cond_signal(&queue)) {
			perror("pthread_cond_signal");
			exit(1);
		}
		i++;
	}

	if (pthread_mutex_unlock(&cs_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}
}
// end of monitor



void init() {

	count = 0;
	train_avl = 1;

	if (pthread_mutex_init(&cs_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}

	if (pthread_cond_init(&queue, NULL)) {
		perror("pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&trip, NULL)) {
		perror("pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&aboard, NULL)) {
		perror("pthread_cond_init");
		exit(1);
	}
}


void *passenger_func(void *arg) {
	int n;
	n = *((int *)arg);

	enter_train(n);

	return(NULL);
}


void *train_func(void *arg) {
	int n;
	n = *((int *)arg);
	srand(time(NULL));

	while (1) {
		all_in(n);

		printf("Train: Traveling...\n");
		sleep(rand()%4+1);

		all_out_return(n);
	}

	return(NULL);
}


int main(int argc, char *argv[]) {
	int n;
	int i;
	int passengers;
	pthread_t train;
	pthread_t *passenger;


	printf("Enter number of train seats: ");
	scanf("%d", &n);

	printf("(Note that train leaves ONLY if it has no empty seats left)\n");
	printf("Enter total passengers randomly arriving: ");
	scanf("%d", &passengers);
	passenger = (pthread_t *)malloc(passengers*sizeof(pthread_t));
	if (passenger == NULL) {
		printf("memory allocation problems\n");
		exit(1);
	}

	init();		// initialize mutexes

	srand(time(NULL));
	// create train thread
	if (pthread_create(&train, NULL, train_func, &n)) {
		perror("pthread_create");
		exit(1);
	}
	// create passengers threads
	for(i=0; i<passengers; i++) {
		if (pthread_create(&passenger[i], NULL, passenger_func, &n)) {
			perror("pthread_create");
			exit(1);
		}
		sleep(rand()%2+1);
	}


	// main waits for some seconds...to let us watch the simulation
	sleep(60);


	return(0);
}
