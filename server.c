#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 4096
#define MAX_THREADS 10
#define MAX_HEADERS 10
#define MAX_PATH_LENGTH 256
#define MAX_METHOD_LENGTH 16
#define MAX_VERSION_LENGTH 16

// Structure to store HTTP headers
typedef struct {
    char key[256];
    char value[256];
} Header;

// Thread pool structure
typedef struct {
    pthread_t threads[MAX_THREADS];
    atomic_int shutdown;
} ThreadPool;

// Function to log messages
void log_message(const char *message) {
    printf("[LOG] %s\n", message);
}

// Function to send an HTTP response
void send_response(int client_socket, const char *status, const char *content_type, const char *body) {
    char response[BUFFER_SIZE];
    snprintf(response, BUFFER_SIZE,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %zu\r\n"
             "\r\n"
             "%s",
             status, content_type, strlen(body), body);

    log_message("Sending response:");
    log_message(response);

    if (send(client_socket, response, strlen(response), 0) < 0) {
        perror("send failed");
    }
}

// Function to determine content type based on file extension
const char *get_content_type(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) {
        return "application/octet-stream";
    }
    if (strcmp(ext, ".html") == 0 || strcmp(ext, ".htm") == 0) {
        return "text/html";
    } else if (strcmp(ext, ".css") == 0) {
        return "text/css";
    } else if (strcmp(ext, ".js") == 0) {
        return "application/javascript";
    } else if (strcmp(ext, ".png") == 0) {
        return "image/png";
    } else if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(ext, ".gif") == 0) {
        return "image/gif";
    } else if (strcmp(ext, ".svg") == 0) {
        return "image/svg+xml";
    } else if (strcmp(ext, ".ico") == 0) {
        return "image/x-icon";
    } else {
        return "application/octet-stream";
    }
}

// Function to parse the request line
void parse_request_line(const char *request_line, char *method, char *path, char *version) {
    sscanf(request_line, "%s %s %s", method, path, version);
}

// Function to parse headers
int parse_headers(const char *request, Header *headers, int max_headers) {
    int header_count = 0;
    const char *header_start = strstr(request, "\r\n") + 2; // Skip request line
    const char *header_end = strstr(header_start, "\r\n\r\n");

    if (!header_start || !header_end) {
        return 0; // No headers found
    }

    char header_buffer[BUFFER_SIZE];
    strncpy(header_buffer, header_start, header_end - header_start);
    header_buffer[header_end - header_start] = '\0';

    char *line = strtok(header_buffer, "\r\n");
    while (line && header_count < max_headers) {
        sscanf(line, "%[^:]: %[^\r\n]", headers[header_count].key, headers[header_count].value);
        header_count++;
        line = strtok(NULL, "\r\n");
    }

    return header_count;
}

// Function to handle the request
void handle_request(int client_socket, const char *request) {
    char method[MAX_METHOD_LENGTH], path[MAX_PATH_LENGTH], version[MAX_VERSION_LENGTH];
    Header headers[MAX_HEADERS];
    int header_count;

    // Parse the request line
    parse_request_line(request, method, path, version);

    // Parse headers
    header_count = parse_headers(request, headers, MAX_HEADERS);

    // Handle only GET requests
    if (strcmp(method, "GET") != 0) {
        send_response(client_socket, "405 Method Not Allowed", "text/plain", "Method Not Allowed");
        return;
    }

    // Check for directory traversal
    if (strstr(path, "..")) {
        send_response(client_socket, "403 Forbidden", "text/plain", "Access Denied");
        return;
    }

    // Default to index.html if path is "/"
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    // Construct the full file path
    char full_path[MAX_PATH_LENGTH + 2];
    snprintf(full_path, sizeof(full_path), ".%s", path);

    // Try to open the file
    FILE *file = fopen(full_path, "r");
    if (!file) {
        send_response(client_socket, "404 Not Found", "text/plain", "File Not Found");
        return;
    }

    // Read the file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        perror("malloc failed");
        fclose(file);
        return;
    }
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    // Send the file content as the response
    send_response(client_socket, "200 OK", get_content_type(path), file_content);
    free(file_content);
}

// Structure to pass client socket to the thread
typedef struct {
    int client_socket;
    char request[BUFFER_SIZE];
} ThreadData;

// Thread function to handle client requests
void *handle_client_thread(void *arg) {
    ThreadData *data = (ThreadData *)arg;
    handle_request(data->client_socket, data->request);
    close(data->client_socket);
    free(data);
    return NULL;
}

// Signal handler for graceful shutdown
void handle_shutdown(int sig) {
    log_message("Shutting down server...");
    exit(0);
}

// Initialize thread pool
void init_thread_pool(ThreadPool *pool) {
    for (int i = 0; i < MAX_THREADS; i++) {
        pool->threads[i] = 0;
    }
    pool->shutdown = 0;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    ThreadPool pool;

    // Initialize thread pool
    init_thread_pool(&pool);

    // Set up signal handler for graceful shutdown
    signal(SIGINT, handle_shutdown);

    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Bind socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen for connections
    if (listen(server_socket, 5) < 0) {
        perror("listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    log_message("Server is running on http://localhost:8080");

    while (!pool.shutdown) {
        // Accept a connection
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("accept failed");
            continue;
        }

        // Read the request
        char request[BUFFER_SIZE];
        ssize_t bytes_read = recv(client_socket, request, BUFFER_SIZE - 1, 0);
        if (bytes_read < 0) {
            perror("recv failed");
            close(client_socket);
            continue;
        }
        request[bytes_read] = '\0';

        log_message("Received request:");
        log_message(request);

        // Create a new thread to handle the request
        ThreadData *data = malloc(sizeof(ThreadData));
        if (!data) {
            perror("malloc failed");
            close(client_socket);
            continue;
        }
        data->client_socket = client_socket;
        strncpy(data->request, request, BUFFER_SIZE);

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client_thread, data) != 0) {
            perror("pthread_create failed");
            free(data);
            close(client_socket);
            continue;
        }

        // Detach the thread to handle its own cleanup
        pthread_detach(thread);
    }

    // Close the server socket
    close(server_socket);
    log_message("Server shutdown complete.");
    return 0;
}