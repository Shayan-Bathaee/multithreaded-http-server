# CSE130 asgn4: Multithreaded HTTP Server | Shayan Bathaee | 5/30/2022
## Files
**httpserver_asgn2.c:** Contains the code for initializing the server, threads, and handling the connection between the server and the client. The functions defined in this file are `main`, `handle_connection`, and `thread_func`. 

The function `main` starts by checking the calling arguments (logfile, threads, and port number). If everything is okay, it creates a queue to store connection file descriptors, and an array of threads. After this, it runs a continuous loop that accepts connections from clients, and passes those connections into a work queue to be handled by threads. When altering the work queue, a lock is used to ensure no threads are editing the queue at the same time. Once a new connection is added to the queue, the main function calls `pthread_cond_signal` to wake up a thread and handle the connection. 

The function `thread_func` contains a continuous while loop. Inside of this loop, it dequeues from the work queue and calls `handle_connection` on the file descriptor. If the queue is empty, this function waits on the condition variable signal from `main`. Once it gets the signal, it dequeues again and uses the valid file descriptor. 

The function `handle_connection` takes a valid connection file descriptor and handles any request the client sends over that connection. It does this by continously reading from the client until the end of the request header (`\r\n\r\n`) is found. Once `handle_connection` sees the end of the request header, it calls functions in format.c to parse the request and execute the proper HTTP method functionality. If there is a message body in the request, the functions in format.c will continue to read from the connection. After completing the request, the function logs the request method, uri, status code, and thread ID to the specified logfile. Lastly, it frees the linked list and request data.

**format.c:** This file manages parsing through requests, and implements the `PUT`, `GET`, and `APPEND` functions.

*Parsing:*
- `parse_header`: Takes in up to 2048 bytes of an HTTP request. Begins processing the request by calling `parse_request_line`. If there are header fields in the request, `get_header_fields` is also called. While parsing, this function fills a `request` struct variable with relevant information about the request.
- `parse_request_line`: Takes in up to the first `\r\n` from an HTTP request, and updates a `request` struct variable with information about the client's request. Uses `strtok_r` to get the method and URI.
- `get_header_fields`: This function parses through each header field, creates a linked list node, and puts the key-value pair into the node.

*HTTP Methods:*
- `GET`: Tries to open the client's specified URI for reading. If it was able to open the file, it reads the contents of the file and writes that content to the client over the communication link. If it couldn't open the file, it returns the relevant error code. 
- `PUT`: Tries to open the client's specified URI for writing. If it was able to open the file, it replaces its contents with the client's message. If the file did not exist, it creates the file and then writes the client's message to it. If any of these processes failed, it returns the relevant error code. 
- `APPEND`: Tries to open the client's specified URI for appending. If it was able to open the file, it appends the client's message to the file. Otherwise, it returns the relevant error code. 

*IO:*
- `ReadIn`: Reads up to 2048 bytes from a given file descriptor into a given buffer.
- `WriteOut`: Writes a specified number of bytes from a buffer into a file descriptor.

*Helper/MISC:*
- `find_end`: Takes in character buffer (http request). Returns true if there is a `\r\n\r\n` in the http request, and false if not.

**format.h:** Contains the function declarations for all of the functions in format.c. Also contains the struct definition for `request`.

**datastruct.c** Contains all of the functions needed for the data structures (linked list and queue).

*Linked List:*
- `make_root`: Creates the root node of a linked list and returns a pointer to it.
- `insert_node`: Inserts a node into a linked list and returns a pointer to the node.
- `print_linked_list`: Prints the key, value pairs of each node in the linked list.
- `free_linked_list`: Iterates through each node in the linked list and frees the allocated memory.
- `get_id`: Iterates through the linked list and returns a pointer to the request ID.

*Queue:*
- `queue_init`: Initializes a queue. Sets the queue length to 0, the queue head to NULL, and the queue tail to NULL.
- `enqueue`: Inserts a qnode to the back of the queue, and returns that qnode. If the head is NULL, it sets the head and tail to the newly inserted qnode and sets the length to 1. Otherwise, it sets the tail to the newly inserted qnode, and increments the length. 
- `dequeue`: Removes the head qnode of the queue, and returns that qnode's connfd. Decrements the length and frees the removed qnode. If the queue is empty, it returns -1.
- `print_queue`: Prints the connfd of every qnode in the queue.
- `free_queue`: Checks if the queue exists. If it does, this function frees every qnode in the queue, and then frees the queue itself.

**datastruct.h** Contains the function declarations for all of the functions in datastruct.c. Also contains the struct defintions for `node`, `queue`, and `qnode`.

**Makefile:** This file compiles the code in httpserver_asgn2.c, format.c, format.h, datastruct.c, and datastruct.h. After running a `make`, `make all`, or `make httpserver`, this file will create an executable called `httpserver` which can be used to run the program.

## Usage and Program Description
To create a server, type `./httpserver [-t threads] [-l logfile] <port>` into the terminal. It is recommended to use a port number above 2000. After doing so, a client can write a request in another terminal by writing the command `printf "<HTTP REQUEST>" | nc -N localhost <port>`. The port number must be the same in both commands for the program to work. 

HTTP requests are formatted as follows: `<method> /<file> HTTP/1.1\r\nContent-Length: <content-length>\r\nRequest-Id: <request-id>\r\n\r\n<message-body>`. GET requests do not need a content length or message body. The Request ID field can be ommitted, with the default being 0. The content length must match the length of the message body.

After running this command, the program will perform the client's specified HTTP request. If the program encounters an error, the user will see a message from the server describing what error occurred. The server can perform the following operations: 

- GET: Grabs the contents of a specified URI from the server and prints those contents to the client's console. 
- PUT: Puts the client's message-body into a specified path on the server. If the path does not exist, it will create the file and place the client's message-body inside.
- APPEND: Appends the client's message-body into a specified path on the server. 

After a request has been executed, the logfile will update to include a line describing the method, URI, status code, and request ID. This server can execute multiple requests at the same time. To do so, simply open another connection with the server via a different client, and write a request.

## Implementation

### HTTP Server
The `main` function first checks that upon executing the program, the user has provided the correct calling arguments (threads, logfile, and port number). If everything is valid, the stater code creates a work queue and a thread pool, and waits for client connections. Once connections are aquired, a worker thread gets woken up to handle the connection.

My `handle_connection` function continuously reads from a client's connection file descriptor until it finds the end of the request header (`\r\n\r\n`). Once the end of the request header is found, `parse_header` is called to obtain information about the request. After all the necessary information is obtained, `handle_connection` calls either `GET`, `PUT`, or `APPEND` to execute the client's requested operation. If the server encounters any errors, it outputs the relevent error response to the client.

Parsing through the request done through the use of `sscanf` and `strtok_r`, along with a bit of hard-coding to index the buffer. `strtok_r` is used in my code to get the method and URI, and `sscanf` is used throughout my code to get anything up to a specified set of characters. My `parse_header` function parses through the http request and calls `parse_request_line`. If `parse_header` sees that there are header fields in the request, it will also call `get_header_fields`.

Response headers that are sent to the client are constructed using `dprintf`, which allows for formatted strings to be output over a given file descriptor. To get the length of the message body for `GET` requests, the `stat` function is used to obtain the length of the file.

With `PUT` and `APPEND` requests, data needs to be read from the client and written into a file. Upon my first reads in `handle_connection`, there is a possibility that I read the start of the message body. Because of this, the first call to `WriteOut` will write what was already read from the client into the file. After that call, a local buffer is created within the `PUT` or `APPEND` function, which is used to read in more bytes from the client and write them into the file. The `GET` function simply calls `ReadIn` on the requested file and `WriteOut` to send the file contents to the client.

Opening files has some unique aspects to it as well. With `PUT`, the first call to `open` has write only and truncate flags. If it couldn't open the file, the `open` function is called again with write only and create flags. If creating the file doesn't work, it notifies the client that there was an internal server error. With `APPEND`, the file is opened with write only and append flags. If it could not be opened, it sends the relevant error response to the client. With `GET`, the file is opened with a read only flag. If it could not be opened, it sends the relevant error response to the client.

One important aspect of the `PUT` and `APPEND` functions is that the response header must be sent at the very end of the function. The reason for this is because the sever will need to try writing the message body to the file to make sure everything worked smoothly. If the sever has some sort of problem writing the client's message body to the file, the response header needs to say internal server error. With `GET`, this isn't possible, because the client needs to see the response header before seeing the message body they requested. 

### Logging
To execute logging, we have a `LOG(...)` macro defined, which is just a call to `fprintf` with the file input being the static `logfile`. Immediately after the `LOG` function is called, `fflush` flushes the buffer to the logfile. Logging is done just before returning in the `handle_connection` function because the entire request needs to be processed for an accurate status code. The log function takes in a formatted string containing the method, uri, status code, and request id. The request ID is found by calling the `get_id` function, which iterates through the nodes in the linked list to find the ID. If the ID is found, it returns it, and if not it returns 0 (the default request ID). 

### Multithreading
From a high level standpoint, my implementation consists of creating a thread pool and assigning a thread function (`thread_func`) to those threads that calls `handle_connection`. The first thing that I do to implement multithreading is initialize the work queue in `main` to contain connections awaiting processing.. This is done by simply allocating memory for the queue, and calling the `queue_init` function. 

After this, I create a thread pool. Given that I know the number of threads from the calling arguments, I am able to create a `pthread_t` array that is of size `threads`. The threads are created using the `pthread_create` function, with the start routine being `thread_func`. 

Once the threads are created, the next step is to get connections and add them to the work queue. If a connection is accepted, `pthread_mutex_lock` is called to ensure no threads are accessing the work queue. After this, the new connection is enqueued, and a thread is signaled using `pthread_cond_signal`. Finally, the lock gets released and the threads can dequeue the connection.

The function `thread_func` needs to dequeue from the work queue to get a connection file descriptor. To do this, it grabs the lock and tries to dequeue. If the queue is empty, the thread calls `pthread_cond_wait` to wait on the condition variable signal from `main`. Once this signal is received, the thread tries to dequeue again. If it dequeues a valid file descriptor, it calls `handle_connection`, closes the connection, and continues trying to get connections from the work queue.

### Atomicity and Coherency
My implementation for atomicity and coherency between threads relies heavily on the `flock` function. This function provides a file lock, which ensures that when one thread has the lock, other threads cannot read or write to it. For `GET` requeusts, the file lock operation used is `LOCK_SH` (shared lock). Using a shared lock for `GET` requests allows multiple threads to read from a file at the same time. For `PUT` and `APPEND` requests, the lock operation used is `LOCK_EX` (exclusive lock). Using an exclusive lock for `PUT` and `APPEND` restricts access so no other threads can access the file while the thread with the lock is writing to it. Just before closing the file, the thread holding a lock will call `flock` again with the operation `LOCK_UN` to unlock the file.
