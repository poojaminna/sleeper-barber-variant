#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define PROGRAMMING_PERIOD 2000
#define TUTORING_PERIOD 200

//student stucture to store details about each student
struct student_struct {
  int helps_taken;                         //helps taken so far
  int tutor;                               //tutor assigned
  sem_t student_waitFor_tutor_toPickHim;   //semaphore for every student
};

// Node structure for implementation of queue
struct node {
  int id;            // id of student in line
  struct node *next; // next in line
};

int students;          //total students
int tutors;            //total tutors
int chairs;            //total chairs
int help;              //total helps each student can receive
int free_chairs;       // number of free chairs
int count_waiting;     // number of students waiting
int currently_tutored; // number of students being tutored
int count_sessions;    // number of sessions given by all tutors combined
int count_requests_taken; // number of requests accepted by the coordinator
int students_left;        // students left to get tutored

struct student_struct *all_students; //This structure stores all students
struct node *queue_to_store_order_of_entering_customers =
    NULL; // stores incoming order
struct node *queue_to_store_priority_of_waiting_customers =
    NULL; // stores priority order


//Locks needed
pthread_mutex_t lockInWaitingArea;          // mutex lock for global variables in waiting area
pthread_mutex_t lockInTutoringArea;         // mutex lock for gloabl variables in tutoring area;
pthread_mutex_t lockInCoordinatorDesk;      // mutex lock for global variables at coordinaor desk;

//semaphores needed
sem_t coordinator_waitFor_student_toArrive;
sem_t tutor_waitFor_coordinator_toIntimate;

// Enqueue operation from the following link
// https://www.log2base2.com/data-structures/queue/queue-using-linked-list.html
struct node *rear = NULL;
struct node *front = NULL;
//Student thread uses this function to push itself to a waiting queue
void push(int id) {
  struct node *newNode = malloc(sizeof(struct node));
  newNode->id = id;
  newNode->next = NULL;

  // if it is the first node
  if (front == NULL && rear == NULL)
    // make both front and rear points to the new node
    front = rear = queue_to_store_order_of_entering_customers = newNode;
  else {
    // add newnode in rear->next
    rear->next = newNode;

    // make the new node as the rear node
    rear = newNode;
  }
}

// Dequeue operation from the following link
// https://www.log2base2.com/data-structures/queue/queue-using-linked-list.html
//Coordinator thread uses this function to pop student from waiting queue
int pop() {
  // used to free the first node after dequeue
  struct node *temp;
  if (front == NULL)
    return -1;
  else {
    // take backup
    temp = front;
    int ans = temp->id;

    // make the front node points to the next node
    // logically removing the front element
    front = front->next;

    // if front == NULL, set rear = NULL
    if (front == NULL)
      rear = NULL;

    // free the first node
    free(temp);
    return ans;
  }
}


struct node *rear1 = NULL;
struct node *front1 = NULL;
//Coordinator thread uses this function to push student into priority queue
void push_with_priority(int id) {
  struct node *newNode = malloc(sizeof(struct node));
  newNode->id = id;
  newNode->next = NULL;
  // if queue is empty
  front1 = queue_to_store_priority_of_waiting_customers;
  if (front1 == NULL && rear1 == NULL) {
    front1 = rear1 = queue_to_store_priority_of_waiting_customers = newNode;
  }
  // if queue is not empty
  else {
    // if current student has greater priority then add at the head
    if (all_students[id].helps_taken < all_students[front1->id].helps_taken) {
      // printf("Here2\n");
      newNode->next = front1;
      front1 = queue_to_store_priority_of_waiting_customers = newNode;
    }
    // add current student at the right place
    else {
      int added = 0; //student initally not added
      int key = all_students[id].helps_taken;
      // place this key in the right place
      while (front1 != NULL && front1->next != NULL) {
        int current = all_students[front1->next->id].helps_taken;
        if (current > key) {
          newNode->next = front1->next;
          front1->next = newNode;
          added = 1;
          break;
        }
        front1 = front1->next;
      }
      //if student has not been added yet then add at the end
      if (front1 == NULL || added == 0) {
        front1->next = newNode;
        front1 = newNode;
        added = 1;
      }
    }
  }
}

//Tutor thread uses this function to pop student from priority queue
int pop_from_priorityQ() {
  // used to free the first node after dequeue
  struct node *temp;
  front1 = queue_to_store_priority_of_waiting_customers;
  if (front1 == NULL)
    return -1;
  else {
    // take backup
    temp = front1;
    int ans = temp->id;

    // make the front node points to the next node
    // logically removing the front element
    front1 = front1->next;
    queue_to_store_priority_of_waiting_customers = front1;
    // if front == NULL, set rear = NULL
    if (front1 == NULL)
      rear1 = NULL;

    // free the first node
    free(temp);
    return ans;
  }
}

//Function for tutor thread
void *tutor(void *arg) {
  int tut_id = *((int *)arg);
  // printf("Hi Im tutor %d\n ", tut_id);
  while (1) {
    // if all students are done, then exit
    pthread_mutex_lock(&lockInCoordinatorDesk);
    if (students_left == 0) {
      pthread_mutex_unlock(&lockInCoordinatorDesk);
      break;
    }
    pthread_mutex_unlock(&lockInCoordinatorDesk);

    // wait until coordinator notifies of a waiting student
    sem_wait(&tutor_waitFor_coordinator_toIntimate);

    // pick the student from queue
    pthread_mutex_lock(&lockInWaitingArea);

    int stu_id = pop_from_priorityQ();
    if (stu_id == -1) {
      pthread_mutex_unlock(&lockInWaitingArea);
      continue;
    }

    // update global variables
    count_waiting--;
    free_chairs++;
    pthread_mutex_unlock(&lockInWaitingArea);

    // wake up student for tutoring
    // sem_post(&all_students[stu_id].student_waitFor_tutor_toPickHim);

    // before getting tutored
    pthread_mutex_lock(&lockInTutoringArea);
    currently_tutored++;
    count_sessions++;
    pthread_mutex_unlock(&lockInTutoringArea);

    // get tutored
    usleep(TUTORING_PERIOD);

    // afer getting tutored
    all_students[stu_id].tutor = tut_id;
    all_students[stu_id].helps_taken++;

    printf("T: Student %d tutored by Tutor %d. Students tutored now = %d. "
           "Total sessions tutored = %d.\n",
           stu_id, tut_id, currently_tutored, count_sessions);
    // wake up student after tutoring
    sem_post(&all_students[stu_id].student_waitFor_tutor_toPickHim);

    //update global variables
    pthread_mutex_lock(&lockInTutoringArea);
    currently_tutored--;
    pthread_mutex_unlock(&lockInTutoringArea);
  }
  return NULL;
}

//Function for Coordinator thread
void *coordinator(void *arg) {

  while (1) {
    // if all students are done, then wake up all tutor threads
    pthread_mutex_lock(&lockInCoordinatorDesk);
    if (students_left == 0) {
      pthread_mutex_unlock(&lockInCoordinatorDesk);
      int i;
      for (i = 0; i < tutors; i++) {
        sem_post(&tutor_waitFor_coordinator_toIntimate);
      }
      break;
    }
    pthread_mutex_unlock(&lockInCoordinatorDesk);

    // wait for student to arrive for help
    sem_wait(&coordinator_waitFor_student_toArrive);

    // arrange students according to priority
    pthread_mutex_lock(&lockInWaitingArea);

    // continue if no students waiting for help
    if (!queue_to_store_order_of_entering_customers) {
      pthread_mutex_unlock(&lockInWaitingArea);
      continue;
    }

    int stu = pop();
    while (stu != -1) {
      push_with_priority(stu);

      // update global variables
      count_requests_taken++;
      count_waiting++;

      printf("C: Student %d with priority %d added to the queue. Waiting "
             "students now = %d. Total requests = %d.\n",
             stu, help - all_students[stu].helps_taken, count_waiting,
             count_requests_taken);

      // inform coordinator that a student has been added
      sem_post(&tutor_waitFor_coordinator_toIntimate);
      stu = pop();
    }
    pthread_mutex_unlock(&lockInWaitingArea);
  }

  return NULL;
}

//Function for Student thread
void *student(void *arg) {

  // student id
  int stu_id = *((int *)arg);

  // per student help local variable to each thread
  int stu_help = help;

  // seed the random function
  time_t t;
  srand((unsigned)time(&t));

  int programming_period;
  while (stu_help != 0) {
    // get random programming period between 0 to 2ms
    programming_period = rand() % (PROGRAMMING_PERIOD + 1);

    // initially every student is programming
    usleep(programming_period);

    // check if seats are available
    pthread_mutex_lock(&lockInWaitingArea);
    if (free_chairs == 0) {
      printf("S: Student %d found no empty chair. Will try again later.\n",
             stu_id);
      pthread_mutex_unlock(&lockInWaitingArea);
      continue; // go back to programming
    }

    // take a seat
    free_chairs--;
    push(stu_id); // Stores order of entering

    pthread_mutex_unlock(&lockInWaitingArea);
    // printf("Hi Im student %d down\n", stu_id);

    // wake up coordinator
    sem_post(&coordinator_waitFor_student_toArrive);

    // wait until student gets tutored
    sem_wait(&all_students[stu_id].student_waitFor_tutor_toPickHim);

    // after getting tutored
    printf("S: Student %d receive help from Tutor %d.\n", stu_id,
           all_students[stu_id].tutor);

    stu_help--;
  }

  // if all helps are completed then number of students should be decremented
  pthread_mutex_lock(&lockInCoordinatorDesk);
  students_left--;
  pthread_mutex_unlock(&lockInCoordinatorDesk);

  return NULL;
}

//Main function
int main(int argc, char *argv[]) {
  if (argc != 5) {
    printf("Error: There should be exactly 4 arguments");
    return -1;
  } else {
    students = atoi(argv[1]);
    tutors = atoi(argv[2]);
    chairs = atoi(argv[3]);
    help = atoi(argv[4]);


    //Update global variables
    free_chairs = chairs;
    students_left = students;

    // Initializing locks
    pthread_mutex_init(&lockInCoordinatorDesk, NULL);
    pthread_mutex_init(&lockInWaitingArea, NULL);
    pthread_mutex_init(&lockInTutoringArea, NULL);

    // Initializing semaphores
    sem_init(&coordinator_waitFor_student_toArrive, 0, 0);
    sem_init(&tutor_waitFor_coordinator_toIntimate, 0, 0);

    //student threads
    int *student_ids = malloc(sizeof(int) * students);
    pthread_t *pths;
    pths = (pthread_t *)malloc(sizeof(pthread_t) * students);
    all_students = malloc(sizeof(struct student_struct) * students);
    int i;
    for (i = 0; i < students; i++) {
      student_ids[i] = i;
      sem_init(&all_students[i].student_waitFor_tutor_toPickHim, 0, 0);
      pthread_create(&pths[i], NULL, student, (void *)&student_ids[i]);
    }

    // tutor threads
    int *tutor_ids = malloc(sizeof(int) * tutors);
    pthread_t *ptht;
    ptht = (pthread_t *)malloc(sizeof(pthread_t) * tutors);
    for (i = 0; i < tutors; i++) {
      tutor_ids[i] = i;
      pthread_create(&ptht[i], NULL, tutor, (void *)&tutor_ids[i]);
    }

    // coordinator thread
    pthread_t coordinator_thread;
    pthread_create(&coordinator_thread, NULL, coordinator, NULL);

    //wait for all threads to be completed
    pthread_join(coordinator_thread, &value);
  for(i = 0; i < students; i++) {
    pthread_join(pths[i], &value);
  }
  for(i = 0; i < tutors; i++) {
    pthread_join(ptht[i], &value);
  }
    return 0;
  }
}