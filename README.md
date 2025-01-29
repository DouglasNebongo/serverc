---

```
# Simple HTTP Web Server in C

This is a simple HTTP web server implemented in C. It serves static files (e.g., HTML, CSS, JavaScript) from the current directory and handles basic `GET` requests. This project is intended for educational purposes and demonstrates the fundamentals of socket programming and HTTP protocol implementation.

---

## Features

- Serves static files (e.g., `index.html`, `style.css`, `script.js`).
- Handles `GET` requests.
- Supports basic error handling (e.g., `404 Not Found`, `405 Method Not Allowed`).
- Easy to compile and run.

---

## Prerequisites

- A C compiler (e.g., `gcc`).
- Basic knowledge of the command line.

---

## Getting Started

### 1. Clone the Repository (if applicable)
```bash
git clone git@github.com:DouglasNebongo/serverc.git
cd simple-c-webserver
```

### 2. Compile the Server
Compile the `server.c` file using `gcc`:
```bash
gcc -o server server.c
```

### 3. Run the Server
Start the server by running the compiled binary:
```bash
./server
```

The server will start and listen on port `8080`.

### 4. Access the Server
Open a web browser and navigate to:
```
http://localhost:8080
```

By default, the server will look for an `index.html` file in the same directory as the server. If the file exists, it will be served. Otherwise, a `404 Not Found` error will be returned.

---

## Directory Structure

Place your website files (e.g., HTML, CSS, JavaScript) in the same directory as the server. For example:
```
.
├── server.c
├── server       # Compiled binary
├── index.html   # Default homepage
├── style.css    # Example CSS file
└── script.js    # Example JavaScript file
```

---

## Example Usage

1. Create an `index.html` file:
   ```html
   <!DOCTYPE html>
   <html>
   <head>
       <title>Simple Web Server</title>
       <link rel="stylesheet" href="style.css">
   </head>
   <body>
       <h1>Hello, World!</h1>
       <p>This is a simple web server written in C.</p>
       <script src="script.js"></script>
   </body>
   </html>
   ```

2. Run the server:
   ```bash
   ./server
   ```

3. Open your browser and visit `http://localhost:8080`. You should see the `index.html` page.

---

## How It Works

### 1. Socket Creation
- The server creates a socket using the `socket()` function.
- It binds the socket to a specific IP address and port using `bind()`.
- It listens for incoming connections using `listen()`.

### 2. Handling Requests
- When a client connects, the server accepts the connection using `accept()`.
- It reads the HTTP request using `recv()`.
- It parses the request to determine the requested file.
- If the file exists, it reads the file and sends it back to the client using `send()`.

### 3. Sending Responses
- The server sends an HTTP response with the appropriate status code, content type, and body.

### 4. Serving Static Files
- The server serves files from the current directory. For example, a request to `/index.html` will serve the `index.html` file.

---

## Limitations

- Only supports `GET` requests.
- Does not handle advanced HTTP features (e.g., headers, query parameters, request bodies).
- Not suitable for production use (e.g., no security features, no multithreading).

---

## Extending the Server

Here are some ideas for improving the server:
- Add support for `POST` requests.
- Implement multithreading to handle multiple clients simultaneously.
- Add support for serving different MIME types (e.g., images, videos).
- Improve error handling and logging.

---


