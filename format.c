#include "format.h"

pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;

// returns true if there is a \r\n\r\n in the http request
bool find_end(char *http_request) {
    int sequence = 0;
    for (int i = 0; i < HEADER_MAX_SIZE; i++) { // iterate through string to find \r\n\r\n
        uint8_t curr = http_request[i];
        if (curr == '\r') {
            if ((sequence == 0) || (sequence == 2)) {
                sequence += 1;
            }
        } else if (curr == '\n') {
            if ((sequence == 1) || (sequence == 3)) {
                sequence += 1;
            }
        } else {
            sequence = 0;
        }
        if (sequence == 4) {
            return true; // found the \r\n\r\n
        }
    }
    return false; // didn't find the \r\n\r\n
}

// parses through the header, gets data into request_data struct and stores request line in request_line
int parse_header(
    char *http_request, struct request *request_data, char *request_line, struct node **root) {
    char *http_request_temp = http_request;
    request_data->header_len = 0; // stores the length of the header including \r\n\r\n

    // get request line
    sscanf(http_request, "%[^\r\n]", request_line);
    int request_line_len = strlen(request_line);
    request_data->header_len += request_line_len; // up to the \r\n
    request_data->request_line_len = request_line_len;

    request_data->header_len += 2; // for the \r\n
    parse_request_line(request_line, request_data); // get URI and method

    // check if request line ends in '\r\n\r\n'
    if ((http_request[request_line_len + 2] == '\r')
        && (http_request[request_line_len + 3] == '\n')) {
        return 200; // there are no headers. we can return
    } else { // we need to get headers
        http_request_temp
            += (request_line_len + 2); // chop off "Request-Line\r\n" from http_request
        get_header_fields(http_request_temp, request_data, root);
        return 200;
    }
}

// parse the request line
int parse_request_line(char *request_line, struct request *request_data) {
    // get method and URI
    char delim[] = " ";
    char request_line_cpy[HEADER_MAX_SIZE + 1];
    strcpy(request_line_cpy, request_line);

    char *save_ptr = request_line;
    request_data->method = strtok_r(save_ptr, delim, &save_ptr);
    request_data->uri = strtok_r(save_ptr, delim, &save_ptr);
    request_data->uri
        = request_data->uri + 1; // cut off the slash so we can properly access the path

    return 200;
}

// puts header fields into a linked list
int get_header_fields(char *fields, struct request *request_data, struct node **root_ptr) {
    request_data->content_len = -1;

    // first, check if there are no headers
    if ((fields[0] == '\r') && (fields[1] == '\n')) {
        return 200;
    }

    for (int i = 0; i < HEADER_MAX_SIZE; i++) {
        // get and check key
        sscanf(fields, "%[^:]", (*root_ptr)->key); // get up to ':' into root->key
        int key_len = strlen((*root_ptr)->key);
        request_data->header_len += key_len;

        request_data->header_len += 2; // for ": "
        fields += (key_len + 2); // chop off "header-field: " from fields

        // get and check value
        sscanf(fields, "%[^\r]", (*root_ptr)->val); // put val into node
        int val_len = strlen((*root_ptr)->val);
        request_data->header_len += val_len;
        fields += val_len; // chop off the value too
        if (strcmp((*root_ptr)->key, CONTENT_LENGTH) == 0) {
            request_data->content_len = atoi((*root_ptr)->val);
        }

        // check end of header field
        if ((fields[0] == '\r') && (fields[1] == '\n')) {
            request_data->header_len += 2;
            fields += 2;
            if ((fields[0] == '\r') && (fields[1] == '\n')) { // done with our headers
                request_data->header_len += 2;
                return 200; // finished headers
            }
            *root_ptr = insert_node(root_ptr); // create a new node for next header field
        }
    }
    return 200; // this really shouldn't happen ever
}

int GET(struct request *request_data, int connfd) {
    int FD = open(request_data->uri, O_RDONLY); // try to open file for reading
    if (FD == -1) {
        if (errno == ENOENT) {
            dprintf(connfd, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n");
            return 404;
        } else {
            dprintf(connfd,
                "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal "
                "Server Error\n");
            return 500;
        }
    }

    // lock the file (shared)
    flock(FD, LOCK_SH);

    // write response header
    struct stat st;
    stat(request_data->uri, &st);
    dprintf(connfd, "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", st.st_size);

    // write message body
    int bytes = 0;
    uint8_t localbuff[BLOCK_SIZE];
    do {
        bytes = ReadIn(FD, localbuff); // read block of message_body
        WriteOut(connfd, localbuff, bytes); // write that block
    } while (bytes != 0);

    // close and return
    close(FD);
    return 200;
}

int PUT(struct request *request_data, int connfd, char *http_request) {
    pthread_mutex_lock(&file_lock); // grab lock to open file
    int FD = open(request_data->uri, O_WRONLY | O_TRUNC); // try to open the file
    int status_code = 200;
    if (FD == -1) { // if we couldn't open it
        FD = open(request_data->uri, O_WRONLY | O_CREAT, 0644); // try to open again with create
        status_code = 201;
        if (FD == -1) {
            dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                            "22\r\n\r\nInternal Server Error\n");
            pthread_mutex_unlock(&file_lock); // release lock, we couldn't create the file
            return 500;
        }
    }
    // lock file for writing (exclusive)
    flock(FD, LOCK_EX);
    pthread_mutex_unlock(&file_lock); // release lock, we have the file

    // skip over the request header
    int message_len = request_data->first_read_size - request_data->header_len;
    if (message_len > request_data->content_len) {
        message_len = request_data->content_len;
    }
    char *buffer;
    buffer = http_request + request_data->header_len;

    // write the current buffer into the file
    int total_bytes_written = WriteOut(FD, (uint8_t *) buffer, message_len);
    if (total_bytes_written < 0) {
        dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal "
                        "Server Error\n");
        close(FD);
        return 500;
    }

    // write rest of buffer into file
    char localbuff[BLOCK_SIZE];
    while (total_bytes_written < request_data->content_len) {
        int bytes = read(
            connfd, (uint8_t *) (localbuff), BLOCK_SIZE); // read up to block size from connfd
        int bytes_written
            = WriteOut(FD, (uint8_t *) (localbuff), bytes); // write the bytes read into the file
        if (bytes_written < 0) {
            dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                            "22\r\n\r\nInternal Server Error\n");
            close(FD);
            return 500;
        }
        total_bytes_written += bytes_written; // update total bytes written
    }

    // send good response and close
    if (status_code == 200) {
        dprintf(connfd, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
    } else {
        dprintf(connfd, "HTTP/1.1 201 Created\r\nContent-Length: 8\r\n\r\nCreated\n");
    }
    close(FD);
    return status_code;
}

int APPEND(struct request *request_data, int connfd, char *http_request) {
    pthread_mutex_lock(&file_lock); // grab lock to open the file
    int FD = open(request_data->uri, O_WRONLY | O_APPEND);
    if (FD == -1) {
        if (errno == ENOENT) {
            dprintf(connfd, "HTTP/1.1 404 Not Found\r\nContent-Length: 10\r\n\r\nNot Found\n");
            pthread_mutex_unlock(&file_lock); // release lock, file not found
            return 404;
        } else {
            dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                            "22\r\n\r\nInternal Server Error\n");
            pthread_mutex_unlock(&file_lock); // release lock, internal server error
            return 500;
        }
    }
    // lock file for writing (exclusive)
    flock(FD, LOCK_EX);
    pthread_mutex_unlock(&file_lock); // release lock, we have the file

    // skip over the request header
    int message_len = request_data->first_read_size - request_data->header_len;
    if (message_len > request_data->content_len) {
        message_len = request_data->content_len;
    }
    char *buffer;
    buffer = http_request + request_data->header_len;

    // lock file for writing (exclusive)
    flock(FD, LOCK_EX);

    // write the current buffer into the file
    int total_bytes_written = WriteOut(FD, (uint8_t *) buffer, message_len);
    if (total_bytes_written < 0) {
        dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 22\r\n\r\nInternal "
                        "Server Error\n");
        close(FD);
        return 500;
    }

    // write rest of buffer into file
    char localbuff[BLOCK_SIZE];
    while (total_bytes_written < request_data->content_len) {
        int bytes = read(
            connfd, (uint8_t *) (localbuff), BLOCK_SIZE); // read up to block size from connfd
        int bytes_written
            = WriteOut(FD, (uint8_t *) (localbuff), bytes); // write the bytes read into the file
        if (bytes_written < 0) {
            dprintf(connfd, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: "
                            "22\r\n\r\nInternal Server Error\n");
            close(FD);
            return 500;
        }
        total_bytes_written += bytes_written; // update total bytes written
    }

    // send good response
    dprintf(connfd, "HTTP/1.1 200 OK\r\nContent-Length: 3\r\n\r\nOK\n");
    close(FD);
    return 200;
}

// This function takes in a file descriptor, reads BLOCK_SIZE bytes of the file, and puts each byte into buffer.
// returns the number of bytes read
int ReadIn(int FD, uint8_t *buffer) {
    int totalBytes = 0; // total number of bytes we have read in this function
    int bytesRead = 0; // bytes we read in last read() call
    while ((bytesRead = read(FD, buffer + totalBytes, BLOCK_SIZE - totalBytes))
           > 0) { // while we haven't reached the end of the file
        totalBytes += bytesRead; // increment total by how many we just read
        if ((totalBytes >= BLOCK_SIZE)) { // if we read all the bytes we wanted to or buffer is done
            break;
        }
    }
    return totalBytes;
}

// This function takes in a buffer, and the size of the buffer (number of bytes), and a file desciptor to write to
// returns the number of bytes written
int WriteOut(int FD, uint8_t *buffer, int bytesToWrite) {
    int totalBytes = 0; // total number of bytes we have read in this function
    int bytesWritten = 0; // bytes we read in last read() call
    while ((bytesWritten = write(FD, buffer + totalBytes, bytesToWrite - totalBytes))
           > 0) { // while we haven't reached the end of the file
        totalBytes += bytesWritten; // increment total by how many we just read
        if (totalBytes >= bytesToWrite) { // if we wrote all the bytes we wanted to
            break;
        }
    }
    return totalBytes;
}
