#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>   // For INT_MAX
#include <stdbool.h>


#ifdef VERBOSE
  #define VERBOSE_PRINT(...)  do { printf(__VA_ARGS__); } while(0)
#else
  #define VERBOSE_PRINT(...)  do {} while(0)
#endif

/* -------------------------------------------------------------------------
 * Data Structures
 * ------------------------------------------------------------------------- */

/* -------------------------
   Process info / PCB
-------------------------- */
typedef struct {
    int   id;               
    int   arrival_time;     
    int   num_bursts;       
    int   cpu_bursts[32];   
    int   io_bursts[32];    

    int   current_burst;    
    int   remaining_cpu;    
    int   total_cpu_io;     
    int   wait_time;        
    int   finish_time;      

    int   last_ready_time;  
    int   start_run_time;   
} PCB;

/* -------------------------
   Event Types
-------------------------- */
typedef enum {
    EVT_ARRIVE      = 0,   
    EVT_CPU_COMPLETE = 1,   
    EVT_CPU_TIMEOUT  = 2    
} EventType;

/* -------------------------
   Event structure
-------------------------- */
typedef struct {
    int        time;       
    EventType  type;
    int        pid;       
} Event;

/* -------------------------
   Min-heap for events
-------------------------- */
typedef struct {
    Event *heapArray;
    int    capacity;
    int    size;
} EventMinHeap;

/* -------------------------
   Ready Queue (FIFO)
-------------------------- */
typedef struct {
    int *data;
    int  capacity;
    int  front;
    int  rear;
    int  count;
} ReadyQueue;

/* -------------------------------------------------------------------------
 * Global or File-Scoped Variables
 * ------------------------------------------------------------------------- */
static PCB  *g_processes = NULL;
static int   g_numProcs   = 0;            
static const char *INPUT_FILE = "input.txt"; 

/* -------------------------------------------------------------------------
 * Function Prototypes
 * ------------------------------------------------------------------------- */

/* --- Ready Queue Ops --- */
void RQ_init(ReadyQueue *q, int capacity);
void RQ_destroy(ReadyQueue *q);
void RQ_enqueue(ReadyQueue *q, int pid, int current_time);
int  RQ_dequeue(ReadyQueue *q);
int  RQ_isEmpty(const ReadyQueue *q);

/* --- Event Min-Heap Ops --- */
void eventHeap_init(EventMinHeap *mh, int capacity);
void eventHeap_push(EventMinHeap *mh, Event ev);
Event eventHeap_pop(EventMinHeap *mh);
int  eventHeap_empty(const EventMinHeap *mh);

/* Compare events for heap ordering */
int compareEvents(const Event *a, const Event *b);

/* --- Input and Simulation --- */
int  read_input_file(const char *filename);
void run_scheduler(long q);
void schedule_next(ReadyQueue *readyQ, EventMinHeap *eventQ,
                   int *running_pid, long *running_end_time,
                   long *cpu_idle_time, long *cpu_busy_until,
                   long current_time, long q);

/* --- Helpers & Printing --- */
void print_per_process_metrics(long current_time, int pid);
void print_aggregate_metrics(long simulation_end, long total_wait, long total_idle);

/* -------------------------------------------------------------------------
 * main()
 * ------------------------------------------------------------------------- */
int main(void)
{
    
    g_numProcs = read_input_file(INPUT_FILE);
    if (g_numProcs <= 0) {
        fprintf(stderr, "Error reading %s or no processes found.\n", INPUT_FILE);
        return 1;
    }
    free(g_processes);

    // 1) FCFS => q = a large number
    printf("**** FCFS Scheduling ****\n");
    run_scheduler(1000000000L);

    // 2) RR => q = 10
    printf("**** RR Scheduling with q = 10 ****\n");
    run_scheduler(10);

    // 3) RR => q = 5
    printf("**** RR Scheduling with q = 5 ****\n");
    run_scheduler(5);

    return 0;
}

/* -------------------------------------------------------------------------
 * read_input_file
 * ------------------------------------------------------------------------- */
int read_input_file(const char *filename)
{
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen()");
        return -1;
    }

    int n;
    fscanf(fp, "%d", &n);
    if (n <= 0) {
        fclose(fp);
        return 0;
    }

    g_processes = (PCB *)calloc(n, sizeof(PCB));
    if (!g_processes) {
        fclose(fp);
        return -1;
    }

    for (int i = 0; i < n; i++) {
        int pid, arrival;
        fscanf(fp, "%d %d", &pid, &arrival);

        g_processes[i].id             = pid;
        g_processes[i].arrival_time   = arrival;
        g_processes[i].finish_time    = 0;
        g_processes[i].wait_time      = 0;
        g_processes[i].total_cpu_io   = 0;
        g_processes[i].current_burst  = 0;
        g_processes[i].remaining_cpu  = 0;
        g_processes[i].last_ready_time= 0;
        g_processes[i].start_run_time = 0;

        // Read alternating CPU and IO times
        int cpu_idx = 0;
        int io_idx  = 0;
        while (1) {
            int cpu;
            fscanf(fp, "%d", &cpu);
            if (cpu == -1) {
                // End
                break;
            }
            g_processes[i].cpu_bursts[cpu_idx] = cpu;
            g_processes[i].total_cpu_io += cpu;
            cpu_idx++;

            // next IO burst
            int io;
            fscanf(fp, "%d", &io);
            if (io == -1) {
                // means process ended exactly after that CPU
                break;
            }
            g_processes[i].io_bursts[io_idx] = io;
            g_processes[i].total_cpu_io += io;
            io_idx++;
        }
        g_processes[i].num_bursts = cpu_idx;
        g_processes[i].remaining_cpu = g_processes[i].cpu_bursts[0];
    }

    fclose(fp);
    return n;
}

/* -------------------------------------------------------------------------
 * run_scheduler
 * ------------------------------------------------------------------------- */
void run_scheduler(long q)
{
    // Re-read the input so each run is fresh
    g_numProcs = read_input_file(INPUT_FILE);
    if (g_numProcs <= 0) {
        printf("No processes or read error.\n");
        return;
    }

    // Event queue
    EventMinHeap eventQ;
    eventHeap_init(&eventQ, g_numProcs * 4);

    // Ready queue
    ReadyQueue readyQ;
    RQ_init(&readyQ, g_numProcs + 10);

    // CPU tracking
    long current_time   = 0;
    long cpu_idle_time  = 0;
    long cpu_busy_until = 0; // We track until what time the CPU is busy
    int  running_pid    = -1;
    long running_end_time = 0;

    int finished_count = 0;

    // Insert initial arrival events
    for (int i = 0; i < g_numProcs; i++) {
        Event e;
        e.time = g_processes[i].arrival_time;
        e.type = EVT_ARRIVE;
        e.pid  = i;
        eventHeap_push(&eventQ, e);
    }

    // Main simulation loop
    while (finished_count < g_numProcs && !eventHeap_empty(&eventQ)) {
        Event evt = eventHeap_pop(&eventQ);
        current_time = evt.time;

        switch (evt.type) {
            case EVT_ARRIVE: {
                // Process arrives, put it in ready queue
                RQ_enqueue(&readyQ, evt.pid, current_time);
                VERBOSE_PRINT("%ld : Process %d joins ready queue upon arrival\n",
                              current_time, g_processes[evt.pid].id);

                // If CPU is free, schedule next
                if (running_pid < 0) {
                    schedule_next(&readyQ, &eventQ,
                                  &running_pid, &running_end_time,
                                  &cpu_idle_time, &cpu_busy_until,
                                  current_time, q);
                }
            } break;

            case EVT_CPU_COMPLETE: {
                int pid = evt.pid;

                // How long did it actually run?
                int used_time = current_time - g_processes[pid].start_run_time;
                if (used_time < 0) used_time = 0;

                g_processes[pid].remaining_cpu -= used_time;
                if (g_processes[pid].remaining_cpu < 0) {
                    g_processes[pid].remaining_cpu = 0; // safety
                }

                // The CPU actually freed up at current_time
                // If we scheduled it for a longer slice,
                // we must update cpu_busy_until if the process ended earlier
                if (current_time < cpu_busy_until) {
                    cpu_busy_until = current_time;
                }

                running_pid = -1;

                // Check if last CPU burst
                int cb = g_processes[pid].current_burst + 1;
                if (cb >= g_processes[pid].num_bursts) {
                    // The process ends
                    g_processes[pid].finish_time = current_time;
                    finished_count++;
                    print_per_process_metrics(current_time, pid);
                } else {
                    // Next is IO
                    long io_time = g_processes[pid].io_bursts[cb - 1];
                    long wakeup  = current_time + io_time;
                    Event newEvt;
                    newEvt.time = wakeup;
                    newEvt.type = EVT_ARRIVE;
                    newEvt.pid  = pid;
                    eventHeap_push(&eventQ, newEvt);

                    // Move to next CPU burst
                    g_processes[pid].current_burst = cb;
                    g_processes[pid].remaining_cpu =
                        g_processes[pid].cpu_bursts[cb];

                    VERBOSE_PRINT("%ld : Process %d will return after IO at %ld\n",
                                  current_time, g_processes[pid].id, wakeup);
                }

                // Try to schedule next
                schedule_next(&readyQ, &eventQ,
                              &running_pid, &running_end_time,
                              &cpu_idle_time, &cpu_busy_until,
                              current_time, q);
            } break;

            case EVT_CPU_TIMEOUT: {
                int pid = evt.pid;

                // How long did it actually run?
                int used_time = current_time - g_processes[pid].start_run_time;
                if (used_time < 0) used_time = 0;

                g_processes[pid].remaining_cpu -= used_time;
                if (g_processes[pid].remaining_cpu < 0) {
                    g_processes[pid].remaining_cpu = 0; // safety
                }

                // The CPU freed up at current_time if we scheduled more
                // but it ended exactly here
                if (current_time < cpu_busy_until) {
                    cpu_busy_until = current_time;
                }

                running_pid = -1;

                // If the CPU burst still has time left, requeue
                if (g_processes[pid].remaining_cpu > 0) {
                    VERBOSE_PRINT("%ld : Process %d joins ready queue after timeout\n",
                                  current_time, g_processes[pid].id);
                    RQ_enqueue(&readyQ, pid, current_time);
                } else {
                    // Possibly it ended exactly at this time
                    int cb = g_processes[pid].current_burst + 1;
                    if (cb >= g_processes[pid].num_bursts) {
                        // done
                        g_processes[pid].finish_time = current_time;
                        finished_count++;
                        print_per_process_metrics(current_time, pid);
                    } else {
                        // Next IO
                        long io_time = g_processes[pid].io_bursts[cb - 1];
                        long wakeup  = current_time + io_time;
                        Event newEvt;
                        newEvt.time = wakeup;
                        newEvt.type = EVT_ARRIVE;
                        newEvt.pid  = pid;
                        eventHeap_push(&eventQ, newEvt);

                        g_processes[pid].current_burst = cb;
                        g_processes[pid].remaining_cpu =
                            g_processes[pid].cpu_bursts[cb];

                        VERBOSE_PRINT("%ld : Process %d will return after IO at %ld\n",
                                      current_time, g_processes[pid].id, wakeup);
                    }
                }

                // Schedule next
                schedule_next(&readyQ, &eventQ,
                              &running_pid, &running_end_time,
                              &cpu_idle_time, &cpu_busy_until,
                              current_time, q);
            } break;
        } // switch
    } // while

    // Simulation ends
    long simulation_end = 0;
    long total_wait     = 0;
    for (int i = 0; i < g_numProcs; i++) {
        if (g_processes[i].finish_time > simulation_end) {
            simulation_end = g_processes[i].finish_time;
        }
        total_wait += g_processes[i].wait_time;
    }

    // Optionally, if everything ended, show CPU goes idle at simulation_end
    VERBOSE_PRINT("%ld : CPU goes idle\n", simulation_end);

    // Print aggregates
    print_aggregate_metrics(simulation_end, total_wait, cpu_idle_time);

    // Cleanup
    RQ_destroy(&readyQ);
    free(eventQ.heapArray);
    free(g_processes);
    g_processes = NULL;
}

/* -------------------------------------------------------------------------
 * schedule_next
 * ------------------------------------------------------------------------- */
void schedule_next(ReadyQueue *readyQ, EventMinHeap *eventQ,
                   int *running_pid, long *running_end_time,
                   long *cpu_idle_time, long *cpu_busy_until,
                   long current_time, long q)
{
    if (*running_pid >= 0) {
        // CPU is already busy
        return;
    }

    if (!RQ_isEmpty(readyQ)) {
        int pid = RQ_dequeue(readyQ);

        // Increase wait time by how long it spent in queue
        g_processes[pid].wait_time += (current_time - g_processes[pid].last_ready_time);

        // Mark when it starts running
        g_processes[pid].start_run_time = current_time;

        // If CPU was idle from [*cpu_busy_until, current_time), track that
        if (current_time > *cpu_busy_until) {
            *cpu_idle_time += (current_time - *cpu_busy_until);
        }

        *running_pid = pid;
        long slice = (g_processes[pid].remaining_cpu > q) ? q : g_processes[pid].remaining_cpu;
        *running_end_time = current_time + slice;

        // CPU now busy until (current_time + slice)
        *cpu_busy_until = *running_end_time;

        VERBOSE_PRINT("%ld : Process %d is scheduled to run for time %ld\n",
                      current_time, g_processes[pid].id, slice);

        // Create an event
        Event e;
        if (slice == g_processes[pid].remaining_cpu) {
            e.type = EVT_CPU_COMPLETE;
        } else {
            e.type = EVT_CPU_TIMEOUT;
        }
        e.time = *running_end_time;
        e.pid  = pid;
        eventHeap_push(eventQ, e);
    } else {
        // No process => CPU stays idle
          }
}

/* =========================================================================
 * PRINTING FUNCTIONS
 * ========================================================================= */
void print_per_process_metrics(long current_time, int pid)
{
    // TAT = finish_time - arrival_time
    int tat = g_processes[pid].finish_time - g_processes[pid].arrival_time;
    // Running time = sum of CPU + IO
    int run_time = g_processes[pid].total_cpu_io;
    double perc = 100.0 * (double)tat / (double)run_time;
    int wtime = tat - run_time;

    printf("%ld : Process %d exits. Turnaround time = %d (%.0f%%), Wait time = %d\n",
           (long)current_time,
           g_processes[pid].id,
           tat,
           perc,
           wtime);
}

void print_aggregate_metrics(long simulation_end, long total_wait, long total_idle)
{
    double avg_wait = 0.0;
    if (g_numProcs > 0) {
        avg_wait = (double)total_wait / (double)g_numProcs;
    }

    // The "total turnaround time" is from 0 to simulation_end
    long total_tat = simulation_end;

    double utilization = 0.0;
    if (simulation_end > 0) {
        long busy_time = simulation_end - total_idle;
        utilization = 100.0 * ((double)busy_time / (double)simulation_end);
    }

    printf("Average wait time = %.2f\n", avg_wait);
    printf("Total turnaround time = %ld\n", total_tat);
    printf("CPU idle time = %ld\n", total_idle);
    printf("CPU utilization = %.2f%%\n", utilization);
}

/* =========================================================================
 * READY QUEUE IMPLEMENTATION (FIFO)
 * ========================================================================= */
void RQ_init(ReadyQueue *q, int capacity)
{
    q->data = (int *)malloc(sizeof(int) * capacity);
    q->capacity = capacity;
    q->front = 0;
    q->rear  = -1;
    q->count = 0;
}

void RQ_destroy(ReadyQueue *q)
{
    if (q->data) free(q->data);
    q->data = NULL;
}

int RQ_isEmpty(const ReadyQueue *q)
{
    return (q->count == 0);
}

/* 
   Enqueue a process ID and record the time it joined the queue 
*/
void RQ_enqueue(ReadyQueue *q, int pid, int current_time)
{
    if (q->count == q->capacity) {
        fprintf(stderr, "ReadyQueue overflow!\n");
        return;
    }
    q->rear = (q->rear + 1) % q->capacity;
    q->data[q->rear] = pid;
    q->count++;

    // Mark the time process joined the queue
    g_processes[pid].last_ready_time = current_time;
}

int RQ_dequeue(ReadyQueue *q)
{
    if (RQ_isEmpty(q)) {
        return -1;
    }
    int pid = q->data[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->count--;
    return pid;
}

/* =========================================================================
 * EVENT MIN-HEAP IMPLEMENTATION
 * ========================================================================= */
void eventHeap_init(EventMinHeap *mh, int capacity)
{
    mh->heapArray = (Event *)malloc(sizeof(Event) * capacity);
    mh->capacity  = capacity;
    mh->size      = 0;
}

int eventHeap_empty(const EventMinHeap *mh)
{
    return (mh->size == 0);
}

static void swapEvents(Event *a, Event *b)
{
    Event tmp = *a;
    *a = *b;
    *b = tmp;
}

/* 
   Compare events by:
   1) time
   2) type: ARRIVE(0) < CPU_COMPLETE(1) < CPU_TIMEOUT(2)
   3) external PID
*/
int compareEvents(const Event *a, const Event *b)
{
    if (a->time < b->time) return -1;
    if (a->time > b->time) return  1;

    if (a->type < b->type) return -1;
    if (a->type > b->type) return  1;

    int idA = g_processes[a->pid].id;
    int idB = g_processes[b->pid].id;
    if (idA < idB) return -1;
    if (idA > idB) return  1;
    return 0;
}

static void bubbleUp(EventMinHeap *mh, int idx)
{
    while (idx > 0) {
        int parent = (idx - 1) / 2;
        if (compareEvents(&mh->heapArray[idx], &mh->heapArray[parent]) < 0) {
            swapEvents(&mh->heapArray[idx], &mh->heapArray[parent]);
            idx = parent;
        } else {
            break;
        }
    }
}

static void bubbleDown(EventMinHeap *mh, int idx)
{
    while (1) {
        int left  = 2*idx + 1;
        int right = 2*idx + 2;
        int smallest = idx;

        if (left < mh->size &&
            compareEvents(&mh->heapArray[left], &mh->heapArray[smallest]) < 0) {
            smallest = left;
        }
        if (right < mh->size &&
            compareEvents(&mh->heapArray[right], &mh->heapArray[smallest]) < 0) {
            smallest = right;
        }
        if (smallest != idx) {
            swapEvents(&mh->heapArray[idx], &mh->heapArray[smallest]);
            idx = smallest;
        } else {
            break;
        }
    }
}

void eventHeap_push(EventMinHeap *mh, Event ev)
{
    if (mh->size >= mh->capacity) {
        fprintf(stderr, "EventHeap overflow!\n");
        return;
    }
    mh->heapArray[mh->size] = ev;
    bubbleUp(mh, mh->size);
    mh->size++;
}

Event eventHeap_pop(EventMinHeap *mh)
{
    if (mh->size == 0) {
        Event dummy;
        dummy.time = INT_MAX;
        dummy.type = EVT_ARRIVE;
        dummy.pid  = -1;
        return dummy;
    }
    Event top = mh->heapArray[0];
    mh->size--;
    if (mh->size > 0) {
        mh->heapArray[0] = mh->heapArray[mh->size];
        bubbleDown(mh, 0);
    }
    return top;
}
