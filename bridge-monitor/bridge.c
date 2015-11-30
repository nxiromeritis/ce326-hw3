#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>

// Start of Monitor
volatile int ncars;			// total number of cars (randomly arriving)
volatile int n;				// how many cars can the bridge handle
volatile int crossing[2];	// how many cars are currently crossing (left/right side)
volatile int blocked[2];	// how many cars are waiting to cross (left/right side)
volatile int limit[2];		// max cars going to be unblocked (left/right side)
							// gets incemented by unlocked cars until it is equal to bridge size

pthread_mutex_t cs_common_mtx;	// cs mutex
pthread_cond_t queue[2];

volatile int cars_done = 0;		// used only to prevent main from returning before
pthread_mutex_t main_mtx;		// the end of the simulation


void enter(int direction) {
	if (pthread_mutex_lock(&cs_common_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	// block if opposite direction is busy crossing the bridge
	// Also block if bridge if full or too many cars of the same direction have already passed
	while (crossing[!direction] || (crossing[direction] == n) || (limit[direction] == n)) {
		blocked[direction]++;
		if (pthread_cond_wait(&queue[direction], &cs_common_mtx)) {
			perror("pthread_cond_wait");
			exit(1);
		}
		blocked[direction]--;
	}
	printf("%s: car unblocked\n", direction?"right":"left");

	// enter bridge
	crossing[direction]++;
	limit[direction]++;

	// unblock up to n cars from the same direction
	if ( (blocked[direction] > 0) && ( crossing[direction] < n ) && ( limit[direction] < n) ) {
		if (pthread_cond_signal(&queue[direction])) {
			perror("pthread_cond_signal");
			exit(1);
		}
	}

	if (pthread_mutex_unlock(&cs_common_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}
}



void leave(int direction) {
	if (pthread_mutex_lock(&cs_common_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

	crossing[direction]--;

	// if this is the last car on the bridge
	if (crossing[direction] == 0) {
		// if there are cars waiting at the opposite give them priority
		if ( blocked[!direction] > 0 ) {
			printf("%s: Giving priority to other side (cars waiting there)\n", direction?"right":"left");
			limit[!direction] = 0;
			if (pthread_cond_signal(&queue[!direction])) {
				perror("pthread_cond_signal");
				exit(1);
			}
		}
		// else give the same direction priority
		else if ( blocked[direction] > 0) {
			printf("%s: No cars waiting at the opposite..But there are cars waiting here (priority)\n", direction?"right":"left");
			limit[direction] = 0;
			if (pthread_cond_signal(&queue[direction])) {
				perror("pthread_cond_signal");
				exit(1);
			}
		}
	}


	if (pthread_mutex_unlock(&cs_common_mtx)) {
		perror("pthread_mutex_unlock");
		exit(1);
	}
}
// end of monitor


void init() {

	crossing[0] = 0;
	crossing[1] = 0;
	blocked[0] = 0;
	blocked[1] = 0;
	limit[0] = 0;
	limit[1] = 0;

	// cs_common_mtx = 1
	if (pthread_mutex_init(&cs_common_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}

	if (pthread_cond_init(&queue[0], NULL)) {
		perror("pthread_cond_init");
		exit(1);
	}

	if (pthread_cond_init(&queue[1], NULL)) {
		perror("pthread_cond_init");
		exit(1);
	}

	// used by main to block until the simulation ends
	// main_mtx = 0
	if (pthread_mutex_init(&main_mtx, NULL)) {
		perror("pthread_mutex_init");
		exit(1);
	}
	if (pthread_mutex_lock(&main_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}

}




void *car_func(void *arg) {
	int direction;

	direction = *((int *)arg);
	srand(time(NULL));
	enter(direction);
	sleep(rand()%3+1);
	printf("%s: car crossing bridge\n", (direction?"right":"left"));
	leave(direction);


	// last car in general notifies main to terminate
	cars_done++;
	if (cars_done == ncars) {
		// up(main_mtx);
		if (pthread_mutex_unlock(&main_mtx)) {
			perror("pthread_mutex_unlock");
			exit(1);
		}
	}
	return(NULL);

}


int main(int argc,char *argv[]) {
	int left = 0;
	int right = 1;
	int i;
	pthread_t *car_thread;

	printf("\nEnter total number of cars on the bridge: ");
	scanf("%d", &n);
	printf("\nEnter total number of cars arriving: ");
	scanf("%d", &ncars);

	car_thread = (pthread_t *)malloc(ncars*sizeof(pthread_t));
	if (car_thread == NULL) {
		printf("Memory allocation problems\n");
		exit(1);
	}

	init();

	srand(time(NULL));

	// cars arriving randomly
	for (i=0; i<ncars; i++) {
		if (rand()%2) {
			/*printf("right_side: new car arriving\n");*/
			if (pthread_create(&car_thread[i], NULL, car_func, &right)) {
				perror("pthread_create");
				exit(1);
			}
		}
		else {
			/*printf("left_side: new car arriving\n");*/
			if (pthread_create(&car_thread[i], NULL, car_func, &left)) {
				perror("pthread_create");
				exit(1);
			}
		}
		sleep(rand()%1);
	}

	// main thread blocks until simulation has ended (last car awakens it)
	// down(main_mtx);
	if (pthread_mutex_lock(&main_mtx)) {
		perror("pthread_mutex_lock");
		exit(1);
	}
	printf("simulation has ended\n");
	return(0);
}
