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
	login_menu();
return 0;
}

void login_menu() {
	int choice;

	printf("\n===== Welcome to User Authentication System =====\n");
    printf("1. Login\n");
    printf("2. Register\n");
    printf("===============================================\n");
	
    do {
        printf("Enter your choice: ");
        scanf("%d", &choice);
        getchar(); // Consume newline character

        switch (choice) {
            case 1:
                login();
                break;
            case 2:
                registerUser();
                break;
            default:
                printf("Invalid choice! Please try again.\n");
                break;
        }
    } while (choice != 1 && choice != 2);
}


void mainMenu() {
    int choice;

    do {
        // Display main menu options
        printf("\n=== Main Menu ===\n");
        printf("1. Ver conversas\n");
        printf("2. Iniciar conversa \n");
        printf("3. Ver usuários online \n");
        printf("4. Bloquear/Desbloquear usuário\n");
        printf("5. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        // Handle user choice
        switch(choice) {
            case 1:
                subMenu1();
                break;
            case 2:
                subMenu2();
                break;
			case 3:
				subMenu3();
				break;
			case 4:
				subMenu4();
				break;
			case 5:
                printf("Exiting program. Goodbye!\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while(choice != 5);
}

void subMenu1() { //Ver conversas
	
    // this menu prints all the conversations to which the user has acess to (access function
    // then, after the right conversation selected, the .txt file is read on the screen;
    
    int choice;

    do {
        // Display submenu 1 options
        printf("\n=== Submenu 1 ===\n");
        printf("1. Ver conversas\n");
        printf("2. Option 2\n");
        printf("3. Go back to Main Menu\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        // Handle user choice
        switch(choice) {
            case 1:
                printf("You selected Option 1 in Submenu 1.\n");
                break;
            case 2:
                printf("You selected Option 2 in Submenu 1.\n");
                break;
            case 3:
                printf("Returning to Main Menu.\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while(choice != 3);
}

void subMenu2() {

	char title[100];
    char participants[10][50]; // Allowing up to 10 participants, with names up to 50 characters each
    int numParticipants;
	
	flush_input_buffer();
    // Prompt the user for the title of the conversation
    printf("Enter the title of the conversation: ");
    fgets(title, sizeof(title), stdin);
    // Remove the newline character from the title
    title[strcspn(title, "\n")] = '\0';

    // Prompt the user for the number of participants
    printf("Enter the number of participants: ");
    scanf("%d", &numParticipants);
    getchar(); // Clearing the input buffer

    // Prompt the user for the names of the participants
    for (int i = 0; i < numParticipants; ++i) {
        printf("Enter participant %d name: ", i + 1);
        fgets(participants[i], sizeof(participants[i]), stdin);
        // Remove the newline character from the participant's name
        participants[i][strcspn(participants[i], "\n")] = '\0';
    }

    // Call the newConversation function with the provided title and participants
    newConversation(title, participants, numParticipants);
}

void subMenu3() {
	
	// use check() on servidor.c to see online users
	
    int choice;
	flush_input_buffer();
    do {
        // Display submenu 1 options
        printf("\n=== Submenu 1 ===\n");
        printf("1. Ver usuários online\n");
        printf("2. Option 2\n");
        printf("3. Go back to Main Menu\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        // Handle user choice
        switch(choice) {
            case 1:
                printf("You selected Option 1 in Submenu 1.\n");
                 printUsers();
                break;
            case 2:
                printf("You selected Option 2 in Submenu 1.\n");
                break;
            case 3:
                printf("Returning to Main Menu.\n");
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while(choice != 3);
}

void subMenu4() {
    int choice;
	//use functions check_blocked,block_user and remove_blocked to manage blocked users
	// falta retorno para o menu principal
	
	flush_input_buffer();
    do {
        // Display submenu 1 options
        printf("\n=== Submenu 4 ===\n");
        printf("1. Check blocked\n");
        printf("2. Block user\n");
        printf("3. Remove blocked\n");
        printf("4. Return to main menu\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);

        // Handle user choice
        switch(choice) {
            case 1:
                printf("You selected Option 1 in Submenu 4.\n");
                check_blocked(users[0].username);
                break;
            case 2:
                printf("You selected Option 2 in Submenu 4.\n");
                flush_input_buffer();
                block_user(users[0].username);
                break;
            case 3:
                printf("Returning to Main Menu.\n");
                remove_blocked(users[0].username);
                break;
            default:
                printf("Invalid choice. Please try again.\n");
        }
    } while(choice != 4); 
}


// Function to handle user login
void login() {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];

    printf("\nEnter username: ");
    fgets(username, MAX_USERNAME_LENGTH, stdin);
    username[strcspn(username, "\n")] = 0; // Remove trailing newline

    printf("Enter password: ");
    fgets(password, MAX_PASSWORD_LENGTH, stdin);
    password[strcspn(password, "\n")] = 0; // Remove trailing newline
	
	
    if (checkCredentials(username, password)) {
		addUser(username,password);
        printf("Login successful!\n");
        mainMenu();
    } else {
        printf("Invalid username or password. Please try again.\n");
        login_menu();
    }
}

// Function to check user credentials
int checkCredentials(const char *username, const char *password) {
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file == NULL) {
        printf("Error opening file!\n");
        return 0; // Authentication fails
    }

    char buffer[MAX_USERNAME_LENGTH + MAX_PASSWORD_LENGTH + 2]; // +2 for space and null terminator
    while (fgets(buffer, sizeof(buffer), file) != EOF) {
        char storedUsername[MAX_USERNAME_LENGTH];
        char storedPassword[MAX_PASSWORD_LENGTH];

        sscanf(buffer, "%s %s", storedUsername, storedPassword);

        if (strcmp(username, storedUsername) == 0 && strcmp(password, storedPassword) == 0) {
            fclose(file);
            return 1; // Authentication succeeds
        }
    }

    fclose(file);
    return 0; // Authentication fails
}

void addUser(const char *username, const char *password) {    
    // Check if the number of users exceeds the maximum limit
    if (numUsers >= MAX_USERS) {
        printf("Cannot add more users. Maximum limit reached.\n");
        return;
    }
    
	// Add the user to the array
	strcpy(users[numUsers].username, username);
	strcpy(users[numUsers].password, password);
	numUsers++;
	printf("User added to the registry of online users.\n");
}


// Function to handle user registration
void registerUser() {
    char username[MAX_USERNAME_LENGTH];
    char password[MAX_PASSWORD_LENGTH];

    printf("\nEnter new username: ");
    fgets(username, MAX_USERNAME_LENGTH, stdin);
    username[strcspn(username, "\n")] = 0; // Remove trailing newline

    printf("Enter new password: ");
    fgets(password, MAX_PASSWORD_LENGTH, stdin);
    password[strcspn(password, "\n")] = 0; // Remove trailing newline
	
	//flush the input buffer
	flush_input_buffer();
	
    // Add user to the file
    addUserToFile(username, password);
    
    FILE *file;
    char blockedFileName[MAX_USERNAME_LENGTH + 20]; // Assuming maximum length of username is 50 characters
    char accessFileName[MAX_USERNAME_LENGTH + 20]; // Assuming maximum length of username is 50 characters

    // Creating blocked file name using snprintf to prevent buffer overflow
    snprintf(blockedFileName, sizeof(blockedFileName), "blocked_%s.txt", username);
    // Creating access file name
    snprintf(accessFileName, sizeof(accessFileName), "access_%s.txt", username);

    // Creating and opening blocked file
    file = fopen(blockedFileName, "w");
    if (file != NULL) {
        fclose(file);
    } else {
        printf("Error creating blocked file for user: %s\n", username);
    }

    // Creating and opening access file
    file = fopen(accessFileName, "w");
    if (file != NULL) {
        fclose(file);
    } else {
        printf("Error creating access file for user: %s\n", username);
    }

    printf("Registration successful!\n");
}



// Function to add a new user to the file
void addUserToFile(const char *username, const char *password) {
	 if (usernameExists(username)) {
        printf("Username already exists!\n");
        login_menu();
        return;
    }
	
    FILE *file = fopen(DATABASE_FILE, "a");
    if (file == NULL) {
        printf("Error opening file!\n");
        return;
    }

    fprintf(file, "%s %s\n", username, password);
    fclose(file);
}

// Function to check if a username already exists in the file
int usernameExists(const char *username) {
    FILE *file = fopen(DATABASE_FILE, "r");
    if (file == NULL) {
        printf("Error opening file!\n");
        exit(EXIT_FAILURE);
    }

    char buffer[100]; // Assuming a maximum username length of 100 characters
    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        char *storedUsername = strtok(buffer, " ");
        if (strcmp(storedUsername, username) == 0) {
            fclose(file);
            return 1; // Username already exists
        }
    }

	flush_input_buffer();
    fclose(file);
    return 0; // Username doesn't exist
}


void newConversation(char title[], char participants[][50], int numParticipants) {
	printf("New conversation executed\n");
	mainMenu();
}

void check (const char *username){ //check user status
	printf("Function executed\n");
}

void printUsers() {
    printf("List of users online:\n");
    printf("----------------------------\n");
    
    // Iterate through the array of users
    for (int i = 0; i < numUsers; i++) {
        printf("User %d:\n", i + 1);
        printf("Username: %s\n", users[i].username);
        printf("----------------------------\n");
    }
}

/*void access (const char *username, const char *conv) {
	//cada usuario tem um ficheiro com uma lista das conversas a que este tem acesso	
}*/

//void check_status (char username[]) {} nota: é possível que isto vá para uma array no ficheiro dos menus

void block_user(const char *username) {
	 char filename[MAX_USERNAME_LENGTH+50];
	 char blocked_user[100];
	 
	 // Create filename based on username
    snprintf(filename, sizeof(filename), "blocked_%s.txt", username);
	 
    // Open the file in append mode
    FILE *file = fopen(filename, "a");
    if (file == NULL) {
        perror("Error opening file");
        return;
    }
	
	printf("Enter the username to be blocked: ");
	fgets(blocked_user, MAX_USERNAME_LENGTH, stdin);

    
    flush_input_buffer();
	
    // Write the username to the file
    fprintf(file, "%s\n", blocked_user);
    
    printf("User '%s' has been blocked.\n", blocked_user);

    // Close the file
    fclose(file);
}

void check_blocked(const char *username) {
	// Construct the file name based on the username
    char filename[100]; // Adjust size as needed
    sprintf(filename, "blocked_%s.txt", username);

    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }

    // Read and process each line (username) in the file
    char line[100]; // Adjust size as needed
    while (fgets(line, sizeof(line), file)) {
        // Remove trailing newline character, if present
        if (line[strlen(line) - 1] == '\n') {
            line[strlen(line) - 1] = '\0';
        }
        // print it
        printf("Blocked username: %s\n", line);
    }

    // Close the file
    fclose(file);
}

void remove_blocked(const char *username) {
	// Define the filename
    char filename[100]; // Adjust size as needed
    char blocked_username[MAX_USERNAME_LENGTH];
    
	printf("Enter the username to remove from the file: ");
    scanf("%s", blocked_username);

    sprintf(filename, "blocked_%s.txt", blocked_username);

    // Open the file in read mode
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        printf("Error opening file.\n");
        return;
    }

    // Create a temporary file for writing
    FILE *tempFile = fopen("temp.txt", "w");
    if (tempFile == NULL) {
        printf("Error creating temporary file.\n");
        fclose(file);
        return;
    }

    char line[256]; 
    int found = 0; // Flag to track if username is found

    // Read each line of the file
    while (fgets(line, sizeof(line), file)) {
        // Check if the line contains the username
        if (strstr(line, blocked_username) == NULL) {
            // If not, write the line to the temporary file
            fputs(line, tempFile);
        } else {
            found = 1; // Set flag to indicate username is found
        }
    }

    // Close both files
    fclose(file);
    fclose(tempFile);

    // Delete the original file
    remove(filename);

    // Rename the temporary file to the original filename
    rename("temp.txt", filename);

    // Print message based on whether username was found and removed
    if (found) {
        printf("Username '%s' removed from the file.\n", blocked_username);
    } else {
        printf("Username '%s' not found in the file.\n", blocked_username);
    }
}


// Function to flush the input buffer
void flush_input_buffer() {
   fflush(stdin);
}
