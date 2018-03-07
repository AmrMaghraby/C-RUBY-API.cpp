/*
Launch some ruby script from threaded C code.
1. Create test.rb script, like this example with random number factorial output:
$ cat test.rb
def fact(n) (2..n).reduce(1, :*) end
puts "#{x = rand(100)}! = #{fact(x)}"
2. Compile and run:
$ gcc ruby-run.c -o ruby-run -lpthread -lruby
$ ./ruby-run
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <ruby/ruby.h>


#define NUM_THREADS 4
#define SCRIPT_PATH "./test.rb"



// This data will be shared across threads
long shared_checksum;

// Global mutex
pthread_mutex_t mutex_lock;


// Thread worker
void *worker(void *arg) {
	long tNum = (long)arg;

	printf("launched thread #%ld\n", tNum + 1);

	// Sleep random time [0..1000ms] to emulate some work
	usleep( rand() % 1000000 );

	// Get mutex to print shared data and to run Ruby C API
	pthread_mutex_lock(&mutex_lock);
	if (1) {
		printf("running ruby in thread #%ld\n", tNum + 1);

		// Sleep random time [0..500ms] to emulate some synchronised work
		usleep( rand() % 500000 );


		// Run Ruby script
		// http://silverhammermba.github.io/emberb/c/#ruby-in-c-threads
		// Ruby VM is not at all thread safe. Ideally, all of your API code should run in a single thread.
		// If not, you’ll probably need to wrap every API call with a locked mutex to make sure
		// that you never ever have multiple threads interacting with the API at the same time.

		int state;

		ruby_script(SCRIPT_PATH);
		VALUE script = rb_str_new_cstr(SCRIPT_PATH);
		rb_load_protect(script, 1, &state);

		if (state) {
			// Get last exception
			VALUE exception = rb_errinfo();
			rb_set_errinfo(Qnil);

			if ( RTEST(exception) ) {
				fprintf(stderr, "Ruby script raised exception\n");
				rb_warn("%"PRIsVALUE"", exception);
			}
		}

		// Increment shared checksum data, for the sake of example
		shared_checksum += (tNum + 1) * 10;
		printf("shared checksum: %ld\n", shared_checksum);
	}
	pthread_mutex_unlock(&mutex_lock);

	pthread_exit(NULL);
}

int main(int argc, char* argv[])
{
	pthread_t 	threads[NUM_THREADS];
	long 		i;


	// Setup Ruby VM
	if ( ruby_setup() ) {
		fprintf(stderr, "Error on ruby_setup\n");
		return EXIT_FAILURE;
	}
	ruby_init_loadpath();


	// Init shared data
	shared_checksum = 0;

	// Seed random generator for thread timeouts
	srand( (unsigned) time(NULL) );

	// Init mutex
	pthread_mutex_init(&mutex_lock, NULL);

	// Create some threads
	for (i = 0; i < NUM_THREADS; i++) {
		if ( pthread_create(&threads[i], NULL, worker, (void *)i) ) {
			fprintf(stderr, "Error creating thread\n");
			return EXIT_FAILURE;
		}
	}

	// Wait for threads to finish
	for (i = 0; i < NUM_THREADS; i++) {
		if ( pthread_join(threads[i], NULL) ) {
			fprintf(stderr, "Error joining thread\n");
			return EXIT_FAILURE;
		}
	}


	// Cleanup Ruby VM
	if ( ruby_cleanup(0) ) {
		fprintf(stderr, "Error on ruby_cleanup\n");

	}

	// Verify checksum
	if (shared_checksum == (NUM_THREADS+1)*5*NUM_THREADS) {
		puts("CHEKSUM OK");
		return EXIT_SUCCESS;
	} else {
		fprintf(stderr, "CHEKSUM FAILED\n");
		return EXIT_FAILURE;
	}

}
