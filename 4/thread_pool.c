#define _POSIX_C_SOURCE 200809
#include "thread_pool.h"
#include <pthread.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>

enum task_state {
	T_DETACHED,
	T_WAIT_PUSH,
	T_WAIT_THREAD,
	T_RUNNING,
	T_FINISHED,
	T_JOINED,
};

struct thread_task {
	thread_task_f function;
	void *arg;

	/* PUT HERE OTHER MEMBERS */
	pthread_t *t_worker; // task worker thread
	pthread_mutex_t *t_mutex; // task mutex
	pthread_cond_t *t_cond; // task conditional
	
	enum task_state state; // task state
	void *result; // result
};

struct thread_pool {
	pthread_t *threads;

	/* PUT HERE OTHER MEMBERS */
	int mx_thread_cnt; // max thread count
	int a_thread_cnt; // active thread count
	int b_thread_cnt; // busy thread count

	pthread_mutex_t *tp_mutex; // pool mutex

	struct thread_task **task_q; // task queue
	int t_cnt; // task count
	bool off; // is pool working
	pthread_cond_t *tp_cond; // task pool conditional

	pthread_condattr_t *attr; // conditional variable attribute

};

struct thread_args {
	struct thread_pool *pool;
	int t_id;
};

void *worker_thread(void *arg) {
	struct thread_args* args = (struct thread_args*)arg;
	struct thread_pool *t_pool = args->pool;
	while(true) {
		// Lock pool mutex
		pthread_mutex_lock(t_pool->tp_mutex);
		// Wait until there are tasks to do
		while(!t_pool->off && !t_pool->t_cnt)
			pthread_cond_wait(t_pool->tp_cond, t_pool->tp_mutex);
		// Pool is down
		if(t_pool->off) {
			pthread_mutex_unlock(t_pool->tp_mutex);
			free(args);
			pthread_exit(NULL);
		}
		// Get a new task
		struct thread_task *task = t_pool->task_q[--t_pool->t_cnt];
		++t_pool->b_thread_cnt;
		// Lock task mutex
		pthread_mutex_lock(task->t_mutex);
		// Pass the handle
		task->t_worker = t_pool->threads[args->t_id];
		// Unlock pool mutex
		pthread_mutex_unlock(t_pool->tp_mutex);
		// Set state as running
		if(task->state != T_DETACHED)
			task->state = T_RUNNING;
		// Save function and argument in the worker thread to unlock task mutex
		thread_task_f wt_f = task->function;
		void *wt_arg = task->arg;
		pthread_mutex_unlock(task->t_mutex);
		void *result = wt_f(wt_arg);
		// Lock task mutex and task pool mutex on completion
		pthread_mutex_lock(task->t_mutex);
		pthread_mutex_lock(t_pool->tp_mutex);
		task->result = result;
		// Set state as finished
		if(task->state != T_DETACHED)
			task->state = T_FINISHED;
		// Signal to join the thread
		pthread_cond_signal(task->t_cond);
		// Change the state if detached
		task->state = task->state == T_DETACHED ? T_JOINED : task->state;
		// Unlock mutex and delete task if state changed, just unlock mutex otherwise
		if(task->state == T_JOINED)
			pthread_mutex_unlock(task->t_mutex), thread_task_delete(task);
		else
			pthread_mutex_unlock(task->t_mutex);
		// Decrement busy thread count
		--t_pool->b_thread_cnt;
		// Unlock pool mutex
		pthread_mutex_unlock(t_pool->tp_mutex);
	}
}

int
thread_pool_new(int max_thread_count, struct thread_pool **pool)
{
	if(max_thread_count > TPOOL_MAX_THREADS || max_thread_count <= 0)
		return TPOOL_ERR_INVALID_ARGUMENT;
	
	// Error handling could be done better, but that is the best my brain could think of at 3AM
	struct thread_pool *t_pool_new = (struct thread_pool *) malloc(sizeof(struct thread_pool));
	if(t_pool_new == NULL)
		return -1;

	struct thread_task **t_queue_new = (struct thread_task **) malloc(TPOOL_MAX_TASKS * sizeof(struct thread_task *));
	if(t_queue_new == NULL) {
		free(t_pool_new);
		return -1;
	}
	pthread_t *threads_new = (pthread_t *) malloc(TPOOL_MAX_THREADS * sizeof(pthread_t));
	if(threads_new == NULL) {
		free(t_queue_new);
		free(t_pool_new);
		return -1;
	}

	pthread_mutex_t *t_mutex_new = (pthread_mutex_t *) malloc(sizeof(pthread_mutex_t));
	if(pthread_mutex_init(t_mutex_new, NULL) != 0) {
		free(threads_new);
		free(t_queue_new);
		free(t_pool_new);
		return -1;
	}

	// Setting condition variables
	pthread_cond_t *t_cond_new = (pthread_cond_t *) malloc(sizeof(pthread_cond_t));
	pthread_condattr_t *t_condattr_new = (pthread_condattr_t *) malloc(sizeof(pthread_condattr_t));

	pthread_condattr_init(t_condattr_new);
	pthread_condattr_setclock(t_condattr_new, CLOCK_MONOTONIC);
	pthread_cond_init(t_cond_new, t_condattr_new);

	*t_pool_new = (struct thread_pool) {
		.mx_thread_cnt = max_thread_count,
		.a_thread_cnt = 0,
		.b_thread_cnt = 0,
		.off = false,
		.tp_mutex = t_mutex_new,
		.task_q = t_queue_new,
		.tp_cond = t_cond_new,
		.attr = t_condattr_new,
		.threads = threads_new,	
		.t_cnt = 0
	};
	*pool = t_pool_new;
	return 0;
}	

int
thread_pool_thread_count(const struct thread_pool *pool)
{
	int a_threads = 0;
	pthread_mutex_lock(pool->tp_mutex);
	a_threads = pool->a_thread_cnt;
	pthread_mutex_unlock(pool->tp_mutex);
	return a_threads;
}

int
thread_pool_delete(struct thread_pool *pool)
{
	pthread_mutex_lock(pool->tp_mutex);
	// Tasks are still in progress
	if(pool->t_cnt || pool->b_thread_cnt) {
		pthread_mutex_unlock(pool->tp_mutex);
		return TPOOL_ERR_HAS_TASKS;
	}
	// Shutdown thread pool and wake up all threads
	pool->off = true;
	pthread_cond_broadcast(pool->tp_cond);
	// Unlock pool mutex
	pthread_mutex_unlock(pool->tp_mutex);
	// Join all active threads
	for(int i = 0; i < pool->a_thread_cnt; ++i)
		pthread_join(pool->threads[i], NULL);
	// Free task queue and thread handle list
	fprintf(stderr, "[thread_pool_delete] hello 1\n");
	free(pool->task_q);
	fprintf(stderr, "[thread_pool_delete] hello 2\n");
	free(pool->threads);
	fprintf(stderr, "[thread_pool_delete] hello 3\n");
	// Destroy mutex and conditionals
	pthread_cond_destroy(pool->tp_cond);
	free(pool->tp_cond);
	pthread_mutex_destroy(pool->tp_mutex);
	free(pool->tp_mutex);
	pthread_condattr_destroy(pool->attr);
	free(pool->attr);
	// Free pool
	free(pool);
	return 0;
}

int
thread_pool_push_task(struct thread_pool *pool, struct thread_task *task)
{
	// Lock pool
	pthread_mutex_lock(pool->tp_mutex);
	if(pool->t_cnt >= TPOOL_MAX_TASKS) {
		pthread_mutex_unlock(pool->tp_mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}
	// Lock task
	pthread_mutex_lock(task->t_mutex);
	// Put task in the queue
	pool->task_q[pool->t_cnt++] = task;
	// Change the state
	task->state = T_WAIT_THREAD;
	// All active threads are busy -- create more
	if(pool->a_thread_cnt < pool->mx_thread_cnt && pool->b_thread_cnt == pool->a_thread_cnt) {
		struct thread_args *args = (struct thread_args*) malloc(sizeof(struct thread_args));
		*args = (struct thread_args) {
			.pool = pool,
			.t_id = pool->a_thread_cnt,
		};
		pthread_create(&(pool->threads[pool->a_thread_cnt++]), NULL, worker_thread, (void*)args);
	}
	// Unlock pool
	pthread_mutex_unlock(pool->tp_mutex);
	// Unlock task
	pthread_mutex_unlock(task->t_mutex);
	// Wake up waiting thread
	pthread_cond_signal(pool->tp_cond);
	return 0;
}

int
thread_task_new(struct thread_task **task, thread_task_f function, void *arg)
{
	// Allocate memory
	*task = (struct thread_task *)malloc(sizeof(struct thread_task));
	(*task)->t_mutex = (pthread_mutex_t *)malloc(sizeof(pthread_mutex_t));
	(*task)->t_cond = (pthread_cond_t *)malloc(sizeof(pthread_cond_t));
	// Initialize mutexes and condition variables
	pthread_mutex_init((*task)->t_mutex, NULL);
	pthread_cond_init((*task)->t_cond, NULL);
	// Lock mutex
	pthread_mutex_lock((*task)->t_mutex);
	// Set attributes
	(*task)->function = function, (*task)->arg = arg, (*task)->state = T_WAIT_PUSH;
	// Unlock mutex
	pthread_mutex_unlock((*task)->t_mutex);
	return 0;
}

bool
thread_task_is_finished(const struct thread_task *task)
{
	if(!task)
		return TPOOL_ERR_INVALID_ARGUMENT;
	return task->state == T_FINISHED;
}

bool
thread_task_is_running(const struct thread_task *task)
{
	if(!task)
		return TPOOL_ERR_INVALID_ARGUMENT;
	return task->state == T_RUNNING;
}

int
thread_task_join(struct thread_task *task, void **result)
{
	if(!task)
		return TPOOL_ERR_INVALID_ARGUMENT;
	// Lock task mutex
	pthread_mutex_lock(task->t_mutex);
	// Task is detached
	if(task->state == T_DETACHED) {
		pthread_mutex_unlock(task->t_mutex);
		return TPOOL_ERR_TASK_DETACHED;
	}
	// Task is not pushed
	if(task->state == T_WAIT_PUSH) {
		pthread_mutex_unlock(task->t_mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;	
	}
	// Wait until task is finished
	while(task->state != T_FINISHED)
		pthread_cond_wait(task->t_cond, task->t_mutex);
	// Save result
	*result = task->result;
	// Change state
	task->state = T_JOINED;
	// Unlock task mutex
	pthread_mutex_unlock(task->t_mutex);
	return 0;
}

#ifdef NEED_TIMED_JOIN

int
thread_task_timed_join(struct thread_task *task, double timeout, void **result)
{
	/* IMPLEMENT THIS FUNCTION */
	(void)task;
	(void)timeout;
	(void)result;
	return TPOOL_ERR_NOT_IMPLEMENTED;
}

#endif

int
thread_task_delete(struct thread_task *task)
{
	if(!task)
		return TPOOL_ERR_INVALID_ARGUMENT;
	pthread_mutex_lock(task->t_mutex);
	if(task->state == T_DETACHED) {
		pthread_mutex_unlock(task->t_mutex);
		return TPOOL_ERR_TASK_DETACHED;
	}
	// Task is still in progress
	if(task->state == T_RUNNING || task->state == T_WAIT_THREAD || task->state == T_FINISHED) {
		pthread_mutex_unlock(task->t_mutex);
		return TPOOL_ERR_TASK_IN_POOL;
	}
	pthread_mutex_unlock(task->t_mutex);
	// Destroy task conditional
	pthread_cond_destroy(task->t_cond);
	free(task->t_cond);
	// Destroy mutex
	pthread_mutex_destroy(task->t_mutex);
	free(task->t_mutex);
	// Free task
	free(task);
	return 0;
}

#ifdef NEED_DETACH

int
thread_task_detach(struct thread_task *task)
{
	if(!task)
		return TPOOL_ERR_INVALID_ARGUMENT;
	pthread_mutex_lock(task->t_mutex);
	// Task is detached
	if(task->state == T_DETACHED) {
		pthread_mutex_unlock(task->t_mutex);
		return TPOOL_ERR_TASK_DETACHED;
	}
	// Task is not pushed
	if(task->state == T_WAIT_PUSH) {
		pthread_mutex_unlock(task->t_mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}
	// Delete if the task finished already, change state and proceed otherwise
	if(task->state == T_FINISHED) {
		task->state = T_JOINED;
		pthread_mutex_unlock(task->t_mutex);
		thread_task_delete(task);
	}
	else {
		task->state = T_DETACHED;
		pthread_mutex_unlock(task->t_mutex);
	}
	return 0;
}

#endif
