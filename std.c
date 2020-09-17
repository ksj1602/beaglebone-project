#ifdef DUMMY
#define MRAA_GPIO_IN 0
#define MRAA_GPIO_EDGE_RISING 0
typedef int mraa_aio_context;
typedef int mraa_gpio_context;
typedef int mraa_gpio_edge_t;

int mraa_aio_read(__attribute__((unused)) mraa_aio_context c)
{
    return 400;
}

void mraa_aio_close(__attribute__((unused)) mraa_aio_context c)
{
    return;
}

void mraa_gpio_isr(__attribute__((unused)) mraa_gpio_context c, __attribute__((unused)) mraa_gpio_edge_t e, __attribute__((unused)) void(*fptr)(void*), __attribute__((unused)) void* args)
{
    return;
}

void mraa_gpio_close(__attribute__((unused)) mraa_gpio_context c)
{
    return;
}

mraa_aio_context mraa_aio_init(__attribute__((unused)) int a)
{
    return a;
}

mraa_gpio_context mraa_gpio_init(__attribute__((unused)) int a)
{
    return a;
}

void mraa_gpio_dir(__attribute__((unused)) mraa_gpio_context a, __attribute__((unused)) int b)
{
    return;
}

#else
#include <mraa.h>
#include <mraa/aio.h>
#include <mraa/gpio.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <fcntl.h>
#include <poll.h>
#include <getopt.h>
#include <math.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define B 4275
#define R0 100000.0
#define BUFFERSIZE 1024

static struct option sensor_options[] = {
    {"period", required_argument, NULL, 'p'},
    {"scale", required_argument, NULL, 's'},
    {"log", required_argument, NULL, 'l'},
    {0, 0, 0, 0}
};

int cp = 0;
FILE* log_file = NULL;
long period = 1;
char scale = 'F', stdin_buffer[BUFFERSIZE], command[BUFFERSIZE + 1];
char *time_string = NULL, *logfile_name = NULL, *log_string = NULL;
bool logging_enabled = false, stop = false;
mraa_aio_context temperature;
mraa_gpio_context button;

float get_temperature(int reading)
{
    float R = 1023.0/((float) reading) - 1.0;
	R = R0 * R;
	
	float C = 1.0 / (log(R/R0)/B + 1/298.15) - 273.15;

    if (scale == 'C')
        return C;
    else
    {
        float F = (C * 9)/5 + 32;
        return F;
    }
}

void internal_program_error()
{
    fprintf(stderr, "Internal program error\n");
    exit(2);
}

void update_time_string(struct timespec time_now)
{
    memset(time_string, 0, 15 * sizeof(char));   
    struct tm *time_value;
    time_value = localtime(&(time_now.tv_sec));
    sprintf(time_string, "%.2d:%.2d:%.2d", time_value->tm_hour, time_value->tm_min, time_value->tm_sec);
}

void button_pressed()
{
    struct timespec time_now;
    clock_gettime(CLOCK_REALTIME, &time_now);
    update_time_string(time_now);
    printf("%s SHUTDOWN\n", time_string);
    if (logging_enabled)
    {
        fprintf(log_file, "%s SHUTDOWN\n", time_string);
    }
    mraa_gpio_close(button);
    mraa_aio_close(temperature);
    free(logfile_name);
    free(log_string);
    free(time_string);
    exit(0);
}

int time_elapsed(struct timespec from, struct timespec to)
{
    return to.tv_sec - from.tv_sec;
}

void parse_commands()
{
    char *ptr = stdin_buffer;
    while(*ptr)
    {
        if (*ptr != '\n' && cp < 1025)
        {
            command[cp] = *ptr;
            cp++;
            if (cp == 1025) // Cannot process a command that is too long
            {
                memset(command, 0, (BUFFERSIZE + 1) * sizeof(char));
                cp = 0;
            }
        }
        else
        {
            if (!strcmp("SCALE=F", command))
            {
                scale = 'F';
            }
            else if (!strcmp("SCALE=C", command))
            {
                scale = 'C';
            }
            else if (!strcmp("STOP", command))
            {
                stop = true;
            }
            else if (!strcmp("START", command))
            {
                stop = false;
            }
            else if (!strcmp("OFF", command))
            {
                if (logging_enabled)
                {
                    fprintf(log_file, "%s\n", command);
                }
                else
                    printf("%s\n", command);
                button_pressed();
            }
            else if (!strncmp("PERIOD=", command, 7))
            {
                long new_period = strtol(command + 7, NULL, 10);
                if (new_period > 0)
                {
                    period = new_period;
                }
            }
            else if (!strncmp("LOG ", command, 4))
            {
                if (logging_enabled)
                {
                    fprintf(log_file, "%s\n", command);
                }
                
            }
            
            // log command to log file whether it is valid or not
            if (logging_enabled)
            {
                fprintf(log_file, "%s\n", command);
            }
            else
                printf("%s\n", command);
            
            // Clear the command buffer
            memset(command, 0, (BUFFERSIZE + 1) * sizeof(char));
            cp = 0;
        }
        
        ptr++;
    }
    memset(stdin_buffer, 0, BUFFERSIZE * sizeof(char));
}

int main (int argc, char *argv[])
{
    // Variables for command line argument processing
    int opt;
    char *waste = NULL;
    
    // Process command line arguments
    while ((opt = getopt_long(argc, argv, "", sensor_options, NULL)) != -1)
    {
        switch (opt)
        {
        case 'p':
            period = strtol(optarg, &waste, 10);
            if (period == 0)
            {
                perror("Invalid period : ");
                exit(1);
            }
            else if (period < 0)
            {
                fprintf(stderr, "Invalid period: Must be greater than 0\n");
                exit(1);
            }
            break;

        case 's':
            scale = *optarg;
            if (scale != 'C' && scale != 'F')
            {
                fprintf(stderr, "Invalid scale: Must be C or F\n");
                exit(1);
            }
            break;

        case 'l':
            logging_enabled = true;
            logfile_name = (char *) calloc(strlen(optarg) + 1, sizeof(char));
            if (!logfile_name)
                internal_program_error();
            strncpy(logfile_name, optarg, strlen(optarg));
            log_file = fopen(logfile_name, "w");
            if (!log_file)
            {
                fprintf(stderr, "Could create logfile '%s': %s\n", logfile_name, strerror(errno));
                exit(1);
            }
            break;
        
        default:
            fprintf(stderr, "Incorrect command line arguments\n");
            exit(1);
            break;
        }
    }

    // Allocate space for time string
    time_string = (char*) calloc(15, sizeof(char));
    if (!time_string)
        internal_program_error();
    

    // Setup sensors
    int ret = 0;
    temperature = mraa_aio_init(0);
    button = mraa_gpio_init(73);
    mraa_gpio_dir(button, MRAA_GPIO_IN);
    mraa_gpio_isr(button, MRAA_GPIO_EDGE_RISING, button_pressed, NULL);

    // Miscellanous variables and processing
    int reading;
    float temperature_reading;
    memset(stdin_buffer, 0, BUFFERSIZE * sizeof(char));
    memset(command, 0, (BUFFERSIZE + 1) * sizeof(char));
    log_string = (char*) calloc(50, sizeof(char));
    if (!log_string)
        internal_program_error();
    

    // Setup nonblocking read and timer variables
    fcntl(0, F_SETFL, O_NONBLOCK);
    struct timespec curr_time, prev_time;
    clock_gettime(CLOCK_REALTIME, &curr_time);
    prev_time = curr_time;

    // Generating first report before the main loop begins
    update_time_string(curr_time);
    reading = mraa_aio_read(temperature);
    temperature_reading = get_temperature(reading);
    printf("%s %.1f\n", time_string, temperature_reading);
    if (logging_enabled)
    {
        fprintf(log_file, "%s %.1f\n", time_string, temperature_reading);
    }

    while (true)
    {
        clock_gettime(CLOCK_REALTIME, &curr_time);
        
        if ((time_elapsed(prev_time, curr_time) >= period) && !stop)
        {
            prev_time = curr_time;
            update_time_string(curr_time);
            reading = mraa_aio_read(temperature);
            temperature_reading = get_temperature(reading);
            printf("%s %.1f\n", time_string, temperature_reading);
            if (logging_enabled)
            {
                fprintf(log_file, "%s %.1f\n", time_string, temperature_reading);
            }
        }
        ret = read(0, stdin_buffer, BUFFERSIZE);
        if (ret > 0)
        {
            parse_commands();
        }
        else if (ret == -1 && errno != EAGAIN)
        {
            fprintf(stderr, "Error in reading from standard input\n");
            exit(1);
        }
        
    }

    mraa_gpio_close(button);
    mraa_aio_close(temperature);
    free(logfile_name);
    free(log_string);
    free(time_string);
	return 0;
}
