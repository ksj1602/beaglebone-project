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
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <openssl/ssl.h>
#include <openssl/evp.h>
#include <openssl/err.h>

#define B 4275
#define R0 100000.0
#define BUFFERSIZE 1024

static struct option sensor_options[] = {
    {"period", required_argument, NULL, 'p'},
    {"scale", required_argument, NULL, 's'},
    {"log", required_argument, NULL, 'l'},
    {"host", required_argument, NULL, 'h'},
    {0, 0, 0, 0}
};

int cp = 0;
FILE* log_file = NULL;
long period = 1;
char scale = 'F', socket_receive_buffer[BUFFERSIZE], command[BUFFERSIZE + 1];
char socket_send_buffer[BUFFERSIZE];
char *time_string = NULL, *logfile_name = NULL, *log_string = NULL;
bool logging_enabled = false, stop = false;
mraa_aio_context temperature;
mraa_gpio_context button;
long port_number = 0;
char *host_name = NULL;
int socket_fd = 0;
SSL_CTX* ssl_context = NULL;
SSL* ssl_client = NULL;
int ec = 0; // integer for error checking

void exit_with_code (char *error_msg, int exit_code)
{
    fprintf(stderr, "%s\n", error_msg);
    exit(exit_code);
}

void internal_program_error()
{
    exit_with_code("Internal program error", 2);
}

void Write(void* buffer, int bytes_to_be_written)
{
    int bytes_written = 0;
    int tmp = 0;
    while ((tmp = SSL_write(ssl_client, buffer + bytes_written, bytes_to_be_written - bytes_written)) && (bytes_written < bytes_to_be_written))
    {
        if (tmp < 0)
            exit_with_code("Error in writing to SSL server", 2);
        bytes_written += tmp;
    }
    memset(buffer, 0, bytes_to_be_written);
}

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

void update_time_string(struct timespec time_now)
{
    memset(time_string, 0, 15 * sizeof(char));   
    struct tm *time_value;
    time_value = localtime(&(time_now.tv_sec));
    sprintf(time_string, "%.2d:%.2d:%.2d", time_value->tm_hour, time_value->tm_min, time_value->tm_sec);
}

/*
    Code for three SSL functions
    Adapted from TA slides except for error-checking
*/

SSL_CTX* ssl_init (void)
{
    SSL_CTX* newContext = NULL;
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    newContext = SSL_CTX_new(TLSv1_client_method());
    if (!newContext)
        exit_with_code("Creation of SSL context failed", 2);
    return newContext;
}

SSL* attach_ssl_to_socket(int sckt, SSL_CTX* context)
{
    SSL* ssl_c = SSL_new(context);
    if (!ssl_c)
        exit_with_code("Creation of SSL client failed", 2);
    ec = SSL_set_fd(ssl_c, sckt);
    if (!ec)
        exit_with_code("Binding SSL client to socket failed", 2);
    ec = SSL_connect(ssl_c);
    if (ec <= 0)
        exit_with_code("SSL connection failed", 2);
    return ssl_c;
}

void ssl_clean_up(SSL* client)
{
    ec = SSL_shutdown(client);
    if (ec < 0)
        exit_with_code("SSL could not shutdown correctly", 2);
    SSL_free(client);
}

void button_pressed()
{
    struct timespec time_now;
    clock_gettime(CLOCK_REALTIME, &time_now);
    update_time_string(time_now);
    fprintf(log_file, "%s SHUTDOWN\n", time_string);
    mraa_gpio_close(button);
    mraa_aio_close(temperature);
    free(logfile_name);
    free(log_string);
    free(time_string);
    free(host_name);
    ssl_clean_up(ssl_client);
    exit(0);
}

int time_elapsed(struct timespec from, struct timespec to)
{
    return to.tv_sec - from.tv_sec;
}

void parse_commands()
{
    char *ptr = socket_receive_buffer;
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
                fprintf(log_file, "%s\n", command);
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
                fprintf(log_file, "%s\n", command);
            }
            
            // log command to log file whether it is valid or not
            fprintf(log_file, "%s\n", command);
            
            
            
            // Clear the command buffer
            memset(command, 0, (BUFFERSIZE + 1) * sizeof(char));
            cp = 0;
        }
        
        ptr++;
    }

    // After all commands have been processed, clear the input buffer
    memset(socket_receive_buffer, 0, BUFFERSIZE * sizeof(char));
}

// Code given in TA slides for Project 1B
int client_connect (char *hostname, unsigned int port)
{
    int error_checker;
    struct sockaddr_in server_address;
    struct hostent *server;
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server = gethostbyname(hostname);
    if (!server)
        exit_with_code("Invalid host", 1);
    memset(&server_address, 0, sizeof(struct sockaddr_in));
	server_address.sin_family = AF_INET;
	memcpy(&server_address.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
	server_address.sin_port = htons(port);
	error_checker = connect(sockfd, (struct sockaddr *) &server_address, sizeof(server_address));
    if (error_checker == -1)
        internal_program_error();
    return sockfd; 
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
                exit_with_code(strcat("Invalid period: ", strerror(errno)), 1);
            else if (period < 0)
                exit_with_code("Invalid period: Must be greater than 0", 1);
            break;

        case 's':
            scale = *optarg;
            if (scale != 'C' && scale != 'F')
                exit_with_code("Invalid scale: Must be C or F", 1);
            break;

        case 'l':
            logging_enabled = true;
            logfile_name = (char *) calloc(strlen(optarg) + 1, sizeof(char));
            if (!logfile_name)
                internal_program_error();
            strncpy(logfile_name, optarg, strlen(optarg));
            break;
        
        case 'h':
            host_name = (char *) calloc(strlen(optarg) + 1, sizeof(char));
            if (!host_name)
                internal_program_error();
            strncpy(host_name, optarg, strlen(optarg));
            break;

        default:
            exit_with_code("Incorrect command line arguments", 1);
            break;
        }
    }

    // Get port number and check it
    if (optind < argc)
    {
		port_number = strtol(argv[optind], &waste, 10);

		if (port_number <= 1024)
            exit_with_code("Invalid port number entered! Must be between 1024 and 65536", 1);
	}

    // check for command line argument errors
    if (!host_name)
        exit_with_code("Please enter a host name using the --host option", 1);
    else if (!logging_enabled)
        exit_with_code("Please enter a log file name using the --log option", 1);
    else if (!port_number)
        exit_with_code("Please enter a valid port number (between 1024 and 65536)", 1);

    // If command line arguments are OK, open file for logging
    log_file = fopen(logfile_name, "w");
    if (!log_file)
    {
        fprintf(stderr, "Could not create logfile '%s': %s\n", logfile_name, strerror(errno));
        exit(1);
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

    // Miscellanous variables and setup
    int reading;
    float temperature_reading;
    memset(socket_receive_buffer, 0, BUFFERSIZE * sizeof(char));
    memset(socket_send_buffer, 0, BUFFERSIZE * sizeof(char));
    memset(command, 0, (BUFFERSIZE + 1) * sizeof(char));
    log_string = (char*) calloc(50, sizeof(char));
    if (!log_string)
        internal_program_error();
    

    // Setup socket and poll for it
    socket_fd = client_connect(host_name, port_number);
    struct pollfd socket_poll;
    socket_poll.fd = socket_fd;
    socket_poll.events = POLLIN | POLLERR | POLLHUP;

    // Setup SSL
    ssl_context = ssl_init();
    ssl_client = attach_ssl_to_socket(socket_fd, ssl_context);

    // Setup timer variables
    struct timespec curr_time, prev_time;
    clock_gettime(CLOCK_REALTIME, &curr_time);
    prev_time = curr_time;

    // Generating first report before the main loop begins
    update_time_string(curr_time);
    reading = mraa_aio_read(temperature);
    temperature_reading = get_temperature(reading);
    sprintf(socket_send_buffer, "%s %.1f\n", time_string, temperature_reading);
    Write(socket_send_buffer, strlen(socket_send_buffer));
    fprintf(log_file, "%s %.1f\n", time_string, temperature_reading);
    

    while (true)
    {
        clock_gettime(CLOCK_REALTIME, &curr_time);
        
        if ((time_elapsed(prev_time, curr_time) >= period) && !stop)
        {
            prev_time = curr_time;
            update_time_string(curr_time);
            reading = mraa_aio_read(temperature);
            temperature_reading = get_temperature(reading);
            sprintf(socket_send_buffer, "%s %.1f\n", time_string, temperature_reading);
            Write(socket_send_buffer, strlen(socket_send_buffer));
            fprintf(log_file, "%s %.1f\n", time_string, temperature_reading);
            
        }

        int temp = poll(&socket_poll, 1, 0);
        if (temp == -1)
            exit_with_code("Poll failed", 2);
        if (socket_poll.revents & POLLIN)
        {
            ret = SSL_read(ssl_client, socket_receive_buffer, BUFFERSIZE);
            if (ret)
            {
                if (ret < 0)
                    exit_with_code("Error in reading from SSL socket", 2);
                parse_commands();
            }
        }
        
    }

    mraa_gpio_close(button);
    mraa_aio_close(temperature);
    free(logfile_name);
    free(log_string);
    free(time_string);
    free(host_name);
    ssl_clean_up(ssl_client);
	return 0;
}
