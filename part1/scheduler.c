#define _XOPEN_SOURCE
#define _XOPEN_SOURCE_EXTENDED

#include "scheduler.h"

#include <assert.h>
#include <curses.h>
#include <ucontext.h>
#include <unistd.h>

#include "util.h"

// This is an upper limit on the number of tasks we can create.
#define MAX_TASKS 128

// This is the size of each task's stack memory
#define STACK_SIZE 65536

// This struct will hold the all the necessary information for each task
typedef struct task_info {
  // This field stores all the state required to switch back to this task
  ucontext_t context;
  
  // This field stores another context. This one is only used when the task
  // is exiting.
  ucontext_t exit_context;
  
  // TODO: Add fields here so you can:
  //   a. Keep track of this task's state.
  //   b. If the task is sleeping, when should it wake up?
  //   c. If the task is waiting for another task, which task is it waiting for?
  //   d. Was the task blocked waiting for user input? Once you successfully
  //      read input, you will need to save it here so it can be returned.

  // true if the task is complete
  bool task_complete;

  // true if the task is being waited for
  bool must_complete;

  // the task is not allowed to execute until AT LEAST this time (in millis)
  size_t wake_time;

} task_info_t;

int current_task = 0; //< The handle of the currently-executing task
int num_tasks = 0;    //< The number of tasks created so far
task_info_t tasks[MAX_TASKS]; //< Information for every task

ucontext_t sched; // the scheduler's context
ucontext_t done_context; // a variable to temporarily store the context of a completed task

/**
 * Initialize the scheduler. Programs should call this before calling any other
 * functiosn in this file.
 */
void scheduler_init() {
  // TODO: Initialize the state of the scheduler 
  
}


/**
 * This function will execute when a task's function returns. This allows you
 * to update scheduler states and start another task. This function is run
 * because of how the contexts are set up in the task_create function.
 */
void task_exit() {
  //printf("task_exit()\n");

  // mark this task as complete and pick up where we left off
  tasks[current_task].task_complete = true;
  swapcontext(&done_context, &sched);
}

/**
 * Create a new task and add it to the scheduler.
 *
 * \param handle  The handle for this task will be written to this location.
 * \param fn      The new task will run this function.
 */
void task_create(task_t* handle, task_fn_t fn) {
  // Quick check to determine if we have reached the task limit
  if (num_tasks == MAX_TASKS) {
    //printf("Task limit reached! The program will exit.\n");
    exit(-1);
  }

  // Claim an index for the new task
  int index = num_tasks;
  num_tasks++;
  
  // Set the task handle to this index, since task_t is just an int
  *handle = index;

  // initial state
  tasks[index].task_complete = false;
  tasks[index].must_complete = false;
  tasks[index].wake_time = 0;
 
  // We're going to make two contexts: one to run the task, and one that runs at the end of the task so we can clean up. Start with the second
  
  // First, duplicate the current context as a starting point
  getcontext(&tasks[index].exit_context);
  
  // Set up a stack for the exit context
  tasks[index].exit_context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].exit_context.uc_stack.ss_size = STACK_SIZE;
  
  // Set up a context to run when the task function returns. This should call task_exit.
  makecontext(&tasks[index].exit_context, task_exit, 0);
  
  // Now we start with the task's actual running context
  getcontext(&tasks[index].context);
  
  // Allocate a stack for the new task and add it to the context
  tasks[index].context.uc_stack.ss_sp = malloc(STACK_SIZE);
  tasks[index].context.uc_stack.ss_size = STACK_SIZE;
  
  // Now set the uc_link field, which sets things up so our task will go to the exit context when the task function finishes
  tasks[index].context.uc_link = &tasks[index].exit_context;
  
  // And finally, set up the context to execute the task function
  makecontext(&tasks[index].context, fn, 0);
}

/**
 * Wait for a task to finish. If the task has not yet finished, the scheduler should
 * suspend this task and wake it up later when the task specified by handle has exited.
 *
 * \param handle  This is the handle produced by task_create
 */
void task_wait(task_t handle) {
  // Block this task until the specified task has exited.

  current_task = (int)handle;
  tasks[current_task].must_complete = true;

  // check if the task is complete
  if (tasks[current_task].task_complete) {
    // the task is complete, no need to context switch or wait
    return;
  }

  // swap context to the task pointed to by handle
  swapcontext(&sched, &tasks[current_task].context);

  // the task is complete at this point

  //printf("task_wait has control again\n");

  // run other tasks being waited for until we are done
  int next_index = first_task_waiting();
  while (next_index != -1) {
    choose_new_task();
    swapcontext(&sched, &tasks[current_task].context);
    next_index = first_task_waiting();
  }

  //printf("task_wait says no more tasks waiting\n");
}

/**
 * The currently-executing task should sleep for a specified time. If that time is larger
 * than zero, the scheduler should suspend this task and run a different task until at least
 * ms milliseconds have elapsed.
 * 
 * \param ms  The number of milliseconds the task should sleep.
 */
void task_sleep(size_t ms) {
  // Block this task until the requested time has elapsed.

  // set the wake time on this task
  tasks[current_task].wake_time = time_ms() + ms;

  // remember this task's index
  int sleeping = current_task;

  // in the meantime, run a new task if one is ready
  // this call may simply block until this task is woken up if another task is not available
  choose_new_task();

  if (sleeping == current_task) {
    // we never changed tasks, no need for a context switch
    return;
  }

  swapcontext(&tasks[sleeping].context, &tasks[current_task].context);
}

/**
 * Read a character from user input. If no input is available, the task should
 * block until input becomes available. The scheduler should run a different
 * task while this task is blocked.
 *
 * \returns The read character code
 */
int task_readchar() {
  // TODO: Block this task until there is input available.
  // To check for input, call getch(). If it returns ERR, no input was available.
  // Otherwise, getch() will returns the character code that was read.
  return ERR;
}

// increment current task, go to zero if we go out of bounds
void next_task() {
  current_task++;
  if (current_task >= num_tasks) {
    current_task = 0;
  }
}

/** Finds a task that is ready for execution, sets current_task to it's index 
 * This may loop until a task is woken up
 */
void choose_new_task() {
  task_info_t task = tasks[current_task];

  // set current_task to the next task until we have a suitable task
  while (true) {
    next_task();
    task = tasks[current_task];

    if (task.wake_time <= time_ms() && !task.task_complete) {
      // we found our task
      break;
    }
  }
}

/** Return the index first uncompleted task in the array which is being waited for
 * Return -1 if no tasks are being waited for
 */
int first_task_waiting() {
  for (size_t i = 0; i < num_tasks; i++) {
    if (!tasks[i].task_complete && tasks[i].must_complete) {
      return (int)i;
    }
  }

  return -1;
}
