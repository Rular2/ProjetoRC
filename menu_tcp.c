#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>


#define MAX_USERS 100
#define MAX_USERNAME_LENGTH 50
#define MAX_PASSWORD_LENGTH 50
#define MAX_LEN 100 // Maximum length of a line in the file
#define DATABASE_FILE "credentials.txt" // File names: credentials.txt; blocked_%username.txt; access_%username.txt; %conv.txt; 
#define PORT 8080

// Function prototypes
void mainMenu();
void login_menu();
void subMenu1();
void subMenu2();
void subMenu3();
void subMenu4();
void login();
void registerUser();
void newConversation(char title[], char participants[][50], int numParticipants);

void remove_blocked(const char *username);
void block_user (const char *username);
void check_blocked(const char *username);

int checkCredentials(const char *username, const char *password);
void addUserToFile(const char *username, const char *password);

// Structure to store user information
typedef struct {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];
} User;

// Array to store online users
User users[MAX_USERS];
int numUsers = 0;


int main() {
	int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = "127.0.0.1";
    address.sin_port = htons(PORT);

    // Forcefully attaching socket to the port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    while (1) {
        printf("Waiting for connection...\n");
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }
        printf("Connection established.\n");

        // Create child process
        if (fork() == 0) {
            // Inside child process
            close(server_fd);
            // Handle communication with client
            login_menu(new_socket);
            close(new_socket);
            exit(0);
        } else {
            // Inside parent process
            close(new_socket);
        }
    }

    return 0;
}

void login_menu(int client_socket) {
    int choice;

    char welcome_message[] = "===== Welcome to User Authentication System =====\n";
    char menu_options[] = "1. Login\n2. Register\n===============================================\n";
    char invalid_choice[] = "Invalid choice! Please try again.\n";
    char choice_prompt[] = "Enter your choice: ";

    // Send welcome message and menu options to the client
    send(client_socket, welcome_message, strlen(welcome_message), 0);
    send(client_socket, menu_options, strlen(menu_options), 0);

    do {
        // Prompt the client for choice
        send(client_socket, choice_prompt, strlen(choice_prompt), 0);

        // Receive choice from client
        recv(client_socket, &choice, sizeof(choice), 0);

        // Convert choice from network byte order to host byte order
        choice = ntohl(choice);

        switch (choice) {
            case 1:
                // Send acknowledgment to the client
                send(client_socket, "Login selected.\n", strlen("Login selected.\n"), 0);
                // Call login function
                //login();
                break;
            case 2:
                // Send acknowledgment to the client
                send(client_socket, "Register selected.\n", strlen("Register selected.\n"), 0);
                // Call register function
                //registerUser();
                break;
            default:
                // Send error message to the client
                send(client_socket, invalid_choice, strlen(invalid_choice), 0);
                break;
        }
    } while (choice != 1 && choice != 2);
}
