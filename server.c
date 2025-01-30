#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define BUFFER_SIZE 4096

// Structure to store HTTP headers
typedef struct {
    char key[256];
    char value[256];
} Header;

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
    // Log the response to the console
    printf("Sending response:\n");
    printf("Status: %s\n", status);
    printf("Content-Type: %s\n", content_type);
    printf("Content-Length: %zu\n", strlen(body));
    printf("Body: %s\n", body);

    // Send the response to the client
    send(client_socket, response, strlen(response), 0);
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
    char method[16], path[256], version[16];
    Header headers[10];
    int header_count;

    // Parse the request line
    parse_request_line(request, method, path, version);

    // Parse headers
    header_count = parse_headers(request, headers, 10);

    // Handle only GET requests
    if (strcmp(method, "GET") != 0) {
        send_response(client_socket, "405 Method Not Allowed", "text/plain", "Method Not Allowed");
        return;
    }

    // Default to index.html if path is "/"
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    // Construct the full file path
    char *full_path = malloc(strlen(path) + 2); // +2 for '.' and null terminator
    if (!full_path) {
    perror("malloc failed");
       return;
    }
    snprintf(full_path, strlen(path) + 2, ".%s", path);


    // Try to open the file
    FILE *file = fopen(full_path, "r");
    if (!file) {
        send_response(client_socket, "404 Not Found", "text/plain", "File Not Found");
        return;
    }
    free(full_path);
    // Read the file content
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *file_content = malloc(file_size + 1);
    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);


    // Send the file content as the response
    send_response(client_socket, "200 OK", "text/html", file_content);
    free(file_content);
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

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

    printf("Server is running on http://localhost:%d\n", PORT);

    while (1) {
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
        
        printf("Request:\n%s\n", request);



        // Handle the request
        handle_request(client_socket, request);

        // Close the client socket
        close(client_socket);
    }

    // Close the server socket (this will never be reached in this example)
    close(server_socket);
    return 0;
}