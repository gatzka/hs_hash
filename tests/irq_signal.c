/*
 * Real asynchronous interrupts: a second thread keeps sending SIGUSR1 to
 * the thread that updates the table. The signal handler plays the role of
 * an interrupt handler on the writer's CPU: it looks up the key that is
 * being updated and checks that it is present and that all value words
 * belong to the same update.
 *
 * Usage: irq_signal [seconds]
 */

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <hs_hash/hs_hash.h>

#define VALUES 16
#define KEYS 700u /* about 68% of the 1024 slots */

DECLARE_HS_HASHTABLE_UINT32(sig, 10, VALUES)

static HS_HASHTABLE_TYPE(sig) *table;
static pthread_t writer;
static volatile sig_atomic_t stop;
static volatile uint32_t current_key = 1;
static volatile unsigned long interrupts;
static volatile unsigned long missing;
static volatile unsigned long torn;

/* All words of a value are derived from the same generation number. */
static void make_value(struct value_sig *v, uintptr_t generation)
{
	unsigned i;
	for (i = 0; i < VALUES; i++) {
		v->vals[i] = (void *)(generation * VALUES + i);
	}
}

static int is_consistent(const struct value_sig *v)
{
	uintptr_t base = (uintptr_t)v->vals[0];
	unsigned i;

	if (base % VALUES != 0) {
		return 0;
	}
	for (i = 1; i < VALUES; i++) {
		if ((uintptr_t)v->vals[i] != base + i) {
			return 0;
		}
	}
	return 1;
}

static void handler(int signal_number)
{
	struct value_sig out;

	(void)signal_number;
	interrupts++;
	if (HS_HASHTABLE_GET(sig, table, current_key, &out) != HS_HASHTABLE_SUCCESS) {
		missing++;
	} else if (!is_consistent(&out)) {
		torn++;
	}
}

static void *interrupter(void *arg)
{
	(void)arg;
	while (!stop) {
		pthread_kill(writer, SIGUSR1);
	}
	return NULL;
}

static double now(void)
{
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return (double)t.tv_sec + (double)t.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
	double seconds = (argc > 1) ? atof(argv[1]) : 1.0;
	double end;
	uintptr_t generation = 1;
	unsigned long updates = 0;
	unsigned long full = 0;
	uint32_t state = 7;
	uint32_t key;
	struct value_sig v;
	struct sigaction action;
	pthread_t thread;

	table = HS_HASHTABLE_CREATE(sig);
	if (table == NULL) {
		printf("irq_signal: out of memory\n");
		return 1;
	}
	for (key = 1; key <= KEYS; key++) {
		make_value(&v, generation++);
		if (HS_HASHTABLE_PUT(sig, table, key, v, NULL) != HS_HASHTABLE_SUCCESS) {
			printf("irq_signal: filling the table failed\n");
			return 1;
		}
	}

	action.sa_handler = handler;
	action.sa_flags = SA_RESTART;
	sigemptyset(&action.sa_mask);
	sigaction(SIGUSR1, &action, NULL);
	writer = pthread_self();
	if (pthread_create(&thread, NULL, interrupter, NULL) != 0) {
		printf("irq_signal: can't create thread\n");
		return 1;
	}

	end = now() + seconds;
	while (now() < end) {
		int i;
		for (i = 0; i < 1000; i++) {
			state = state * 1103515245u + 12345u;
			current_key = 1 + (state >> 8) % KEYS;
			make_value(&v, generation++);
			if (HS_HASHTABLE_PUT(sig, table, current_key, v, NULL) == HS_HASHTABLE_FULL) {
				full++;
			}
			updates++;
		}
	}
	stop = 1;
	pthread_join(thread, NULL);

	HS_HASHTABLE_DELETE(sig, table);
	printf("irq_signal: %lu updates (%lu FULL), %lu interrupts, %lu missing keys, %lu torn values\n",
	       updates, full, (unsigned long)interrupts, (unsigned long)missing, (unsigned long)torn);
	return (interrupts == 0 || missing != 0 || torn != 0) ? 1 : 0;
}
