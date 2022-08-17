#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "thread.h"
#include "threadpool.h"
#include "util.h"

struct threadpool {
	void                     *jobs;
	pthread_t                *threads;
	struct threadpool_config  config;

	pthread_cond_t  cond;
	pthread_mutex_t cond_mutex;

	struct {
		size_t jobs;
		size_t threads;
	} num;

	bool shutdown;
};

// Get pointer to job slot #n.
static inline void *
jobslot (struct threadpool *p, const size_t n)
{
	return (char *) p->jobs + n * p->config.jobsize;
}

// Insert a job into the job queue. Needs mutex!
static bool
job_insert (struct threadpool *p, void *job)
{
	// Fail if the job queue is at capacity:
	if (p->num.jobs == p->config.num.jobs)
		return false;

	memcpy(jobslot(p, p->num.jobs++), job, p->config.jobsize);
	return true;
}

// Extract the first job from the job queue. Needs mutex!
static bool
job_take (struct threadpool *p, void *result)
{
	// Fail if there are no pending jobs:
	if (p->num.jobs == 0)
		return false;

	// Return first job in queue:
	memcpy(result, jobslot(p, 0), p->config.jobsize);

	// Copy last job to first slot:
	if (--p->num.jobs)
		memcpy(jobslot(p, 0), jobslot(p, p->num.jobs), p->config.jobsize);

	return true;
}

static void *
thread_main (void *data)
{
	void *job;
	struct threadpool *p = data;

	if ((job = malloc(p->config.jobsize)) == NULL)
		return NULL;

	while (p->shutdown == false) {

		// The pthread_cond_wait() must run under a locked mutex
		// (it does its own internal unlocking/relocking):
		if (thread_mutex_lock(&p->cond_mutex) == false)
			break;

		// Wait for predicate to change, allow spurious wakeups:
		while (!(job_take(p, job) || p->shutdown))
			thread_cond_wait(&p->cond, &p->cond_mutex);

		// Unlock the condition mutex to release the job structure:
		thread_mutex_unlock(&p->cond_mutex);

		// Run user-provided routine on data:
		if (p->shutdown == false)
			p->config.process(job);
	}

	free(job);
	return NULL;
}

static void
threads_destroy (struct threadpool *p)
{
	p->shutdown = true;
	thread_cond_broadcast(&p->cond);
	FOREACH_NELEM (p->threads, p->num.threads, t)
		thread_join(*t);
}

static bool
threads_create (struct threadpool *p)
{
	FOREACH_NELEM (p->threads, p->config.num.threads, t) {
		if (thread_create(t, thread_main, p) == false) {
			threads_destroy(p);
			return false;
		}
		p->num.threads++;
	}

	return true;
}

bool
threadpool_job_enqueue (struct threadpool *p, void *job)
{
	bool ret;

	if (p == NULL)
		return false;

	if ((ret = thread_mutex_lock(&p->cond_mutex))) {
		if ((ret = job_insert(p, job)))
			thread_cond_signal(&p->cond);

		thread_mutex_unlock(&p->cond_mutex);
	}

	return ret;
}

struct threadpool *
threadpool_create (const struct threadpool_config *config)
{
	struct threadpool *p;

	if (config == NULL || config->num.threads == 0 || config->num.jobs == 0)
		goto err0;

	if ((p = calloc(1, sizeof (*p))) == NULL)
		goto err0;

	p->config   = *config;
	p->shutdown = false;
	p->num.jobs = 0;

	if ((p->jobs = calloc(config->num.jobs, config->jobsize)) == NULL)
		goto err1;

	if ((p->threads = calloc(config->num.threads, sizeof (pthread_t))) == NULL)
		goto err2;

	if (thread_mutex_init(&p->cond_mutex) == false)
		goto err3;

	if (thread_cond_init(&p->cond) == false)
		goto err4;

	if (threads_create(p) == false)
		goto err5;

	return p;

err5:	thread_cond_destroy(&p->cond);
err4:	thread_mutex_destroy(&p->cond_mutex);
err3:	free(p->threads);
err2:	free(p->jobs);
err1:	free(p);
err0:	return NULL;
}

void
threadpool_destroy (struct threadpool *p)
{
	if (p == NULL)
		return;

	threads_destroy(p);

	thread_mutex_destroy(&p->cond_mutex);
	thread_cond_destroy(&p->cond);

	free(p->threads);
	free(p->jobs);
	free(p);
}
