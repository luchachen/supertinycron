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
#define CRON_FILE_PATH "/etc/crontabs/root"
#define CRON_UPDATE_FILE_PATH "/etc/crontabs/cron.update"
#else
#define CRON_FILE_PATH "/etc/crontabs/root"
#define CRON_UPDATE_FILE_PATH "/etc/crontabs/cron.update"
#endif

bool is_gateway_series() {
    // Implement this function based on ProductSeries().is_gateway_series
    return false; // Placeholder implementation
}

bool is_gateway_sub_idp_series() {
    // Implement this function based on ProductSeries().is_gateway_sub_idp_series
    return false; // Placeholder implementation
}

void update_cron() {
    FILE *fp = fopen(CRON_UPDATE_FILE_PATH, "w");
    if (fp == NULL) {
        perror("Failed to open cron update file");
        return;
    }
    ftruncate(fileno(fp), 0);
    fclose(fp);
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
    FILE *fp = fopen(CRON_FILE_PATH, "r+");
    if (fp == NULL) {
        perror("Failed to open crontab file");
        return;
    }

    FILE *temp_fp = tmpfile();
    if (temp_fp == NULL) {
        perror("Failed to create temporary file");
        fclose(fp);
        return;
    }

    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, SCHEDULED_RESTART_CMD) == NULL) {
            fputs(line, temp_fp);
        } else {
            found = true;
        }
    }

    if (found) {
        fseek(fp, 0, SEEK_SET);
        fseek(temp_fp, 0, SEEK_SET);
        while (fgets(line, sizeof(line), temp_fp) != NULL) {
            fputs(line, fp);
        }

        ftruncate(fileno(fp), ftell(fp));
        update_cron();
    }
    fclose(fp);
    fclose(temp_fp);


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

    // Validate the cron expression
    cron_expr expr;
    const char *error = NULL;
    cron_parse_expr(cron_expr_str, &expr, &error);
    if (error) {
        printf("Invalid cron expression: %s\n", error);
        return;
    }

    FILE *fp = fopen(CRON_FILE_PATH, "r+");
    if (fp == NULL) {
        perror("Failed to open crontab file");
        return;
    }

    FILE *temp_fp = tmpfile();
    if (temp_fp == NULL) {
        perror("Failed to create temporary file");
        fclose(fp);
        return;
    }

    char line[256];
    bool found = false;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, SCHEDULED_RESTART_CMD) == NULL) {
            fputs(line, temp_fp);
        } else {
            found = true;
        }
    }

    if (enabled) {
        // Calculate the next execution time
        time_t now = time(NULL);
        time_t next = cron_next(&expr, now);

        // Log the next execution time
        char next_time_str[64];
        strftime(next_time_str, sizeof(next_time_str), "%Y-%m-%d %H:%M:%S", localtime(&next));
        printf("Next scheduled restart: %s\n", next_time_str);
        fprintf(temp_fp, "%s %s\n", cron_expr_str, SCHEDULED_RESTART_CMD);
    }

    if (enabled || found) {
        fseek(fp, 0, SEEK_SET);
        fseek(temp_fp, 0, SEEK_SET);
        while (fgets(line, sizeof(line), temp_fp) != NULL) {
            fputs(line, fp);
        }

        ftruncate(fileno(fp), ftell(fp));
        update_cron();
    }

    fclose(fp);
    fclose(temp_fp);

    if (enabled && !found) {
        printf("Scheduled restart added to crontab.\n");
    } else if (!enabled && found) {
        printf("Scheduled restart removed from crontab.\n");
    } else {
        printf("Scheduled restart configuration updated.\n");
    }
}

// Function to get the state of the scheduled restart
int get_state_scheduled_restart() {
    FILE *fp = fopen(CRON_FILE_PATH, "r");
    if (fp == NULL) {
        perror("Failed to open crontab file");
        return 0;
    }

    char line[256];
    int running = 0;
    while (fgets(line, sizeof(line), fp) != NULL) {
        if (strstr(line, SCHEDULED_RESTART_CMD) != NULL) {
            running = 1; // Command exists in crontab
            break;
        }
    }

    fclose(fp);

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
