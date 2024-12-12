// Asgn 4: Multithreaded HTTP server.
// By: Kai Mullens

#include "asgn2_helper_funcs.h"
#include "connection.h"
#include "debug.h"
#include "response.h"
#include "request.h"
#include "queue.h"
#include "rwlock.h"

#include "hash_table.h"

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#include <sys/stat.h>

#define DEFAULT_THREADS 4

void handle_connection(int);
void handle_get(conn_t *);
void handle_put(conn_t *);
void handle_unsupported(conn_t *);

// Initialize the file lock hash table, thread queue, and file lock mutex
pthread_mutex_t file_lock_ht_mutex = PTHREAD_MUTEX_INITIALIZER;
queue_t *thread_queue;
Hashtable *file_lock_ht = NULL;

/** 
 * @brief Get the file lock for a given filename. If the lock does not exist, create a new lock.
 * 
 * This function utilizes a hash table to store file locks.
 * 
 * @param filename The name of the file to get the lock for.
 * 
 * @return The lock for the file.
*/
rwlock_t *get_file_lock(const char *filename) {
    pthread_mutex_lock(&file_lock_ht_mutex);
    rwlock_t **file_lock = hash_get(file_lock_ht, filename);

    if (file_lock) {
        pthread_mutex_unlock(&file_lock_ht_mutex);
        return *file_lock;
    }

    rwlock_t *new_lock = rwlock_new(N_WAY, 1);
    if (!new_lock) {
        fprintf(stderr, "Failed to create new lock for file: %s\n", filename);
        pthread_mutex_unlock(&file_lock_ht_mutex);
        return NULL;
    }

    if (!hash_put(file_lock_ht, filename, new_lock)) {
        rwlock_delete(&new_lock);
        fprintf(stderr, "Failed to add lock to hash table for file: %s\n", filename);
        pthread_mutex_unlock(&file_lock_ht_mutex); 
        return NULL;
    }
    pthread_mutex_unlock(&file_lock_ht_mutex);
    return new_lock;
}
/**
 * @brief Cleanup all file locks and the hash table.
 * 
 * This function should be called when the server is shutting down.
 * 
 * @return void
 */
void cleanup_file_locks(void) {
    if (!file_lock_ht) {
        return;
    }
    pthread_mutex_lock(&file_lock_ht_mutex);
    for (int i = 0; i < TABLE_SIZE; i++) {
        LL *list = file_lock_ht->table[i];
        Node *current = list->head;

        while (current != NULL) {
            rwlock_t *lock = current->data.id;
            if (lock) {
                rwlock_delete(&lock);
            }
            current = current->next;
        }
        list_destroy(&list);
    }

    hash_destroy(&file_lock_ht);
    pthread_mutex_unlock(&file_lock_ht_mutex);
}
/**
 * @brief Logs audit information for an HTTP request.
 *
 * This function logs the HTTP method, URI, status code, and request ID
 * of an HTTP request to the standard error stream.
 *
 * @param method The HTTP method used in the request (e.g., "GET", "POST").
 * @param conn A pointer to the connection object containing request details.
 * @param status_code The HTTP status code of the response.
 */
void audit_log(const char *method, conn_t *conn, int status_code) {
    const char *uri = conn_get_uri(conn);
    const char *request_id = conn_get_header(conn, "Request-Id");

    if (uri == NULL) {
        uri = "Unknown uri";
    }

    if (request_id == NULL) {
        request_id = "0";
    }

    fprintf(stderr, "%s,/%s,%d,%s\n", method, uri, status_code, request_id);
}
/**
 * @brief Worker thread function for handling HTTP requests.
 * 
 * This function is the main function for worker threads. It pops a connection
 * file descriptor from the thread queue, handles the connection, and then closes
 * the connection.
 * 
 * @return void
 */
void *worker_thread(void) {
    while (1) {
        void *connfd;
        queue_pop(thread_queue, (void **) &connfd);
        handle_connection((int) (uintptr_t) connfd);
        close((int) (uintptr_t) connfd);
    }
}
/**
 * @brief Main function for the HTTP server.
 * 
 * This function initializes the listener socket, worker threads, and other data structures.
 * It then enters an infinite loop to accept connections and push them to the thread queue.
 * 
 * @param argc The number of command-line arguments.
 * @param argv The command-line arguments.
 * 
 * @return int The exit status of the program.
 */
int main(int argc, char **argv) {
    if (argc < 2) {
        warnx("wrong arguments: %s port_num", argv[0]);
        fprintf(stderr, "usage: %s -t <thread count> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Parse command-line arguments
    int opt;
    int num_threads = DEFAULT_THREADS;
    while ((opt = getopt(argc, argv, "t:")) != -1) {
        switch (opt) {
        case 't': num_threads = atoi(optarg); break;
        default: fprintf(stderr, "usage: %s [-t threads] <port>\n", argv[0]); return EXIT_FAILURE;
        }
    }
    // Get port number
    char *endptr = NULL;
    size_t port = (size_t) strtoull(argv[optind], &endptr, 10);
    if (endptr && *endptr != '\0') {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }
    
    // Initialize listener socket
    signal(SIGPIPE, SIG_IGN);
    Listener_Socket sock;
    if (listener_init(&sock, port) < 0) {
        fprintf(stderr, "Invalid Port\n");
        return EXIT_FAILURE;
    }

    // Initialize worker threads, queue, and file lock hash table
    thread_queue = queue_new(num_threads);
    file_lock_ht = hash_create();
    pthread_t threads[num_threads];

    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        pthread_create(&threads[i], NULL, (void *(*) (void *) ) worker_thread, NULL);
    }

    // Main loop to accept connections and push them to the thread queue
    while (1) {
        int connfd = listener_accept(&sock);
        queue_push(thread_queue, (void *) (uintptr_t) connfd);
    }

    // Cleanup file locks and hash table
    cleanup_file_locks();
    return EXIT_SUCCESS;
}
/**
 * @brief Handle an HTTP connection.
 * 
 * This function parses an HTTP connection, determines the request type, and then
 * calls the appropriate handler function.
 * 
 * @param connfd The file descriptor for the connection.
 * 
 * @return void
 */
void handle_connection(int connfd) {
    conn_t *conn = conn_new(connfd);

    const Response_t *res = conn_parse(conn);

    if (res != NULL) {
        conn_send_response(conn, res);
    } else {
        // debug("%s", conn_str(conn));
        const Request_t *req = conn_get_request(conn);
        if (req == &REQUEST_GET) {
            handle_get(conn);
        } else if (req == &REQUEST_PUT) {
            handle_put(conn);
        } else {
            handle_unsupported(conn);
        }
    }

    conn_delete(&conn);
}

/**
 * @brief Handles a GET request.
 * 
 * This function handles a GET request by opening the requested file, checking for errors,
 * and then sending the file to the client.
 * 
 * @param conn The connection object containing the request details.
 * 
 * @return void
 */
void handle_get(conn_t *conn) {
    char *uri = conn_get_uri(conn);

    // Open the file and ensure it is valid
    int fd = open(uri, O_RDONLY);
    if (fd < 0) {
        if (errno == EACCES) {
            conn_send_response(conn, &RESPONSE_FORBIDDEN);
            audit_log("GET", conn, 403);
            return;
        } else if (errno == ENOENT) {
            conn_send_response(conn, &RESPONSE_NOT_FOUND);
            audit_log("GET", conn, 404);
            return;
        } else {
            conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
            audit_log("GET", conn, 500);
            return;
        }
    }
    // Get the per-file lock
    rwlock_t *lock = get_file_lock(uri);
    reader_lock(lock);
    // debug("Reader lock acquired");

    // Get the size of the file.
    struct stat file_stat;
    if (fstat(fd, &file_stat) < 0) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("GET", conn, 500);
        reader_unlock(lock);
        // debug("Reader lock released");
        return;
    }
    long file_size = file_stat.st_size;

    // Check if the file is a directory
    if (S_ISDIR(file_stat.st_mode)) {
        conn_send_response(conn, &RESPONSE_FORBIDDEN);
        audit_log("GET", conn, 403);
        reader_unlock(lock);
        // debug("Reader lock released");
        return;
    }

    // Send the file to the client
    const Response_t *res = conn_send_file(conn, fd, file_size);

    // Send the response
    if (res != NULL) {
        conn_send_response(conn, res);
        audit_log("GET", conn, 500);
    } else {
        audit_log("GET", conn, 200);
    }
    // Close the file
    close(fd);
    reader_unlock(lock);
    // debug("Reader lock released");
}

/**
 * @brief Handles a PUT request.
 * 
 * This function handles a PUT request by creating a temporary file, receiving the data from the client,
 * and then passing the data to the target file.
 * 
 * @param conn The connection object containing the request details.
 * 
 * @return void
 */
void handle_put(conn_t *conn) {
    char *uri = conn_get_uri(conn);
    const Response_t *res = NULL;

    // Creates unique temp file
    char temp_file_template[] = "tmpXXXXXX";
    int tmp_fd = mkstemp(temp_file_template);
    if (tmp_fd < 0) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("PUT", conn, 500);
        return;
    }

    // Receive data from the client into the temporary file
    res = conn_recv_file(conn, tmp_fd);
    if (res != NULL) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("PUT", conn, 500);
        close(tmp_fd);
        unlink(temp_file_template);
        return;
    }
    close(tmp_fd);

    rwlock_t *lock = get_file_lock(uri);
    writer_lock(lock);

    // Determine if the target file already exists
    int exists = access(uri, F_OK);

    // Opens the target file
    int target_fd = open(uri, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (target_fd < 0) {
        if (errno == EACCES || errno == EISDIR) {
            conn_send_response(conn, &RESPONSE_FORBIDDEN);
            audit_log("PUT", conn, 403);
        } else {
            conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
            audit_log("PUT", conn, 500);
        }
        unlink(temp_file_template);
        writer_unlock(lock);
        return;
    }

    // Re opens temp file
    tmp_fd = open(temp_file_template, O_RDONLY);
    if (tmp_fd < 0) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("PUT", conn, 500);
        close(target_fd);
        unlink(temp_file_template);
        writer_unlock(lock);
        return;
    }

    // Gets size of temp file
    struct stat file_stat;
    if (fstat(tmp_fd, &file_stat) < 0) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);

        audit_log("PUT", conn, 500);
        close(target_fd);
        close(tmp_fd);
        unlink(temp_file_template);
        writer_unlock(lock);
        // debug("Writer lock released");
        return;
    }
    size_t file_size = file_stat.st_size;

    // Passes the data from the temp file to the target file
    ssize_t result = pass_n_bytes(tmp_fd, target_fd, file_size);
    if (result == -1) {
        conn_send_response(conn, &RESPONSE_INTERNAL_SERVER_ERROR);
        audit_log("PUT", conn, 500);
        close(target_fd);
        close(tmp_fd);
        unlink(temp_file_template);
        writer_unlock(lock);
        // debug("Writer lock released");
        return;
    }

    close(tmp_fd);
    close(target_fd);

    if (exists < 0) {
        conn_send_response(conn, &RESPONSE_CREATED);
        audit_log("PUT", conn, 201);
    } else {
        conn_send_response(conn, &RESPONSE_OK);
        audit_log("PUT", conn, 200);
    }

    unlink(temp_file_template);
    writer_unlock(lock);
}

/**
 * @brief Handles an unsupported request.
 * 
 * This function sends a 501 Not Implemented response to the client.
 * 
 * @param conn The connection object containing the request details.
 * 
 * @return void
 */
void handle_unsupported(conn_t *conn) {
    // debug("Handling unsupported request");

    // Send responses
    conn_send_response(conn, &RESPONSE_NOT_IMPLEMENTED);
    audit_log("UNSUPPORTED", conn, 501);
}
