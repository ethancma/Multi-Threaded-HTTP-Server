## Multi-Threaded HTTP Server

Ethan Ma - Spring 2022

### Basic Overview

###### Thread Handle
- This function was to help modularize my handle_connection function, otherwise
  I could have added mutex locks and handled each connection within main.
- The point of this function is to dequeue the next connfd to be processed,
  process it, then wait for the next task.
- The conditional variables are used to check whether the queue is empty
  as to decide whether or not work needs to be done or not.
- The mutex locks are to ensure no other threads are in the critical region of
  dequeueing and processing connfd's.

###### Handle Connection
- In this step parse all headers and requests and assign the various fields
- use **regex**  to pattern match headers, and **sscanf** to get arguments
- make sure that response includes the following or else it is a bad request
    - method
    - uri
    - version
- once valid request, check methods for one of the three, GET PUT APPEND
    - else not implemented error
- if PUT APPEND, parse content-length, bad request if missing

###### Get Handle
- open uri with O_RDONLY flag
- check if directory or no permissions to read, then forbidden
- send 200 OK, and read file and send back to connfd
    - if error occurs while reading, send internal server error
- if file not found, then file not found, 404

###### Put Handle
- if file does not exist
    - open using O_CREAT | O_WRONLY | O_TRUNC, 0622
    - send 201 CREATED
    - read from socket and write into created file
        - if any reading errors, send internal server error
- file exists
    - check if directory or no access, then 403 Forbidden
    - else send 200 OK since no need to create file
    - open file using O_WRONLY | O_TRUNC, 0622
    - read from socket and write into existing file
        - if any reading errors, send internal server error
###### Append Handle
- if file does not exist, send 404 Not Found
- same process as append but open using O_APPEND
- send 200 OK
- read from socket and append to existing file
    - if any reading errors, send internal server error

### Data Structures
1. Linked List
    - I used a linked list in order to hold a worker queue to keep track of all
      connfd's that needed to be handled. The linked list operated as a queue
      and in my thread handler, I dequeue the next connfd and processed it.

### Maintaining Thread Safety

##### Shared Variables
1. Worker queue (`queue`)
    - this helps to keep track of all work to be done
    - all connfd's are enqueue'd in main, and dequeue'd in my thread_handler
      where both operations are protected by locks since they are critical
      sections
2. Mutex locks (`lock, readerwriter_lock`)
    - This helps to prevent multiple threads from being in the critical section
      at once. Other threads will not be able to access such region until it has
      been unlocked by another thread.
    - I implemented a `readerwriter_lock` in this assignment to restrict writing
      and reading to files to one thread at a time, since in this assignment
      that is how we will maintain atomicity.
3. Conditional Variable (`cond, reading, writing`)
    - The `cond` variable will either be waiting, or be signaled to go ahead and
      process the connection. I utilize this when the queue is empty, `cond` will
      wait since there is nothing to process. This is put into a while loop, and
      as soon as `cond` has been signaled, the thread handler has the go ahead
      to process the connfd at the head of the queue.
    - The `reading` variable is to signify when there is a thread that is
      currently reading to a file, since we will only read when there is no one
      writing. So we wait on a read until it has been signaled by a writing
      process.
    - The `writing` variable is to signify when there is a thread that is
      currently writing to a file, since we only one one file being to written
      to at a time. For example, PUT and APPEND operations will be using this to
      make sure one thread does not overwrite another.
4. Reader/Writer Count
    - These two variables act as a semaphore, which help to control the new
      conditional variables `reading, writing` that I introduce in this version
      of the assignment.
    - If there are readers, then the `writing` cond var will wait until there
      are no more readers.
    - If there are writers, then you increment, and once the writer finishes,
      you decrement to ensure there is only one writer at a time.

##### Critical Sections
1. Enqueueing
    - Done in `main`
    - We do not want multiple threads to be enqueing the same connfd, or
      affecting the process of adding work to the worker queue, so this is
      protected by the lock.
2. Dequeing
    - Done in `thread_handler`
    - Similar to the enqueing, we do not want other threads to be able to access
      this critical region when processing connfd's. So this area is locked
      until the thread has finished its task.
3. Incrementing/Decrementing Reader Count
    - Done in `handle_connection`
    - This is a critical section since we do not want multiple processes
      decrementing readers at a time
4. Writing to a file
    - Done in `handle_connection`
    - This is a rather larger critical section, but I am not sure how to go
      arround this. This encompasses each PUT and APPEND operation along with
      the writer incrementing/decrementing to ensure having only one writer at a
      time.
5. Others
    - Do note that this code is not perfect, and there are critical sections
      that I may or may not have handled yet, these are the only sections that I
      have dealt with in this version.
    

### Efficiency
- I use a threadpool to make sure there are not infinite amounts of threads
  created, per a youtube video by Jacob Sorber, which guided me
  throughout the multithreading process of this assignment.
- at all points throughout this assignment, I make sure to store and use **buffers**
  this will reduce system calls and time needed to go into disk to retrieve data
  by using buffer, I will be reading in blocks of data at a time, namely 2048
- I will be storing the bytes read in from either a file descriptor or socket
  descriptor and be calling writes or sends with the number of bytes read in for
  some buffer
- I have also tried to modularize my code where I seem fit, by separating
  get/put/append handlers and another function where I will be parsing requests
    - this can still always be improved by for example, created a struct for
      error messages, defining macros, or just creating a function to send
      specific messages when errors occur
