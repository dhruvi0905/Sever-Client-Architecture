#include <stdio.h> // This brings in tools for printing to the screen and reading input, like a typewriter for the program.
#include <stdlib.h> // This adds tools for managing memory, generating random numbers, and exiting the program.
#include <string.h> // This provides functions to work with text, like copying or comparing strings.
#include <unistd.h> // This includes system tools for things like closing files or pausing the program.
#include <sys/socket.h> // This gives tools to create network connections, like setting up a phone line for data.
#include <sys/types.h> // This defines basic data types, like IDs for processes, used in system programming.
#include <sys/stat.h> // This lets the program check file details, like whether a file exists or its size.
#include <netinet/in.h> // This sets up structures for internet addresses, needed for networking.
#include <arpa/inet.h> // This helps convert network addresses, like turning an IP address into a usable format.
#include <errno.h> // This provides error codes to figure out what went wrong when something fails.
#include <libgen.h> // This includes tools to handle file paths, like getting the name of a file from its path.

#define SERVER_IP "127.0.0.1" // This sets the server’s address to "127.0.0.1", which means the local computer.
#define SERVER_PORT 8080 // This sets the port number to 8080, like a specific channel for communication.
#define BUFFER_SIZE 1024 // This defines a 1024-byte space for holding data, like a bucket for carrying information.
#define MAX_PATH_LENGTH 1024 // This limits file paths to 1024 characters, so paths don’t get too long.

int fileExists(const char *path) { // This starts a function to check if a file exists at a given path.
    struct stat buffer; // This creates a container to hold information about the file.
    return (stat(path, &buffer) == 0); // This checks if the file exists and returns true if it does, false if not.
}

int uploadFile(int sock, const char *local_path, const char *server_path) { // This starts a function to upload a file to the server.
    char buffer[BUFFER_SIZE]; // This creates a bucket to hold data while transferring the file.
    FILE *file; // This will hold the file being uploaded.
    size_t bytes_read; // This tracks how many bytes are read from the file at a time.
    
    if (!fileExists(local_path)) { // This checks if the file exists on the local computer.
        printf("Error: Local file %s not found\n", local_path); // If not, this prints an error message.
        return -1; // This stops the function with an error code.
    }
    
    file = fopen(local_path, "rb"); // This opens the file for reading in binary mode (like raw data).
    if (!file) { // This checks if the file was opened successfully.
        perror("Unable to open file for reading"); // If not, this prints an error with the reason.
        return -1; // This stops the function with an error code.
    }
    
    snprintf(buffer, BUFFER_SIZE, "uploadf %s %s", local_path, server_path); // This creates a command to tell the server to upload a file.
    printf("Sending command: %s\n", buffer); // This prints the command for tracking.
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This sends the command to the server.
        perror("Failed to send command"); // If it fails, this prints an error.
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the bucket to prepare for new data.
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server’s response.
    if (bytes_received <= 0) { // This checks if the response was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive READY: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs if there was another error.
            printf("Failed to receive READY: %s\n", strerror(errno)); // This prints the error with details.
        }
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    printf("Received: %s\n", buffer); // This prints what the server sent back.
    if (strncmp(buffer, "READY", 5) != 0) { // This checks if the server said it’s ready.
        if (strncmp(buffer, "ERROR", 5) == 0) { // This checks if the server sent an error.
            printf("Server error: %s\n", buffer); // If so, this prints the error message.
        } else { // This runs if the server sent something unexpected.
            printf("Server not ready for file transfer: %s\n", buffer); // This prints what was received.
        }
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    
    fseek(file, 0, SEEK_END); // This moves to the end of the file to find its size.
    long file_size = ftell(file); // This gets the file’s size in bytes.
    fseek(file, 0, SEEK_SET); // This moves back to the start of the file.
    if (file_size <= 0) { // This checks if the file size is valid.
        printf("Error: Invalid file size %ld for %s\n", file_size, local_path); // If not, this prints an error.
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    
    snprintf(buffer, BUFFER_SIZE, "%ld", file_size); // This puts the file size into the bucket as text.
    printf("Sending file size: %s\n", buffer); // This prints the size being sent.
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This sends the file size to the server.
        perror("Failed to send file size"); // If it fails, this prints an error.
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server to confirm the size.
    if (bytes_received <= 0) { // This checks if the confirmation was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive SIZE_ACK: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs for other errors.
            printf("Failed to receive SIZE_ACK: %s\n", strerror(errno)); // This prints the error details.
        }
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    printf("Received: %s\n", buffer); // This prints the server’s response.
    if (strcmp(buffer, "SIZE_ACK") != 0) { // This checks if the server confirmed the size.
        printf("File size acknowledgment failed: %s\n", buffer); // If not, this prints what was received.
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    
    printf("Sending file data\n"); // This logs that the file data is being sent.
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) { // This loops to read chunks of the file.
        if (send(sock, buffer, bytes_read, 0) < 0) { // This sends each chunk to the server.
            perror("Failed to send file data"); // If it fails, this prints an error.
            fclose(file); // This closes the file.
            return -1; // This stops the function with an error code.
        }
    }
    if (ferror(file)) { // This checks if there was an error reading the file.
        perror("Error reading file"); // If so, this prints the error.
        fclose(file); // This closes the file.
        return -1; // This stops the function with an error code.
    }
    
    fclose(file); // This closes the file after sending.
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server’s final status.
    if (bytes_received <= 0) { // This checks if the status was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive upload status: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs for other errors.
            printf("Failed to receive upload status: %s\n", errno == 0 ? "Connection closed" : strerror(errno)); // This prints the error.
        }
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    printf("Received status: %s\n", buffer); // This prints the server’s status.
    if (strncmp(buffer, "SUCCESS", 7) == 0) { // This checks if the server said the upload worked.
        printf("File uploaded successfully\n"); // If so, this prints a success message.
        return 0; // This means the upload was successful.
    } else { // This runs if the upload failed.
        printf("Upload failed: %s\n", buffer); // This prints the server’s error message.
        return -1; // This stops the function with an error code.
    }
}

int downloadFile(int sock, const char *server_path, const char *local_path) { // This starts a function to download a file from the server.
    char buffer[BUFFER_SIZE]; // This creates a bucket to hold data during the download.
    FILE *file; // This will hold the file being saved locally.
    long file_size; // This will store the size of the file.
    long total_bytes = 0; // This tracks how many bytes have been received.
    int bytes_received; // This stores the number of bytes received in each chunk.
    
    snprintf(buffer, BUFFER_SIZE, "downlf %s", server_path); // This creates a command to download a file.
    printf("Sending command: %s\n", buffer); // This prints the command for tracking.
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This sends the command to the server.
        perror("Failed to send command"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server to send the file size.
    if (bytes_received <= 0) { // This checks if the size was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive file size: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs for other errors.
            printf("Failed to receive file size: %s\n", strerror(errno)); // This prints the error details.
        }
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    
    if (strncmp(buffer, "ERROR", 5) == 0) { // This checks if the server sent an error.
        printf("Server error: %s\n", buffer); // If so, this prints the error message.
        return -1; // This stops the function with an error code.
    }
    
    file_size = atol(buffer); // This converts the received size to a number.
    if (file_size <= 0) { // This checks if the size is valid.
        printf("Invalid file size received: %ld\n", file_size); // If not, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    if (send(sock, "SIZE_ACK", 8, 0) < 0) { // This sends a confirmation to the server.
        perror("Failed to send SIZE_ACK"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    char dir_path[MAX_PATH_LENGTH]; // This will hold the directory part of the local path.
    snprintf(dir_path, MAX_PATH_LENGTH, "%s", local_path); // This copies the local path to work with.
    char *last_slash = strrchr(dir_path, '/'); // This finds the last slash in the path.
    if (last_slash) { // This checks if there’s a directory in the path.
        *last_slash = '\0'; // This cuts off the filename to get the directory.
        if (strlen(dir_path) > 0) { // This checks if the directory path isn’t empty.
            char cmd[MAX_PATH_LENGTH * 2]; // This will hold a command to create directories.
            snprintf(cmd, MAX_PATH_LENGTH * 2, "mkdir -p %s", dir_path); // This builds a command to create folders.
            printf("Creating local directory: %s\n", cmd); // This prints the command for tracking.
            if (system(cmd) != 0) { // This runs the command to create directories.
                printf("Failed to create directory %s\n", dir_path); // If it fails, this prints an error.
            }
        }
    }
    
    file = fopen(local_path, "wb"); // This opens a file for writing in binary mode.
    if (!file) { // This checks if the file was opened successfully.
        perror("Unable to open file for writing"); // If not, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    while (total_bytes < file_size) { // This loops until all file bytes are received.
        memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0); // This receives a chunk of the file.
        if (bytes_received <= 0) { // This checks if the data was received.
            if (bytes_received == 0) { // This checks if the server closed the connection.
                printf("File receive error: Server closed connection\n"); // If so, this prints an error.
            } else { // This runs for other errors.
                perror("File receive error"); // This prints the error details.
            }
            fclose(file); // This closes the file.
            return -1; // This stops the function with an error code.
        }
        if (fwrite(buffer, 1, bytes_received, file) != bytes_received) { // This writes the chunk to the file.
            perror("File write error"); // If it fails, this prints an error.
            fclose(file); // This closes the file.
            return -1; // This stops the function with an error code.
        }
        total_bytes += bytes_received; // This updates the total bytes received.
        printf("Received %d bytes, total: %ld\n", bytes_received, total_bytes); // This prints progress.
    }
    
    if (fflush(file) != 0 || fclose(file) != 0) { // This ensures the file is saved and closed properly.
        perror("Error finalizing file"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    if (send(sock, "SUCCESS", 7, 0) < 0) { // This sends a success message to the server.
        perror("Failed to send SUCCESS"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    printf("File downloaded successfully to %s\n", local_path); // This prints that the download worked.
    return 0; // This means the download was successful.
}

int removeFile(int sock, const char *server_path) { // This starts a function to delete a file on the server.
    char buffer[BUFFER_SIZE]; // This creates a bucket to hold data.
    
    snprintf(buffer, BUFFER_SIZE, "removef %s", server_path); // This creates a command to delete a file.
    printf("Sending command: %s\n", buffer); // This prints the command for tracking.
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This sends the command to the server.
        perror("Failed to send command"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server’s response.
    if (bytes_received <= 0) { // This checks if the response was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive response: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs for other errors.
            printf("Failed to receive response: %s\n", strerror(errno)); // This prints the error details.
        }
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    
    if (strncmp(buffer, "SUCCESS", 7) == 0) { // This checks if the server said the deletion worked.
        printf("File removed successfully\n"); // If so, this prints a success message.
        return 0; // This means the deletion was successful.
    } else { // This runs if the deletion failed.
        printf("Remove failed: %s\n", buffer); // This prints the server’s error message.
        return -1; // This stops the function with an error code.
    }
}

int downloadTar(int sock, const char *filetype, const char *local_path) { // This starts a function to download a tar file of a specific type.
    char buffer[BUFFER_SIZE]; // This creates a bucket to hold data.
    FILE *file; // This will hold the tar file being saved.
    long file_size; // This will store the size of the tar file.
    long total_bytes = 0; // This tracks how many bytes have been received.
    int bytes_received; // This stores the number of bytes received in each chunk.
    
    snprintf(buffer, BUFFER_SIZE, "downltar %s", filetype); // This creates a command to download a tar file.
    printf("Sending command: %s\n", buffer); // This prints the command for tracking.
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This sends the command to the server.
        perror("Failed to send command"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
    bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0); // This waits for the server to send the file size.
    if (bytes_received <= 0) { // This checks if the size was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive file size: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs for other errors.
            printf("Failed to receive file size: %s\n", strerror(errno)); // This prints the error details.
        }
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    
    if (strncmp(buffer, "ERROR", 5) == 0) { // This checks if the server sent an error.
        printf("Server error: %s\n", buffer); // If so, this prints the error message.
        return -1; // This stops the function with an error code.
    }
    
    file_size = atol(buffer); // This converts the received size to a number.
    if (file_size <= 0) { // This checks if the size is valid.
        printf("Invalid file size received: %ld\n", file_size); // If not, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    if (send(sock, "SIZE_ACK", 8, 0) < 0) { // This sends a confirmation to the server.
        perror("Failed to send SIZE_ACK"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    char dir_path[MAX_PATH_LENGTH]; // This will hold the directory part of the local path.
    snprintf(dir_path, MAX_PATH_LENGTH, "%s", local_path); // This copies the local path to work with.
    char *last_slash = strrchr(dir_path, '/'); // This finds the last slash in the path.
    if (last_slash) { // This checks if there’s a directory in the path.
        *last_slash = '\0'; // This cuts off the filename to get the directory.
        if (strlen(dir_path) > 0) { // This checks if the directory path isn’t empty.
            char cmd[MAX_PATH_LENGTH * 2]; // This will hold a command to create directories.
            snprintf(cmd, MAX_PATH_LENGTH * 2, "mkdir -p %s", dir_path); // This builds a command to create folders.
            printf("Creating local directory: %s\n", cmd); // This prints the command for tracking.
            if (system(cmd) != 0) { // This runs the command to create directories.
                printf("Failed to create directory %s\n", dir_path); // If it fails, this prints an error.
            }
        }
    }
    
    file = fopen(local_path, "wb"); // This opens a file for writing in binary mode.
    if (!file) { // This checks if the file was opened successfully.
        perror("Unable to open file for writing"); // If not, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    while (total_bytes < file_size) { // This loops until all file bytes are received.
        memset(buffer, 0, BUFFER_SIZE); // This clears the bucket for new data.
        bytes_received = recv(sock, buffer, BUFFER_SIZE, 0); // This receives a chunk of the file.
        if (bytes_received <= 0) { // This checks if the data was received.
            if (bytes_received == 0) { // This checks if the server closed the connection.
                printf("File receive error: Server closed connection\n"); // If so, this prints an error.
            } else { // This runs for other errors.
                perror("File receive error"); // This prints the error details.
            }
            fclose(file); // This closes the file.
            return -1; // This stops the function with an error code.
        }
        if (fwrite(buffer, 1, bytes_received, file) != bytes_received) { // This writes the chunk to the file.
            perror("File write error"); // If it fails, this prints an error.
            fclose(file); // This closes the file.
            return -1; // This stops the function with an error code.
        }
        total_bytes += bytes_received; // This updates the total bytes received.
        printf("Received %d bytes, total: %ld\n", bytes_received, total_bytes); // This prints progress.
    }
    
    if (fflush(file) != 0 || fclose(file) != 0) { // This ensures the file is saved and closed properly.
        perror("Error finalizing file"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    if (send(sock, "SUCCESS", 7, 0) < 0) { // This sends a success message to the server.
        perror("Failed to send SUCCESS"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    printf("Tar file downloaded successfully to %s\n", local_path); // This prints that the tar download worked.
    return 0; // This means the download was successful.
}

int listFiles(int sock, const char *server_path) { // This starts a function to list files in a server directory.
    char buffer[BUFFER_SIZE * 10]; // This creates a large bucket to hold the list of files.
    
    snprintf(buffer, BUFFER_SIZE, "dispfnames %s", server_path); // This creates a command to list files.
    printf("Sending command: %s\n", buffer); // This prints the command for tracking.
    if (send(sock, buffer, strlen(buffer), 0) < 0) { // This sends the command to the server.
        perror("Failed to send command"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    memset(buffer, 0, BUFFER_SIZE * 10); // This clears the bucket for new data.
    ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE * 10 - 1, 0); // This waits for the server’s response.
    if (bytes_received <= 0) { // This checks if the response was received.
        if (bytes_received == 0) { // This checks if the server closed the connection.
            printf("Failed to receive file list: Server closed connection\n"); // If so, this prints an error.
        } else { // This runs for other errors.
            printf("Failed to receive file list: %s\n", strerror(errno)); // This prints the error details.
        }
        return -1; // This stops the function with an error code.
    }
    buffer[bytes_received] = '\0'; // This marks the end of the received data.
    
    if (strncmp(buffer, "ERROR", 5) == 0) { // This checks if the server sent an error.
        printf("Server error: %s\n", buffer); // If so, this prints the error message.
        return -1; // This stops the function with an error code.
    } else if (strlen(buffer) == 0) { // This checks if the directory was empty.
        printf("No files found in %s\n", server_path); // If so, this prints that no files were found.
    } else { // This runs if files were found.
        printf("\nFiles in %s:\n", server_path); // This prints the directory name.
        printf("----------------------------------------\n"); // This prints a line for formatting.
        printf("%s\n", buffer); // This prints the list of files.
        printf("----------------------------------------\n"); // This prints another line for formatting.
    }
    
    return 0; // This means the list was retrieved successfully.
}

int displayMenu() { // This starts a function to show a menu of options.
    int choice; // This will store the user’s menu selection.
    
    printf("\n================ FILE TRANSFER MENU ================\n"); // This prints the menu title.
    printf("1. Upload a file to server\n"); // This shows option 1.
    printf("2. Download a file from server\n"); // This shows option 2.
    printf("3. Remove a file from server\n"); // This shows option 3.
    printf("4. Download tar file by type (.c, .txt, .pdf)\n"); // This shows option 4.
    printf("5. List files in directory\n"); // This shows option 5.
    printf("0. Exit\n"); // This shows option 0 to quit.
    printf("===================================================\n"); // This prints a closing line.
    printf("Enter your choice: "); // This asks the user to pick an option.
    
    if (scanf("%d", &choice) != 1) { // This reads the user’s choice and checks if it’s a number.
        while (getchar() != '\n'); // Clear input buffer // This clears any bad input from the keyboard.
        return -1; // This returns an error if the input wasn’t a number.
    }
    getchar(); // Clear newline // This removes the enter key from the input.
    
    return choice; // This returns the user’s choice.
}

// Add this function to create a new socket connection
int connectToServer() { // This starts a function to connect to the server.
    int sock = 0; // This will hold the socket number for the connection.
    struct sockaddr_in serv_addr; // This will store the server’s address details.
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { // This creates a socket for networking.
        perror("Socket creation error"); // If it fails, this prints an error.
        return -1; // This stops the function with an error code.
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr)); // This clears the server address structure.
    serv_addr.sin_family = AF_INET; // This sets the address type to internet (IPv4).
    serv_addr.sin_port = htons(SERVER_PORT); // This sets the port number, converting it for networking.
    
    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) { // This converts the IP address to a usable format.
        printf("Invalid address/ Address not supported\n"); // If it fails, this prints an error.
        close(sock); // This closes the socket.
        return -1; // This stops the function with an error code.
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) { // This tries to connect to the server.
        perror("Connection Failed"); // If it fails, this prints an error.
        close(sock); // This closes the socket.
        return -1; // This stops the function with an error code.
    }
    
    return sock; // This returns the socket number if the connection worked.
}

// Modify your main function
int main() { // This starts the main program.
    int sock = -1; // This will hold the socket number, starting with an invalid value.
    int choice; // This will store the user’s menu choice.
    char local_path[MAX_PATH_LENGTH]; // This will hold the path to a local file.
    char server_path[MAX_PATH_LENGTH]; // This will hold the path to a server file.
    char filetype[10]; // This will hold the file type for tar downloads.
    
    // Create initial connection
    sock = connectToServer(); // This tries to connect to the server.
    if (sock < 0) { // This checks if the connection failed.
        return -1; // This stops the program with an error code.
    }
    
    printf("Connected to file server at %s:%d\n", SERVER_IP, SERVER_PORT); // This prints that the connection worked.
    
    while (1) { // This starts an infinite loop to keep showing the menu.
        choice = displayMenu(); // This shows the menu and gets the user’s choice.
        if (choice == -1) { // This checks if the input was invalid.
            printf("Invalid input. Please enter a number.\n"); // This prints an error message.
            continue; // This goes back to the start of the loop.
        }
        
        // Close previous connection and create a new one for each operation
        close(sock); // This closes the current connection.
        sock = connectToServer(); // This tries to make a new connection.
        if (sock < 0) { // This checks if the connection failed.
            printf("Failed to reconnect to server. Retrying...\n"); // This prints that it’s retrying.
            sock = connectToServer(); // This tries connecting again.
            if (sock < 0) { // This checks if the retry failed.
                printf("Connection failed. Exiting.\n"); // This prints that the program is giving up.
                return -1; // This stops the program with an error code.
            }
        }
        
        switch (choice) { // This checks the user’s menu choice.
            case 1: // This runs if the user chose to upload a file.
                printf("Enter local file path: "); // This asks for the file to upload.
                if (!fgets(local_path, MAX_PATH_LENGTH, stdin)) { // This reads the file path.
                    printf("Error reading local path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                local_path[strcspn(local_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                printf("Enter server directory path (e.g., ~/S1/docs): "); // This asks where to put the file on the server.
                if (!fgets(server_path, MAX_PATH_LENGTH, stdin)) { // This reads the server path.
                    printf("Error reading server path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                server_path[strcspn(server_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                uploadFile(sock, local_path, server_path); // This tries to upload the file.
                break; // This exits the switch.
            case 2: // This runs if the user chose to download a file.
                printf("Enter server file path (e.g., ~/S1/docs/file.txt): "); // This asks for the file on the server.
                if (!fgets(server_path, MAX_PATH_LENGTH, stdin)) { // This reads the server path.
                    printf("Error reading server path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                server_path[strcspn(server_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                printf("Enter local file path to save: "); // This asks where to save the file locally.
                if (!fgets(local_path, MAX_PATH_LENGTH, stdin)) { // This reads the local path.
                    printf("Error reading local path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                local_path[strcspn(local_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                downloadFile(sock, server_path, local_path); // This tries to download the file.
                break; // This exits the switch.
                
            case 3: // This runs if the user chose to delete a file.
                printf("Enter server file path to remove (e.g., ~/S1/docs/file.txt): "); // This asks for the file to delete.
                if (!fgets(server_path, MAX_PATH_LENGTH, stdin)) { // This reads the server path.
                    printf("Error reading server path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                server_path[strcspn(server_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                removeFile(sock, server_path); // This tries to delete the file.
                break; // This exits the switch.
                
            case 4: // This runs if the user chose to download a tar file.
                printf("Enter file type to download (c, txt, pdf): "); // This asks for the file type.
                if (!fgets(filetype, sizeof(filetype), stdin)) { // This reads the file type.
                    printf("Error reading file type\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                filetype[strcspn(filetype, "\n")] = '\0'; // This removes the enter key from the type.
                
                printf("Enter local path to save tar file: "); // This asks where to save the tar file.
                if (!fgets(local_path, MAX_PATH_LENGTH, stdin)) { // This reads the local path.
                    printf("Error reading local path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                local_path[strcspn(local_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                downloadTar(sock, filetype, local_path); // This tries to download the tar file.
                break; // This exits the switch.
                
            case 5: // This runs if the user chose to list files.
                printf("Enter server directory path (e.g., ~/S1/docs): "); // This asks for the directory to list.
                if (!fgets(server_path, MAX_PATH_LENGTH, stdin)) { // This reads the server path.
                    printf("Error reading server path\n"); // If it fails, this prints an error.
                    continue; // This goes back to the menu.
                }
                server_path[strcspn(server_path, "\n")] = '\0'; // This removes the enter key from the path.
                
                listFiles(sock, server_path); // This tries to list the files.
                break; // This exits the switch.
                
            case 0: // This runs if the user chose to exit.
                printf("Exiting program. Goodbye!\n"); // This prints a goodbye message.
                close(sock); // This closes the connection.
                return 0; // This ends the program successfully.
                
            default: // This runs if the choice wasn’t recognized.
                printf("Invalid choice. Please try again.\n"); // This prints an error message.
                break; // This exits the switch.
        }
    }
    
    close(sock); // This closes the connection (though the loop means this is never reached).
    return 0; // This ends the program successfully (never reached due to the loop).
}
