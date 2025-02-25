#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>


#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_HEADERS 10
#define MAX_PATH_LENGTH 256
#define MAX_METHOD_LENGTH 16
#define MAX_VERSION_LENGTH 16
#define THREAD_POOL_SIZE 10

// Structure to store HTTP headers
typedef struct {
    char key[256];
    char value[256];
} Header;

// Job structure for thread pool
typedef struct Job {
    int client_socket;
    struct Job* next;
} Job;

// Job queue structure
typedef struct {
    Job *head;
    Job *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} JobQueue;

JobQueue job_queue = { NULL, NULL, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER };

// Global flag for server running (used for graceful shutdown)
volatile sig_atomic_t running = 1;

// Function to log messages
void log_message(const char *message) {
    printf("[LOG] %s\n", message);
}

// Function to send an HTTP response
void send_response(int client_socket, const char *status, const char *content_type, const char *body) {
    char header[BUFFER_SIZE];
    int body_length = (int)strlen(body);
    int header_length = snprintf(header, sizeof(header),
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "\r\n",
             status, content_type, body_length);
    if (header_length < 0 || header_length >= (int)sizeof(header)) {
        log_message("Response header too large");
        return;
    }
    
    if (send(client_socket, header, header_length, 0) < 0) {
        perror("send header failed");
        return;
    }
    
    if (send(client_socket, body, body_length, 0) < 0) {
        perror("send body failed");
    }
}

// Function to determine content type based on file extension
const char *get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (strcasecmp(ext, ".html") == 0 || strcasecmp(ext, ".htm") == 0) {
        return "text/html";
    } else if (strcasecmp(ext, ".css") == 0) {
        return "text/css";
    } else if (strcasecmp(ext, ".js") == 0) {
        return "application/javascript";
    } else if (strcasecmp(ext, ".png") == 0) {
        return "image/png";
    } else if (strcasecmp(ext, ".jpg") == 0 || strcasecmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcasecmp(ext, ".gif") == 0) {
        return "image/gif";
    } else if (strcasecmp(ext, ".svg") == 0) {
        return "image/svg+xml";
    } else if (strcasecmp(ext, ".ico") == 0) {
        return "image/x-icon";
    } else {
        return "application/octet-stream";
    }
}

// Function to parse the request line
void parse_request_line(const char *request_line, char *method, char *path, char *version) {
    // Use width specifiers to avoid buffer overflow
    sscanf(request_line, "%15s %255s %15s", method, path, version);
}

// Function to parse headers
int parse_headers(const char *request, Header *headers, int max_headers) {
    int header_count = 0;
    const char *header_start = strstr(request, "\r\n");
    if (!header_start) {
        return 0;
    }
    header_start += 2; // Skip the request line

    const char *header_end = strstr(header_start, "\r\n\r\n");
    if (!header_end) {
        return 0;
    }

    size_t headers_length = header_end - header_start;
    char *header_buffer = malloc(headers_length + 1);
    if (!header_buffer) {
        perror("malloc failed");
        return 0;
    }
    strncpy(header_buffer, header_start, headers_length);
    header_buffer[headers_length] = '\0';

    char *line = strtok(header_buffer, "\r\n");
    while (line && header_count < max_headers) {
        sscanf(line, "%255[^:]: %255[^\r\n]", headers[header_count].key, headers[header_count].value);
        header_count++;
        line = strtok(NULL, "\r\n");
    }
    free(header_buffer);
    return header_count;
}

// Function to handle the HTTP request
void handle_request(int client_socket, const char *request) {
    char method[MAX_METHOD_LENGTH] = {0};
    char path[MAX_PATH_LENGTH] = {0};
    char version[MAX_VERSION_LENGTH] = {0};
    Header headers[MAX_HEADERS];
    int header_count;

    parse_request_line(request, method, path, version);
    header_count = parse_headers(request, headers, MAX_HEADERS);

    // Only allow GET requests
    if (strcasecmp(method, "GET") != 0) {
        send_response(client_socket, "405 Method Not Allowed", "text/plain", "Method Not Allowed");
        return;
    }

    // Prevent directory traversal
    if (strstr(path, "..")) {
        send_response(client_socket, "403 Forbidden", "text/plain", "Access Denied");
        return;
    }

    // Default to index.html if path is "/"
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    char full_path[MAX_PATH_LENGTH + 2];
    snprintf(full_path, sizeof(full_path), ".%s", path);

    FILE *file = fopen(full_path, "r");
    if (!file) {
        send_response(client_socket, "404 Not Found", "text/plain", "File Not Found");
        return;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("fseek failed");
        fclose(file);
        return;
    }
    long file_size = ftell(file);
    if (file_size < 0) {
        perror("ftell failed");
        fclose(file);
        return;
    }
    rewind(file);

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        perror("malloc failed");
        fclose(file);
        return;
    }
    size_t read_size = fread(file_content, 1, file_size, file);
    if (read_size != (size_t)file_size) {
        perror("fread failed");
        free(file_content);
        fclose(file);
        return;
    }
    file_content[file_size] = '\0';
    fclose(file);

    send_response(client_socket, "200 OK", get_content_type(path), file_content);
    free(file_content);
}

// Worker thread function for the thread pool
void *worker_thread(void *arg) {
    (void)arg;
    char request[BUFFER_SIZE];
    while (running) {
        pthread_mutex_lock(&job_queue.mutex);
        while (running && job_queue.head == NULL) {
            pthread_cond_wait(&job_queue.cond, &job_queue.mutex);
        }
        if (!running) {
            pthread_mutex_unlock(&job_queue.mutex);
            break;
        }
        Job *job = job_queue.head;
        job_queue.head = job ? job->next : NULL;
        if (job_queue.head == NULL)
            job_queue.tail = NULL;
        pthread_mutex_unlock(&job_queue.mutex);
        
        if (job) {
            int client_socket = job->client_socket;
            free(job);

            // Read the client's request
            ssize_t bytes_read = recv(client_socket, request, sizeof(request) - 1, 0);
            if (bytes_read < 0) {
                perror("recv failed");
                close(client_socket);
                continue;
            }
            request[bytes_read] = '\0';

            log_message("Received request:");
            log_message(request);

            handle_request(client_socket, request);
            close(client_socket);
        }
    }
    return NULL;
}

// Add a new job (client connection) to the job queue
void add_job(int client_socket) {
    Job *new_job = malloc(sizeof(Job));
    if (!new_job) {
        perror("malloc failed");
        close(client_socket);
        return;
    }
    new_job->client_socket = client_socket;
    new_job->next = NULL;

    pthread_mutex_lock(&job_queue.mutex);
    if (job_queue.tail == NULL) {
        job_queue.head = new_job;
        job_queue.tail = new_job;
    } else {
        job_queue.tail->next = new_job;
        job_queue.tail = new_job;
    }
    pthread_cond_signal(&job_queue.cond);
    pthread_mutex_unlock(&job_queue.mutex);
}

// Signal handler for graceful shutdown
void handle_shutdown(int sig) {
    (void)sig;
    running = 0;
    log_message("Shutting down server...");
    pthread_cond_broadcast(&job_queue.cond);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct sigaction sa;

    // Set up the signal handler using sigaction
    sa.sa_handler = handle_shutdown;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    // Create the server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the specified port
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 10) < 0) {
        perror("listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    log_message("Server is running on http://localhost:8080");

    // Create the worker thread pool
    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        if (pthread_create(&threads[i], NULL, worker_thread, NULL) != 0) {
            perror("pthread_create failed");
        }
    }

    // Main loop: accept new connections and add them as jobs
    while (running) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            if (errno == EINTR) continue; // Interrupted by signal, check running flag
            perror("accept failed");
            continue;
        }
        add_job(client_socket);
    }

    // Close the server socket
    close(server_socket);

    // Wait for all worker threads to finish
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    log_message("Server shutdown complete.");
    return 0;
}
