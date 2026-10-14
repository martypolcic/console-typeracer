#include "eventQueue.h"
#include <stdlib.h>
#include <sys/time.h>

void event_queue_init(EventQueue *q)
{
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
    q->closed = false;
}

void event_queue_push(EventQueue *q, GameEvent ev)
{
    EventNode *node = malloc(sizeof(EventNode));
    if (!node)
    {
        return;
    }
    node->event = ev;
    node->next = NULL;

    pthread_mutex_lock(&q->mutex);
    if (q->closed)
    {
        free(node);
        pthread_mutex_unlock(&q->mutex);
        return;
    }
    if (q->tail)
    {
        q->tail->next = node;
        q->tail = node;
    }
    else
    {
        q->head = node;
        q->tail = node;
    }
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

bool event_queue_pop(EventQueue *q, GameEvent *ev, int timeout_ms)
{
    pthread_mutex_lock(&q->mutex);
    while (q->head == NULL && !q->closed)
    {
        if (timeout_ms < 0)
        {
            pthread_cond_wait(&q->cond, &q->mutex);
        }
        else if (timeout_ms == 0)
        {
            break;
        }
        else
        {
            struct timespec ts;
            struct timeval now;
            gettimeofday(&now, NULL);
            ts.tv_sec = now.tv_sec + timeout_ms / 1000;
            ts.tv_nsec = (now.tv_usec + (timeout_ms % 1000) * 1000) * 1000;
            if (ts.tv_nsec >= 1000000000L)
            {
                ts.tv_sec += 1;
                ts.tv_nsec -= 1000000000L;
            }
            int rc = pthread_cond_timedwait(&q->cond, &q->mutex, &ts);
            if (rc != 0)
            {
                break;
            }
        }
    }

    if (q->head == NULL)
    {
        pthread_mutex_unlock(&q->mutex);
        return false;
    }

    EventNode *node = q->head;
    *ev = node->event;
    q->head = node->next;
    if (q->head == NULL)
    {
        q->tail = NULL;
    }
    free(node);
    pthread_mutex_unlock(&q->mutex);
    return true;
}

void event_queue_close(EventQueue *q)
{
    pthread_mutex_lock(&q->mutex);
    q->closed = true;
    pthread_cond_broadcast(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

void event_queue_destroy(EventQueue *q)
{
    pthread_mutex_lock(&q->mutex);
    q->closed = true;
    EventNode *curr = q->head;
    while (curr)
    {
        EventNode *tmp = curr;
        curr = curr->next;
        free(tmp);
    }
    q->head = NULL;
    q->tail = NULL;
    pthread_mutex_unlock(&q->mutex);

    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}
