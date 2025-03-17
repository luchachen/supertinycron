#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include "ccronexpr.h"

//#define SCHEDULED_RESTART_CMD "/usr/bin/restart"
#define SCHEDULED_RESTART_CMD "logger \"scheduled-restart task exec\" && /usr/sbin/reboot"
//Crontab directory
#ifdef __NTOS__
// host linux
#define CRON_DIR_ARG "-c /etc/crontab/"
#else
#define CRON_DIR_ARG ""
#endif

bool is_gateway_series() {
    // Implement this function based on ProductSeries().is_gateway_series
    return false; // Placeholder implementation
}

bool is_gateway_sub_idp_series() {
    // Implement this function based on ProductSeries().is_gateway_sub_idp_series
    return false; // Placeholder implementation
}

void reset_factory_settings() {
    const char *hostname;

    if (is_gateway_series()) {
        hostname = "Ruijie";
    } else if (is_gateway_sub_idp_series()) {
        hostname = "idp";
    } else {
        hostname = "firewall";
    }

    // Change hostname using sethostname
    if (sethostname(hostname, strlen(hostname)) != 0) {
        perror("Failed to set hostname");
    }

    // Remove scheduled restart cron jobs
    system("crontab " CRON_DIR_ARG " -l |" 
           "grep -v '" SCHEDULED_RESTART_CMD "' |"
           "crontab " CRON_DIR_ARG " -");
    printf("Scheduled restart reset to factory settings.\n");
}

// Function to apply scheduled restart configuration
void scheduled_restart_apply(int enabled, int hour, int minute, const char *weekdays[], int weekday_count) {
    char cron_expr_str[256] = {0};
    char weekday_str[64] = {0};
    int i;

    // Build the weekday part of the cron expression
    if (weekday_count == 0) {
        strcpy(weekday_str, "*"); // All days
    } else {
        for (i = 0; i < weekday_count; i++) {
            strcat(weekday_str, weekdays[i]);
            if (i < weekday_count - 1) {
                strcat(weekday_str, ",");
            }
        }
    }

    // Construct the cron expression
    snprintf(cron_expr_str, sizeof(cron_expr_str), "%d %d * * %s", minute, hour, weekday_str);

    // Log the configuration
    printf("Scheduled restart config: enabled=%d, cron_expr=%s\n", enabled, cron_expr_str);

    // If enabled, schedule the restart
    if (enabled) {
        // Parse the cron expression
        cron_expr expr;
        const char *err = NULL;
        memset(&expr, 0, sizeof(expr));
        cron_parse_expr(cron_expr_str, &expr, &err);

        if (err) {
            fprintf(stderr, "Error parsing cron expression: %s\n", err);
            return;
        }

        // Calculate the next execution time
        time_t now = time(NULL);
        time_t next = cron_next(&expr, now);

        // Log the next execution time
        char next_time_str[64];
        strftime(next_time_str, sizeof(next_time_str), "%Y-%m-%d %H:%M:%S", localtime(&next));
        printf("Next scheduled restart: %s\n", next_time_str);

        // Add the cron job to the system's crontab
        char command[512];
        snprintf(command, sizeof(command), "(crontab " CRON_DIR_ARG " -l; echo \"%s %s\") |"
               "crontab " CRON_DIR_ARG " -", cron_expr_str, SCHEDULED_RESTART_CMD);
        system(command);
    } else {
        // Remove the scheduled restart command from the crontab
        system("crontab " CRON_DIR_ARG " -l | grep -v '" SCHEDULED_RESTART_CMD "' |"
               "crontab " CRON_DIR_ARG " -");
        printf("Scheduled restart disabled. Removed from crontab.\n");
    }
}

// Function to get the state of the scheduled restart
int get_state_scheduled_restart() {
    FILE *pipe = popen("crontab " CRON_DIR_ARG " -l | grep '" SCHEDULED_RESTART_CMD "'", "r");
    if (!pipe) {
        perror("Failed to run crontab command");
        return 0;
    }

    char buffer[128];
    int running = 0;
    if (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        running = 1; // Command exists in crontab
    }
    // Get actual command exit code
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        int exit_code = WEXITSTATUS(status);
        if (exit_code == 0) {
            running = 1;    // Command succeeded (match found)
        } else if (exit_code == 1) {
            running = 0;    // No match found
        } else {
            fprintf(stderr, "Command failed with exit code: %d\n", exit_code);
            return 0;
        }
    } else {
        perror("Command did not exit normally");
        return 0;
    }

    printf("Scheduled restart running: %d\n", running);
    return running;
}

int main() {
    // Example usage of the functions
    reset_factory_settings();

    const char *weekdays[] = {"mon", "wed", "fri"};
    scheduled_restart_apply(1, 3, 0, weekdays, 3); // Enable restart at 3:00 AM on Mon, Wed, Fri

    int state = get_state_scheduled_restart();
    printf("Scheduled restart state: %d\n", state);

    scheduled_restart_apply(0, 0, 0, NULL, 0); // Disable restart

    return 0;
}
