/*******************************************************
  rebuild.c
 
  Usage:
    1) ./rebuild u
       (Root call: initializes visited array in done.txt)
 
    2) ./rebuild u child
       (Child call: does NOT initialize done.txt)
 
  Compile:
    gcc -Wall -o rebuild rebuild.c
 *******************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_DEPENDENCIES 100
#define MAX_LINE_LENGTH  256

// read_visited_file: Reads a single line from done.txt.
// That line has n characters, each '0' or '1', indexing 1..n.
void read_visited_file(char *filename, int *visited, int n) {
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        perror("fopen (read_visited_file)");
        exit(EXIT_FAILURE);
    }

    char buffer[256];
    if (!fgets(buffer, sizeof(buffer), fp)) {
        fprintf(stderr, "Error: Could not read visited line from %s\n", filename);
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    // Remove trailing newline if present
    int len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') {
        buffer[len - 1] = '\0';
        len--;
    }

    // Convert each character to 0 or 1
    for (int i = 1; i <= n; i++) {
        if (buffer[i - 1] == '0') {
            visited[i] = 0;
        } else if (buffer[i - 1] == '1') {
            visited[i] = 1;
        } else {
            fprintf(stderr, "Error: Invalid character '%c' in visited array\n", buffer[i - 1]);
            exit(EXIT_FAILURE);
        }
    }
}

// write_visited_file: Writes the visited array into a single line in done.txt,
// followed by one newline character.
void write_visited_file(char *filename, int *visited, int n) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("fopen (write_visited_file)");
        exit(EXIT_FAILURE);
    }

    // Write all bits side-by-side on one line
    for (int i = 1; i <= n; i++) {
        fprintf(fp, "%d", visited[i]);
    }
    // Then add a single newline at the end
    fprintf(fp, "\n");

    fclose(fp);
}

// This function reads "n" from foodep.txt (first line),
// then finds the line "u: ..." and extracts its dependencies into deps[].
// Returns the number of dependencies found for foodule u.
int read_dependencies_for_u(char *depfilename, int u, int *deps, int max_deps) {
    FILE *fp = fopen(depfilename, "r");
    if (!fp) {
        perror("fopen foodep.txt");
        exit(EXIT_FAILURE);
    }

    int n;
    if (fscanf(fp, "%d", &n) != 1) {
        fprintf(stderr, "Could not read n from foodep.txt\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }

    // Skip leftover from the line where n was read
    char line[MAX_LINE_LENGTH];
    fgets(line, sizeof(line), fp);

   
    int numDeps = 0;

    while (fgets(line, sizeof(line), fp)) {
        // We expect lines of the form: "x: v1 v2 v3..."
        char *colonPos = strchr(line, ':');
        if (!colonPos) {
            // If there's no colon, it's malformed or an empty dep line
            continue;
        }

        *colonPos = '\0'; // Split at the colon
        int x = atoi(line);
        if (x == u) {
            // We found the dependencies for our foodule u
            char *rest = colonPos + 1; // the substring after the colon
            char *token = strtok(rest, " \t\n");
            while (token) {
                if (numDeps >= max_deps) {
                    fprintf(stderr, "Too many dependencies for %d\n", u);
                    fclose(fp);
                    exit(EXIT_FAILURE);
                }
                deps[numDeps++] = atoi(token);
                token = strtok(NULL, " \t\n");
            }
            break;
        }
    }

    fclose(fp);
    return numDeps;  // If not found, 0 => no dependencies
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <foodule> [child]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Which foodule do we rebuild?
    int u = atoi(argv[1]);

    // Read total number of foodules (n) from foodep.txt
    FILE *fp = fopen("foodep.txt", "r");
    if (!fp) {
        perror("fopen foodep.txt");
        exit(EXIT_FAILURE);
    }
    int n;
    if (fscanf(fp, "%d", &n) != 1) {
        fprintf(stderr, "Could not read n from foodep.txt\n");
        fclose(fp);
        exit(EXIT_FAILURE);
    }
    fclose(fp);

    // We'll store visited bits in visited[1..n].
    int *visited = (int *)calloc(n + 1, sizeof(int));
    if (!visited) {
        perror("calloc visited");
        exit(EXIT_FAILURE);
    }

    // If this is the "root" call (argc == 2), we initialize done.txt to all zeros.
    if (argc == 2) {
        for (int i = 1; i <= n; i++) {
            visited[i] = 0;
        }
        write_visited_file("done.txt", visited, n);
    }

    // Read the direct dependencies for u
    int deps[MAX_DEPENDENCIES];
    int numDeps = read_dependencies_for_u("foodep.txt", u, deps, MAX_DEPENDENCIES);

    // For each dependency v, if it isn't visited, fork+exec rebuild v
    for (int i = 0; i < numDeps; i++) {
        int v = deps[i];
        // Re-read visited file each time
        read_visited_file("done.txt", visited, n);

        if (visited[v] == 0) {
            pid_t pid = fork();
            if (pid == 0) {
                // child process
                char v_str[32];
                snprintf(v_str, sizeof(v_str), "%d", v);

                // We'll pass "child" as 2nd arg so child doesn't re-init done.txt
                char *newargv[] = {"./rebuild", v_str, "child", NULL};
                execvp(newargv[0], newargv);
                // If execvp returns, there's an error
                printf("execvp");
                _exit(EXIT_FAILURE);
            } else {
                // parent process waits
                int status;
                waitpid(pid, &status, 0);
            }
        }
    }

 
    read_visited_file("done.txt", visited, n);
    visited[u] = 1;
    write_visited_file("done.txt", visited, n);

  
    if (numDeps == 0) {
        // no dependencies
        printf("foo%d rebuilt\n", u);
    } else {
        printf("foo%d rebuilt from foo%d", u, deps[0]);
        for (int i = 1; i < numDeps; i++) {
            printf(", foo%d", deps[i]);
        }
        printf("\n");
    }

    free(visited);
    return 0;
}

