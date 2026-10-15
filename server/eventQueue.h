#ifndef EVENTQUEUE_H
#define EVENTQUEUE_H

#include <pthread.h>
#include <stdbool.h>
#include <time.h>

typedef enum EventType {
    EVENT_NONE = 0,
    EVENT_WORD_COMPLETED,    // Player completed a word (sent '3')
    EVENT_SENTENCE_FINISHED, // Player finished the sentence (sent '2')
    EVENT_DISCONNECT         // Player disconnected or connection dropped
} EventType;

typedef struct GameEvent {
    EventType type;
    int playerIndex;
    struct timespec timestamp;
} GameEvent;

typedef struct EventNode {
    GameEvent event;
    struct EventNode *next;
} EventNode;

#define MAX_EVENT_QUEUE_CAPACITY 4096

typedef struct EventQueue {
    EventNode *head;
    EventNode *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    bool closed;
    int count;
} EventQueue;

void event_queue_init(EventQueue *q);
void event_queue_push(EventQueue *q, GameEvent ev);
bool event_queue_pop(EventQueue *q, GameEvent *ev, int timeout_ms);
void event_queue_close(EventQueue *q);
void event_queue_destroy(EventQueue *q);

#endif // EVENTQUEUE_H
