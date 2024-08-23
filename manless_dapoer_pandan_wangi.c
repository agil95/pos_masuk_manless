#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <curl/curl.h>
#include <sqlite3.h>
#include <cjson/cJSON.h>
#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
// #include "thread.h"
#include <signal.h>
#include <dirent.h>

#include "serial.h"
#include "printer.h"
#include "constants.h"

#define BUFLEN 1024
#define AUTH_USER "svt"
#define AUTH_PASSWORD "lengbes73."
#define MAX_PATH_LENGTH 256
#define MAX_FILENAME_LENGTH 256
#define DB_LOCAL "local.db"
#define MIGRATION_LOOP 600
#define SDK 0
#define EMONEY 0

// #define ESC "\x1b"
// #define GS "\x1d"
// #define LF "\n"
// #define HT "\x09"

// Update version list:
// 1.0.0 === Basic version button method
// 1.0.1 === Add e-money method (set EMONEY = 1)

// Compile make -f Makefile_pos_masuk

static char app_version[] = "2.0.0";
static char app_builddate[] = "22/08/2024";

char id_config[1];
char status_config[1];
char ip_server[50];
char ip_manless[50];
char id_manless[50];
char id_location[2];
char path_image[100];
char gate_name[50];
char gate_kind[50];
char printer_port[50];
char printer_baudrate[50];
char controller_port[50];
char controller_baudrate[50];
char delay_loop[50];
char delay_led[50];
char paper_warning[50];
char service_host[50];
char service_port[50];
char reader_id[4];
char ip_camera_1[100];
char ip_camera_1_user[50];
char ip_camera_1_password[50];
char ip_camera_1_model[50];
char ip_camera_1_status[2];
char ip_camera_2[100];
char ip_camera_2_user[50];
char ip_camera_2_password[50];
char ip_camera_2_model[50];
char ip_camera_2_status[2];
char lpr_engine[255];
char lpr_engine_status[2];
char face_engine[255];
char face_engine_status[2];
char nama_lokasi[50];
char alamat_lokasi[50];
char pesan_masuk[100];
char jenis_langganan[57];
char bank_active[50];

char lastTicketCounter[4];
char nextTicketCounter[4];
char *timeFromServer;
int statusManless;
char linuxSerialController[256];
char linuxSerialPrinter[256];
int startController = 1;
int isFinish = 0;
int isPrinting = 0;
int fd;
int fdl;
char randomTicketCode[3];
char notiket[14];
char notiketQr[100];
int shift;
char nopol[14] = "-";
char nokartu[17] = "";
char nopol_lpr[14];
int max_call;
int isIdle;
bool isEmoney = false;
bool isTapin = true;
char bank[20];
int countBlocked = 0;
int countNotBlocked = 0;
char *time_from_server;
pthread_t ptd_balance, ptd_stop;
bool balanceTimeout = false;
bool printTicket;
int typeTrans = 0;

char buffer[1024];

struct string
{
	char *ptr;
	size_t len;
};

// Structure to hold the response data
struct MemoryStruct
{
    char *memory;
    size_t size;
};

// Prototype
void app_log(char* text);
int msleep(long tms);
void set_led(char *port, char *xstr);
int is_member(char *nokartu, char *nopol, char *notiket);
void *tapstop(void *data);
void *tapin(void *data);
int create_ticket();

static size_t 
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = (char *)realloc(mem->memory, mem->size + realsize + 1);
    if(ptr == NULL) {
        printf("Not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// Serial
void 
init_string(struct string *s)
{
	s->len = 0;
	s->ptr = (char *)malloc(s->len + 1);
	if (s->ptr == NULL)
	{
		fprintf(stderr, "malloc() failed\n");
		exit(EXIT_FAILURE);
	}
	s->ptr[0] = '\0';
}

size_t 
writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
{
	size_t new_len = s->len + size * nmemb;
	s->ptr = (char *)realloc(s->ptr, new_len + 1);
	if (s->ptr == NULL)
	{
		fprintf(stderr, "realloc() failed\n");
		exit(EXIT_FAILURE);
	}
	memcpy(s->ptr + s->len, ptr, size * nmemb);
	s->ptr[new_len] = '\0';
	s->len = new_len;

	return size * nmemb;
}

int 
set_interface_attribs(int fd, int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
	{
		printf("[ERROR] From tcgetattr: %s\n", strerror(errno));
		return -1;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tty.c_cflag |= (CLOCAL | CREAD); /* ignore modem controls */
	tty.c_cflag &= ~CSIZE;
	tty.c_cflag |= CS8;		/* 8-bit characters */
	tty.c_cflag &= ~PARENB; /* no parity bit */
	tty.c_cflag &= ~CSTOPB; /* only need 1 stop bit */
	// tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

	/* setup for non-canonical mode */
	tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
	tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
	tty.c_oflag &= ~OPOST;

	/* fetch bytes as they become available */
	tty.c_cc[VMIN] = 1;
	tty.c_cc[VTIME] = 1;

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		printf("[ERROR] From tcsetattr: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

void 
set_mincount(int fd, int mcount)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
	{
		printf("[ERROR] tcgetattr: %s\n", strerror(errno));
		return;
	}

	tty.c_cc[VMIN] = mcount ? 1 : 0;
	tty.c_cc[VTIME] = 5; /* half second timer */

	if (tcsetattr(fd, TCSANOW, &tty) < 0)
		printf("[ERROR] tcsetattr: %s\n", strerror(errno));
}
// --------------------------------------------------------------------------------------------------

// ESC POS Printer
typedef enum escpos_error
{
	ESCPOS_ERROR_NONE = 0,

	ESCPOS_ERROR_SOCK,
	ESCPOS_ERROR_INVALID_ADDR,
	ESCPOS_ERROR_CONNECTION_FAILED,
	ESCPOS_ERROR_SEND_FAILED,
	ESCPOS_ERROR_IMAGE_UPLOAD_FAILED,
	ESCPOS_ERROR_IMAGE_PRINT_FAILED,
	ESCPOS_ERROR_INVALID_CONFIG
} escpos_error;

escpos_error escpos_last_error();
escpos_error last_error = ESCPOS_ERROR_NONE;

escpos_printer 
*escpos_printer_serial(const char *const portname, const int baudrate)
{
	assert(portname != NULL);

	int serialfd;
	escpos_printer *printer = NULL;

	serialfd = open(portname, O_WRONLY | O_NOCTTY | O_SYNC);

	if (serialfd < 0)
	{
		last_error = ESCPOS_ERROR_SOCK;
		printf("[ERROR] Error opening %s: %s\n", portname, strerror(errno));
	}
	else
	{
		set_interface_attribs(serialfd, baudrate);

		printer = (escpos_printer *)malloc(sizeof(escpos_printer));
		printer->sockfd = serialfd;
		printer->config.max_width = ESCPOS_MAX_DOT_WIDTH;
		printer->config.chunk_height = ESCPOS_CHUNK_DOT_HEIGHT;
		printer->config.is_network_printer = 0;
	}

	return printer;
}

int 
escpos_printer_config(escpos_printer *printer, const escpos_config *const config)
{
	assert(printer != NULL);
	assert(config != NULL);

	printer->config = *config;
	return 0;
}

void 
escpos_printer_destroy(escpos_printer *printer)
{
	assert(printer != NULL);

	close(printer->sockfd);
	free(printer);
}

int 
escpos_printer_raw(escpos_printer *printer, const char *const message, const int len)
{
	assert(printer != NULL);

	int total = len;
	int sent = 0;
	int bytes = 0;

	if (printer->config.is_network_printer)
	{
		// Network printer logic.
		// Make sure send() sends all data
		while (sent < total)
		{
			bytes = send(printer->sockfd, message, total, 0);
			if (bytes == -1)
			{
				last_error = ESCPOS_ERROR_SEND_FAILED;
				break;
			}
			else
			{
				sent += bytes;
			}
		}
	}
	else
	{
		// Serial printer logic.
		sent = write(printer->sockfd, message, len);
		if (sent != len)
		{
			last_error = ESCPOS_ERROR_SEND_FAILED;
		}
		tcdrain(printer->sockfd);
	}

	return !(sent == total);
}

int 
escpos_printer_cut(escpos_printer *printer, const int lines)
{
	char buffer[4];
	strncpy(buffer, ESCPOS_CMD_CUT, 3);
	buffer[3] = lines;
	return escpos_printer_raw(printer, buffer, sizeof(buffer));
}

int 
escpos_printer_feed(escpos_printer *printer, const int lines)
{
	assert(lines > 0 && lines < 256);

	char buffer[3];
	strncpy(buffer, ESCPOS_CMD_FEED, 2);
	buffer[2] = lines;
	return escpos_printer_raw(printer, buffer, sizeof(buffer));
}

void 
set_bit(unsigned char *byte, const int i, const int bit)
{
	assert(byte != NULL);
	assert(i >= 0 && i < 8);

	if (bit > 0)
	{
		*byte |= 1 << i;
	}
	else
	{
		*byte &= ~(1 << i);
	}
}

// Calculates the padding required so that the size fits in 32 bits
void 
calculate_padding(const int size, int *padding_l, int *padding_r)
{
	assert(padding_l != NULL);
	assert(padding_r != NULL);

	if (size % 32 == 0)
	{
		*padding_l = 0;
		*padding_r = 0;
	}
	else
	{
		int padding = 32 - (size % 32);
		*padding_l = padding / 2;
		*padding_r = padding / 2 + (padding % 2 == 0 ? 0 : 1);
	}
}

void 
convert_image_to_bits(unsigned char *pixel_bits,
						   const unsigned char *image_data,
						   const int w,
						   const int h,
						   int *bitmap_w,
						   int *bitmap_h)
{
	assert(pixel_bits != NULL);
	assert(image_data != NULL);
	assert(bitmap_w != NULL);
	assert(bitmap_h != NULL);

	int padding_l, padding_r, padding_t, padding_b;
	calculate_padding(w, &padding_l, &padding_r);
	calculate_padding(h, &padding_t, &padding_b);

	int padded_w = w + padding_l + padding_r;

	// We only need to add the padding to the bottom for height.
	// This is because when printing long images, only the last
	// chunk will have the irregular height.
	padding_b += padding_t;

	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < padded_w; x++)
		{
			int pi = (y * padded_w) + x;
			int curr_byte = pi / 8;
			unsigned char pixel = image_data[(y * w) + x];
			int bit = y < h ? pixel < 128 : 0;
			set_bit(&pixel_bits[curr_byte], 7 - (pi % 8), bit);
		}
	}

	for (int y = 0; y < padding_b; y++)
	{
		for (int x = 0; x < padded_w; x++)
		{
			int pi = (h * padded_w) + (y * padded_w) + x;
			int curr_byte = pi / 8;
			set_bit(&pixel_bits[curr_byte], 7 - (pi % 8), 0);
		}
	}

	// Outputs the bitmap width and height after padding
	*bitmap_w = w + padding_l + padding_r;
	*bitmap_h = h + padding_b;
}

int 
escpos_printer_print(escpos_printer *printer,
						 const unsigned char *pixel_bits,
						 const int w,
						 const int h)
{
	assert(printer != NULL);
	assert(pixel_bits != NULL);
	assert(w > 0 && w <= printer->config.max_width);
	assert(h > 0 && h <= printer->config.chunk_height);
	assert(w % 32 == 0);
	assert(h % 32 == 0);

	int result = escpos_printer_raw(printer, ESCPOS_CMD_PRINT_RASTER_BIT_IMAGE, 4);

	char buffer[4];
	buffer[0] = (((w / 8) >> 0) & 0xFF);
	buffer[1] = (((w / 8) >> 8) & 0xFF);
	buffer[2] = ((h >> 0) & 0xFF);
	buffer[3] = ((h >> 8) & 0xFF);
	result = escpos_printer_raw(printer, buffer, 4);
	result = escpos_printer_raw(printer, (char *)pixel_bits, (w / 8) * h);

	if (result != 0)
	{
		last_error = ESCPOS_ERROR_IMAGE_PRINT_FAILED;
	}

	return result;
}

int 
escpos_printer_image(escpos_printer *printer,
						 const unsigned char *const image_data,
						 const int width,
						 const int height)
{
	assert(printer != NULL);
	assert(image_data != NULL);
	assert(width > 0 && width <= printer->config.max_width);
	assert(height > 0);

	int result = 0;

	if (image_data != NULL)
	{
		int byte_width = printer->config.max_width / 8;

		int padding_t, padding_b;
		calculate_padding(height, &padding_t, &padding_b);
		int print_height = height + padding_t + padding_b;

		unsigned char pixel_bits[byte_width * print_height];

		int c = 0;
		int chunks = 0;
		if (height <= printer->config.chunk_height)
		{
			chunks = 1;
		}
		else
		{
			chunks = (height / printer->config.chunk_height) + (height % printer->config.chunk_height ? 1 : 0);
		}

		while (c < chunks)
		{
			// Because the printer's image buffer has a limited memory,
			// if the image's height exceeds config.chunk_height pixels,
			// it is printed in chunks of x * config.chunk_height pixels.
			int chunk_height = (c + 1) * printer->config.chunk_height <= height ? printer->config.chunk_height : height - (c * printer->config.chunk_height);

			int bitmap_w, bitmap_h;
			convert_image_to_bits(
				pixel_bits,
				image_data + (c * printer->config.chunk_height * width),
				width,
				chunk_height,
				&bitmap_w,
				&bitmap_h);

			result = escpos_printer_print(printer, pixel_bits, bitmap_w, bitmap_h);
			if (result != 0)
			{
				break;
			}

			c += 1;
		}
	}

	return result;
}
// --------------------------------------------------------------------------------------------------
// Log
void 
app_log(char* text) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);

    char year[5], month[3], day[3];
    sprintf(year, "%d", tm.tm_year + 1900);
    sprintf(month, "%02d", tm.tm_mon + 1);
    sprintf(day, "%02d", tm.tm_mday);

    char filename[20];
    sprintf(filename, "log/%s-%s-%s.txt", year, month, day);

    char paths[10] = "log/";

    if (mkdir(paths, 0777) == -1) {
        // Directory already exists or creation failed
    }

    FILE *file = fopen(filename, "a+");

    if (file != NULL) {
        fprintf(file, "[%d-%02d-%02d %02d:%02d:%02d]: %s",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                tm.tm_hour, tm.tm_min, tm.tm_sec, text);

        fclose(file);
    } else {
        perror("[ERROR] input log error -> Unable to open file");
    }
}

// Function to get the difference in seconds between two timespec structures
double 
getTimeDifference(struct timespec start, struct timespec end) {
    double start_sec = (double)start.tv_sec + (double)start.tv_nsec / 1e9;
    double end_sec = (double)end.tv_sec + (double)end.tv_nsec / 1e9;
    return end_sec - start_sec;
}

// Function to convert a string to lowercase
char* 
stringToLower(char* str) {
    for (int i = 0; str[i]; i++) {
        str[i] = tolower((unsigned char)str[i]);
    }
    return str;
}

int
list_bank()
{
	sqlite3 *DB;
	sqlite3_stmt *res;
	const char *tail;
	char banks[50];

	int sqlite = sqlite3_open(DB_LOCAL, &DB);
	if (sqlite)
		fprintf(stderr, "[ERROR] Can't open database: %s\n", sqlite3_errmsg(DB));

	if (sqlite3_prepare_v2(DB, "SELECT * FROM config WHERE status=1", 128, &res, &tail) != SQLITE_OK)
	{
		sqlite3_close(DB);
		printf("[ERROR] Can't retrieve data: %s\n", sqlite3_errmsg(DB));
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		// printf("%s", sqlite3_column_text(res, 35));
		sprintf(bank_active, "%s", sqlite3_column_text(res, 35));
	}

	sqlite3_finalize(res);

	return 0;
}

// Socket connection
void 
*tapstop(void *data)
{
	pthread_detach(pthread_self());
	int status, valread, client_fd;
    struct sockaddr_in serv_addr;
	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "taptype", "stop");
	cJSON_AddNumberToObject(json, "id", atoi(reader_id));
	char *json_str = cJSON_Print(json);
    char buffer[1024] = { 0 };

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[ERROR] Socket creation error \n");
    }

	// Set the socket connect timeout to 5 seconds
	struct timeval tv;
	tv.tv_sec = 5;
	tv.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(service_port));
    serv_addr.sin_addr.s_addr = inet_addr(service_host);

    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        printf("[ERROR] Socket Connection failed \n");
    }

	// Send data
    send(client_fd, json_str, strlen(json_str), 0);
    valread = read(client_fd, buffer, 1024 - 1); // subtract 1 for the null terminator at the end
	if (valread < 0) {
		printf("[ERROR] Socket Connection Time out\n");
	}
    printf("[INFO] Received from service => %s\n", buffer);

    // closing the connected socket
    close(client_fd);
	return 0;
}

void 
*tapin(void *data)
{
	pthread_detach(pthread_self());
    isTapin = false;
	int status, valread, client_fd;
	bool isSuccess = false;
    const char delimiter[2] = ",";
    char* token;
    char *v1, *v2, *v3;
    struct sockaddr_in serv_addr;
	cJSON *json = cJSON_CreateObject();
	cJSON_AddStringToObject(json, "taptype", "in");
	cJSON_AddNumberToObject(json, "id", atoi(reader_id));
	char *json_str = cJSON_Print(json);
	int res_status;
	bool isTimeout;
	bool bankRegistered = false;
    char buffer[1024] = { 0 };

	// if (isTimeout) {
	// 	app_log((char *)"[INFO] Silahkan tap ulang kartu anda ...\n");
	// 	printf("[INFO] Silahkan tap ulang kartu anda ...\n");
	// }

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[ERROR][TAPIN] Socket creation error \n");
        // return -1;
    }

	// Set the socket connect timeout to 5 seconds
	struct timeval tv;
	tv.tv_sec = 30;
	tv.tv_usec = 0;
	setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(service_port));
    serv_addr.sin_addr.s_addr = inet_addr(service_host);

    if ((status = connect(client_fd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))) < 0) {
        printf("[ERROR][TAPIN] Socket Connection Failed \n");
        // return -1;
    }

	// Send data
    send(client_fd, json_str, strlen(json_str), 0);
    valread = read(client_fd, buffer, 1024 - 1); // subtract 1 for the null terminator at the end
	if (valread < 0) {
		isTimeout = true;
		printf("[ERROR][TAPIN] Socket Connection Time Out\n");
		isTapin = true;
		// pthread_create(&ptd_balance, NULL, tapin, (void *)"1");
		// return -1;
	} else {
		isTimeout = false;
		printf("[INFO] Received from service => %s\n", buffer);

		// parse the JSON data
		cJSON *jsonRes = cJSON_Parse(buffer);
		if (jsonRes == NULL)
		{
			cJSON_Delete(json);
		} else {
			// access the JSON data
			cJSON *success = cJSON_GetObjectItem(jsonRes, "status");
			// printf("Status: %d\n", cJSON_IsTrue(status));
			res_status = cJSON_IsTrue(success);
			if (res_status == 1)
			{
				cJSON *cbank = cJSON_GetObjectItemCaseSensitive(jsonRes, "bank");
				if (cJSON_IsString(cbank) && (cbank->valuestring != NULL)) {
					strcpy(bank, cbank->valuestring);
				}
				asprintf(&v1, "[INFO] Bank: %s\n", bank);
				app_log(v1);
				free(v1);
				printf("[INFO] Bank: %s\n", bank);
				cJSON *can = cJSON_GetObjectItemCaseSensitive(jsonRes, "can");
				if (cJSON_IsString(can) && (can->valuestring != NULL)) {
					// strcpy(nokartu, can->valuestring);
					sprintf(nokartu, "%s", can->valuestring);
				}
				asprintf(&v2, "[INFO] CAN: %s\n", nokartu);
				app_log(v2);
				free(v2);
				printf("[INFO] CAN : %s\n", nokartu);
				//char* lowerBank = stringToLower(bank);
				list_bank();
				// printf("[INFO] Bank Active : %s\n", bank_active);
				token = strtok(bank_active, delimiter);
				while (token != NULL) {
					//char* lowerToken = stringToLower(token);
					// printf("[INFO] - %s\n", token);
					if (strstr(token, bank) != NULL){
						bankRegistered = true;
					}
					token = strtok(NULL, delimiter);
				}
				if (!bankRegistered) {
					asprintf(&v3,"[INFO] Bank %s belum terdaftar di config!\n", bank);
					app_log(v3);
					free(v3);
					printf("[INFO] Bank %s belum terdaftar di config!\n", bank);
					sleep(3);
					isTapin = true;
				} else {
					isSuccess = true;
				}
				isEmoney = true;
			}
			else
			{
				sleep(3);
				isTapin = true;
			}
		}
	}
    // closing the connected socket
    close(client_fd);
	if (isSuccess == true && bankRegistered == true)
	{
        // member cek
        is_member(nokartu, (char *)"-", (char *)"-");
		msleep(100);
		set_led(linuxSerialController, (char *)"LD3T\n");
		create_ticket();
	} 
	return 0;
}

// Upload poto
int 
insert_image(char *file)
{
	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct curl_slist *headerlist = NULL;
	static const char buf[] = "Expect:";
	int success = 0;
	char url[256];

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl)
	{
		sprintf(url, "http://%s:5000/business-parking/api/v1/tools/image/upload", ip_server);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_USERNAME, AUTH_USER);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, AUTH_PASSWORD);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);

		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "file",
					 CURLFORM_FILE, file,
					 CURLFORM_CONTENTTYPE, "image/jpeg",
					 CURLFORM_END);

		headerlist = curl_slist_append(headerlist, buf);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
			fprintf(stderr, "[ERROR][API] Upload photo failed: %s\n", curl_easy_strerror(res));
		}

		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		if (http_code == 200)
		{
			success = 1;
			// printf("[INFO] Insert image -> %s\n", res);
		}
		else
		{
			printf("[INFO] Gagal insert image -> %d\n", res);
		}

		curl_easy_cleanup(curl);
		curl_formfree(formpost);
		curl_slist_free_all(headerlist);
	}

	curl_global_cleanup();
	return success;
}

// Create folder
int 
CreatePath(const char *sPathName)
{
	char DirName[256];
	strcpy(DirName, sPathName);
	int i, len = strlen(DirName);
	if (DirName[len - 1] != '/')
		strcat(DirName, "/");

	len = strlen(DirName);

	for (i = 1; i < len; i++)
	{
		if (DirName[i] == '/')
		{
			DirName[i] = 0;
			if (access(DirName, R_OK) != 0)
			{
				if (mkdir(DirName, 0755) == -1)
				{
					perror("mkdir   error");
					return -1;
				}
			}
			DirName[i] = '/';
		}
	}

	return 0;
}

int 
is_valid_ip(const char *ip, int index)
{
	int section = 0;
	int dot = 0;
	int last = -1;
	while (*ip)
	{
		if (*ip == '.')
		{
			if (*(ip + 1))
			{
				if (*(ip + 1) == '.')
					return 0;
			}
			else
				return 0;
			dot++;
			if (dot > 3)
			{
				return 0;
			}
			if (section >= 0 && section <= 255)
			{
				section = 0;
			}
			else
			{
				return 0;
			}
		}
		else if (*ip >= '0' && *ip <= '9')
		{
			section = section * 10 + *ip - '0';
		}
		else
		{
			return 0;
		}
		last = *ip;
		ip++;
	}

	if (section >= 0 && section <= 255)
	{
		if (3 == dot)
		{
			if ((section == 0 || section == 255) && (0 == index))
			{
				return 0;
			}
			return 1;
		}
	}
	return 0;
}

int 
ping(const char *ip)
{
	int response = 1;
	char command[120];
	sprintf(command, "ping -c 1 %s > /dev/null", ip);
	response = system(command);
	// printf("[INFO] %i\n", response);
	return response;
}

int 
msleep(long tms)
{
	struct timespec ts;
	int ret;

	if (tms < 0)
	{
		errno = EINVAL;
		return -1;
	}

	ts.tv_sec = tms / 1000;
	ts.tv_nsec = (tms % 1000) * 1000000;

	do
	{
		ret = nanosleep(&ts, &ts);
	} while (ret && errno == EINTR);

	return ret;
}

void 
random_ticket_code(int min_num, int max_num)
{
	int result = 0, low_num = 0, hi_num = 0;

	if (min_num < max_num)
	{
		low_num = min_num;
		hi_num = max_num + 1; // include max_num in output
	}
	else
	{
		low_num = max_num + 1; // include max_num in output
		hi_num = min_num;
	}

	srand(time(NULL));
	result = (rand() % (hi_num - low_num)) + low_num;
	sprintf(randomTicketCode, "%02d", result);
	printf("[INFO] Ticket code -> %s\n", randomTicketCode);
	// return result;
}

int 
device_io(char *port, int baudrate, int timeout)
{
	int fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1)
	{
		printf("[ERROR] Failed to open port %s\n", port);
		return -1;
	}

	struct termios options;
	tcgetattr(fd, &options);
	cfsetispeed(&options, baudrate);
	cfsetospeed(&options, baudrate);
	options.c_cflag |= (CLOCAL | CREAD);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
	options.c_iflag &= ~(IXON | IXOFF | IXANY);
	options.c_oflag &= ~OPOST;
	options.c_cc[VMIN] = 0;
	options.c_cc[VTIME] = timeout;

	tcsetattr(fd, TCSANOW, &options);

	return fd;
}

void 
copyfile(const char *src, const char *dest)
{
	FILE *source = fopen(src, "rb");
	FILE *destination = fopen(dest, "wb");
	if (source == NULL || destination == NULL)
	{
		printf("[ERROR] copyfile opening file\n");
		return;
	}
	char buff[1024];
	size_t bytesRead;
	while ((bytesRead = fread(buff, 1, sizeof(buff), source)) > 0)
	{
		fwrite(buff, 1, bytesRead, destination);
	}
	fclose(source);
	fclose(destination);
}

int 
fileCheck(const char *fileName)
{
	if (!access(fileName, F_OK))
	{
		return 1;
	}
	else
	{
		return 0;
	}
}

static int 
sqlite_callback(void *data, int argc, char **argv, char **azColName)
{
	int i;
	fprintf(stderr, "%s: \n", (const char *)data);

	for (i = 0; i < argc; i++)
	{
		// printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
		if (azColName[i] == "id")
			strcpy(id_config, argv[i] ? argv[i] : "NULL");
		if (azColName[i] == "server")
			strcpy(ip_server, argv[i] ? argv[i] : "NULL");
		if (azColName[i] == "ipcam_1")
			strcpy(ip_camera_1, argv[i] ? argv[i] : "NULL");
		// printf("%s ", azColName[i]);
	}

	if (stderr == NULL)
		printf("Null Data");

	printf("\n");
	return 0;
}

// LPR Detection versi PlateRecognizer
char 
*lpr_detection(char *url, int max_call, char *file)
{
	char *region = (char *)"id";
	char *nopol = NULL;
	CURL *curl;
	CURLcode res;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct curl_slist *headerlist = NULL;
	static const char buf[] = "Expect:";

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl)
	{
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "regions",
					 CURLFORM_COPYCONTENTS, region,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "upload",
					 CURLFORM_FILE, file,
					 CURLFORM_END);

		headerlist = curl_slist_append(headerlist, buf);
		if (curl)
		{
			curl_easy_setopt(curl, CURLOPT_URL, url);
			curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
			curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);

			res = curl_easy_perform(curl);
			if (res != CURLE_OK)
			{
				fprintf(stderr, "curl_easy_perform() failed: %s\n",
						curl_easy_strerror(res));
			}

			curl_easy_cleanup(curl);
			curl_formfree(formpost);
			curl_slist_free_all(headerlist);
		}
	}
	curl_global_cleanup();

	return nopol;
}

// Ticket Counter
const char 
*ticket_counter()
{
	sqlite3 *DB;
	sqlite3_stmt *res;
	const char *tail;

	int sqlite = sqlite3_open("local.db", &DB);
	if (sqlite)
		fprintf(stderr, "[ERROR] Can't open database: %s\n", sqlite3_errmsg(DB));

	if (sqlite3_prepare_v2(DB, "SELECT * FROM counter LIMIT 1", 128, &res, &tail) != SQLITE_OK)
	{
		sqlite3_close(DB);
		printf("[ERROR] Can't retrieve data: %s\n", sqlite3_errmsg(DB));
	}

	while (sqlite3_step(res) == SQLITE_ROW)
	{
		// printf("%s", sqlite3_column_text(res, 1));
		sprintf(lastTicketCounter, "%s", sqlite3_column_text(res, 1));
	}

	sqlite3_finalize(res);

	return lastTicketCounter;
}

// Update Counter
int 
update_counter(char *counter)
{
	time_t now = time(NULL);
	struct tm *timeinfo = localtime(&now);
	sqlite3 *db;
	char *err_msg = 0;
	char *v18;

	if (strcmp(counter, "999") == 0) asprintf(&counter, "%s", "0");
	
	int rc = sqlite3_open("local.db", &db);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	char updated_at[20];
	strftime(updated_at, sizeof(updated_at), "%Y-%m-%d %H:%M:%S", timeinfo);
	char sql[255];
	snprintf(sql, sizeof(sql), "UPDATE counter SET count=\"%s\", updated_at=\"%s\" WHERE id=1;",
			 counter, updated_at);
	// printf("%s\n", sql);

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return 1;
	}
	asprintf(&v18, "[INFO][LOCAL] Successfully update counter -> %s\n", counter);
	app_log(v18);
	printf("[INFO][LOCAL] Successfully update counter -> %s\n", counter);

	sqlite3_close(db);
	return 0;
}

// Parking Available
int 
parking_available(unsigned int ct)
{
	char *v5;
	int pa = atoi(paper_warning) - ct;
	if (pa == 0 || ct == 999)
	{
		app_log((char *)"[INFO] Gagal print tiket! kertas habis...\n");
		printf("[INFO] Gagal print tiket! kertas habis...\n");
	}
	else if (ct >= pa)
	{
		app_log((char *)"[INFO] Peringatan! kertas sebentar lagi habis...\n");
		printf("[INFO] Peringatan! kertas sebentar lagi habis...\n");
	}

	asprintf(&v5, "[INFO] Parking available => %d\n", pa);
	app_log(v5);
	printf("[INFO] Parking available => %d\n", pa);
	return 0;
}

// Store data
int 
store_cloud(char *waktumasuk, char *posmasuk, char *kasirmasuk, char *jeniskend, char *noid, char *jnslangganan, char *shift, char *notiket, char *nopol, char *nokartu)
{
	CURL *curl;
	CURLcode res;
	int response;
	int res_status;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct MemoryStruct chunk;
    chunk.memory = (char *)malloc(1);  // Initial memory allocation
    chunk.size = 0;            // Initial size
    char *tolog;
	char url[256];

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl)
	{
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "waktumasuk",
					 CURLFORM_COPYCONTENTS, waktumasuk,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "posmasuk",
					 CURLFORM_COPYCONTENTS, posmasuk,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "kasirmasuk",
					 CURLFORM_COPYCONTENTS, kasirmasuk,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "jeniskend",
					 CURLFORM_COPYCONTENTS, jeniskend,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "noid",
					 CURLFORM_COPYCONTENTS, noid,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "jnslangganan",
					 CURLFORM_COPYCONTENTS, jnslangganan,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "shift",
					 CURLFORM_COPYCONTENTS, shift,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "notiket",
					 CURLFORM_COPYCONTENTS, notiket,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "nopol",
					 CURLFORM_COPYCONTENTS, nopol,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "nokartu",
					 CURLFORM_COPYCONTENTS, nokartu,
					 CURLFORM_END);

		sprintf(url, "http://%s:5000/business-parking/api/v1/manless/ticket/store", ip_server);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERNAME, AUTH_USER);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, AUTH_PASSWORD);
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
            response = 1;
            asprintf(&tolog, "[ERROR][API] Store ticket failed: %s\n", curl_easy_strerror(res));
			app_log(tolog);
			fprintf(stderr, "[ERROR][API] Store ticket failed: %s\n", curl_easy_strerror(res));
		} else {
			// printf("[INFO] Response store: %s\n", chunk.memory);

			// Parse JSON response
			cJSON *json = cJSON_Parse(chunk.memory);
			if (json == NULL) {
				const char *error_ptr = cJSON_GetErrorPtr();
				if (error_ptr != NULL) {
					fprintf(stderr, "[ERROR] Error before: %s\n", error_ptr);
				}
			} else {
				// Accessing values from JSON
				cJSON *status = cJSON_GetObjectItem(json, "success");
				res_status = cJSON_IsTrue(status);
				cJSON *j_message = cJSON_GetObjectItemCaseSensitive(json, "message");
				if (cJSON_IsString(j_message) && (j_message->valuestring != NULL)) {
					asprintf(&tolog, "[INFO] Response store [%s]: %s\n", notiket, j_message->valuestring);
					app_log(tolog);
					printf("[INFO] Response store [%s]: %s\n", notiket, j_message->valuestring);
				} 
				if (res_status == 1)
				{
					response = 0;
				} else {
					response = 1;
				}
				cJSON_Delete(json);
			}
		}

		// long http_code = 0;
		// curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
		// if (http_code == 200)
		// {
		// 	response = 0;
		// }
		// else
		// {
		// 	response = 1;
		// }

		curl_easy_cleanup(curl);
		curl_formfree(formpost);
	}

	curl_global_cleanup();
	return response;
}

int 
store_local(char *waktumasuk, char *posmasuk, char *kasirmasuk, char *jeniskend, char *noid, char *jnslangganan, char *shift, char *notiket, char *nopol, char *nokartu)
{
	sqlite3 *db;
	char *err_msg = 0;
	char *v19;

	int rc = sqlite3_open(DB_LOCAL, &db);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] Cannot open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	char sql[255];
	snprintf(sql, sizeof(sql), "INSERT INTO tlog(waktumasuk, posmasuk, kasirmasuk, jeniskend, noid, jnslangganan, shift, notiket, nopol, nokartu) VALUES(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\", \"%s\");",
			 waktumasuk, posmasuk, kasirmasuk, jeniskend, noid, jnslangganan, shift, notiket, nopol, nokartu);
	// printf("%s\n", sql);

	rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] SQL error: %s\n", err_msg);
		sqlite3_free(err_msg);
		sqlite3_close(db);
		return 1;
	}
	asprintf(&v19, "[INFO][LOCAL] Successfully store ticket #%s\n", notiket);
	app_log(v19);
	printf("[INFO][LOCAL] Successfully store ticket #%s\n", notiket);

	sqlite3_close(db);
	return 0;
}

void 
delete_data_offline(char *notiket)
{
	sqlite3 *DB;
	char *errMsg;
	int rc;

	rc = sqlite3_open(DB_LOCAL, &DB);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] Cannot open database: %s\n", sqlite3_errmsg(DB));
		return;
	}

	const char *sql = "DELETE FROM tlog WHERE notiket=?";
	sqlite3_stmt *res;
	rc = sqlite3_prepare_v2(DB, sql, -1, &res, NULL);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] Cannot prepare statement: %s\n", sqlite3_errmsg(DB));
		sqlite3_close(DB);
		return;
	}

	rc = sqlite3_bind_text(res, 1, notiket, -1, SQLITE_STATIC);
	if (rc != SQLITE_OK)
	{
		fprintf(stderr, "[ERROR] Cannot bind parameter: %s\n", sqlite3_errmsg(DB));
		sqlite3_finalize(res);
		sqlite3_close(DB);
		return;
	}

	rc = sqlite3_step(res);
	if (rc != SQLITE_DONE)
	{
		fprintf(stderr, "[ERROR] Execution failed: %s\n", sqlite3_errmsg(DB));
		sqlite3_finalize(res);
		sqlite3_close(DB);
		return;
	}

	sqlite3_finalize(res);
	sqlite3_close(DB);

	printf("[INFO] Success delete notiket #%s [offline]\n", notiket);
}

int 
is_member(char *nokartu, char *nopol, char *notiket)
{
	CURL *curl;
	CURLcode res;
    struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	char url[100];
	int res_status;
	char defaultjenis[] = "CASUAL";
	bool isSuccess = false;
	int member_code;
	struct MemoryStruct chunk;
    chunk.memory = (char *)malloc(1);  // Initial memory allocation
    chunk.size = 0;            // Initial size
	curl = curl_easy_init();
	if(curl) {
        curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "notiket",
					 CURLFORM_COPYCONTENTS, notiket,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "nopol",
					 CURLFORM_COPYCONTENTS, nopol,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "nokartu",
					 CURLFORM_COPYCONTENTS, nokartu,
					 CURLFORM_END);
		sprintf(url, "http://%s:5000/business-parking/api/v1/card/checking", ip_server);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
		curl_easy_setopt(curl, CURLOPT_USERNAME, AUTH_USER);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, AUTH_PASSWORD);
		// Set the callback function to handle the response data
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    	// Pass the chunk structure to the callback function
    	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

		res = curl_easy_perform(curl);
		if(res != CURLE_OK) {
			fprintf(stderr, "[ERROR][API] Member check failed: %s\n", curl_easy_strerror(res));
		} else {
			// Successfully received response, print it
			// printf("[INFO] %lu bytes retrieved\n", (unsigned long)chunk.size);
			printf("[INFO] Response member: %s\n", chunk.memory);

			// Parse JSON response
			cJSON *json = cJSON_Parse(chunk.memory);
			if (json == NULL) {
				const char *error_ptr = cJSON_GetErrorPtr();
				if (error_ptr != NULL) {
					fprintf(stderr, "[ERROR] Error before: %s\n", error_ptr);
				}
			} else {
				// Accessing values from JSON
				cJSON *status = cJSON_GetObjectItem(json, "success");
				res_status = cJSON_IsTrue(status);
				if (res_status == 1)
				{
					cJSON *j_data = cJSON_GetObjectItemCaseSensitive(json, "data");
					cJSON *j_code = cJSON_GetObjectItemCaseSensitive(json, "code");
					cJSON *j_jnslangganan = cJSON_GetObjectItemCaseSensitive(j_data, "jnslangganan");
					if (cJSON_IsNumber(j_code)) member_code = j_code->valueint;
					switch (member_code)
					{
						case 2:
							if (cJSON_IsString(j_jnslangganan) && (j_jnslangganan->valuestring != NULL)) sprintf(jenis_langganan, "%s", j_jnslangganan->valuestring);
							break;

						default:
							sprintf(jenis_langganan, "CASUAL %s", gate_kind);
							break;
					}
				} else {
					sprintf(jenis_langganan, "CASUAL %s", gate_kind);
				}
				printf("[INFO] Jenis Langganan -> %s\n", jenis_langganan);
				cJSON_Delete(json);
			}
		}
	}
	curl_easy_cleanup(curl);
	return member_code;
}

// Migration transaction
void 
move_photo()
{
	char src[16];
	struct dirent *entry;
	DIR *dir;
	time_t rawtime;
	struct tm *timeinfo;
	char tanggal[11];
	char source[275];
	char notiket[14];
	int i = 0;
	int len = 0;

	time(&rawtime);
	timeinfo = localtime(&rawtime);
	strftime(tanggal, sizeof(tanggal), "%Y-%m-%d", timeinfo);
	sprintf(src, "tmp/%s", tanggal);

	dir = opendir(src);
	if (dir == NULL)
	{
		printf("[INFO] No temporary picture found...\n");
	}
	else
	{
		while ((entry = readdir(dir)) != NULL)
		{
			if (entry->d_type == DT_REG)
			{
				len++;
				sprintf(source, "%s/%s", src, entry->d_name);
				strncpy(notiket, source + (16 - 1), 13);
				printf("[INFO] Migrate => %s  %d/%d Ok\n", source, i + 1, len);
				int res_upload = insert_image(source);
				if (res_upload == 1)
				{
					printf("[INFO] Migrate photo => %s  %d/%d Ok\n", source, i + 1, len);
					remove(source);
				}
				else
				{
					printf("[INFO] Migrate photo => %s  %d/%d Failed\n", source, i + 1, len);
				}
				i++;
			}
		}
		rewinddir(dir);
		closedir(dir);
	}
}

void 
*migrate_transaction(void *tipe)
{
	sqlite3 *DB;
	sqlite3_stmt *res;
	const char *tail;
	int rc;
	char source[16];
	char wm_substring[11];
	char *v8;

	pthread_detach(pthread_self());
	while (1)
	{
		rc = sqlite3_open(DB_LOCAL, &DB);
		if (rc != SQLITE_OK)
		{
			fprintf(stderr, "[ERROR] Cannot open database: %s\n", sqlite3_errmsg(DB));
		}

		if (sqlite3_prepare_v2(DB, "SELECT * FROM tlog", 128, &res, &tail) != SQLITE_OK)
		{
			sqlite3_close(DB);
			fprintf(stderr, "[ERROR] Failed to execute statement: %s\n", sqlite3_errmsg(DB));
		}

		int data = 0;
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			data++;
			int id = sqlite3_column_int(res, 0);
			const unsigned char *waktu_masuk = sqlite3_column_text(res, 1);
			const unsigned char *pos = sqlite3_column_text(res, 2);
			const unsigned char *gate_name = sqlite3_column_text(res, 3);
			const unsigned char *jenis_kendaraan = sqlite3_column_text(res, 4);
			const unsigned char *noid = sqlite3_column_text(res, 5);
			const unsigned char *jnslangganan = sqlite3_column_text(res, 6);
			const unsigned char *shift = sqlite3_column_text(res, 7);
			const unsigned char *ticket = sqlite3_column_text(res, 8);
			const unsigned char *nopol = sqlite3_column_text(res, 9);
			const unsigned char *kartu = sqlite3_column_text(res, 10);

			int response = store_cloud((char *)waktu_masuk, (char *)pos, (char *)gate_name, (char *)jenis_kendaraan, (char *)noid, (char *)jnslangganan, (char *)shift, (char *)ticket, (char *)nopol, (char *)kartu);
			if(response == 0)
			{
				asprintf(&v8, "[INFO] Migrate transaction => ID:[%d] | TIKET:[%s] | WAKTU MASUK:[%s] | JENIS KENDARAAN:[%s] %d/%d Success\n", id, ticket, waktu_masuk, jenis_kendaraan, data, data);
				app_log(v8);
				printf("[INFO] Migrate transaction => ID:[%d] | TIKET:[%s] | WAKTU MASUK:[%s] | JENIS KENDARAAN:[%s] %d/%d Success\n", id, ticket, waktu_masuk, jenis_kendaraan, data, data);
				delete_data_offline((char *)ticket);
				move_photo();
			} else {
				asprintf(&v8, "[INFO] Migrate transaction => ID:[%d] | TIKET:[%s] | WAKTU MASUK:[%s] | JENIS KENDARAAN:[%s] %d/%d Failed\n", id, ticket, waktu_masuk, jenis_kendaraan, data, data);
				app_log(v8);
				printf("[INFO] Migrate transaction => ID:[%d] | TIKET:[%s] | WAKTU MASUK:[%s] | JENIS KENDARAAN:[%s] %d/%d Failed\n", id, ticket, waktu_masuk, jenis_kendaraan, data, data);
			}
		}

		if (data != 0)
		{
			asprintf(&v8, "[INFO] Transaction in local found `%d` data\n", data);
			printf("[INFO] Transaction in local found `%d` data\n", data);
			move_photo();
		}
		else
		{
			app_log((char *)"[INFO] No data transaction in local...\n");
			printf("[INFO] No data transaction in local...\n");
		}

		sqlite3_finalize(res);
		sqlite3_close(DB);
		sleep(MIGRATION_LOOP);
	}
	// pthread_exit(NULL);
}

// Time Sync
void 
time_sync()
{
	CURL *curl;
    CURLcode res;
    char url[256];
    char command[256];
    struct MemoryStruct chunk;
    chunk.memory = (char *)malloc(1);  // Initial memory allocation
    chunk.size = 0;            // Initial size
    curl = curl_easy_init();
    if (curl)
    {
        sprintf(url, "http://%s:5000/business-parking/api/v1/tools/date/now", ip_server);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "GET");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_DEFAULT_PROTOCOL, "https");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERNAME, AUTH_USER);
        curl_easy_setopt(curl, CURLOPT_PASSWORD, AUTH_PASSWORD);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");
        // Set the callback function to handle the response data
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        // Pass the chunk structure to the callback function
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 1);

        res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            fprintf(stderr, "[ERROR] Failed get time from server: %s\n", curl_easy_strerror(res));
        }
        else
        {
            // parse the JSON data
            cJSON *json = cJSON_Parse(chunk.memory);
            if (json == NULL)
            {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL) {
                    fprintf(stderr, "[ERROR] Error before: %s\n", error_ptr);
                }
            } else {
                int res_status;
                // access the JSON data
                cJSON *status = cJSON_GetObjectItem(json, "success");
                // printf("Status: %d\n", cJSON_IsTrue(status));
                res_status = cJSON_IsTrue(status);
                // printf("[INFO] status => %d\n", res_status);
                if (res_status == 1)
                {
                    cJSON *datetime = cJSON_GetObjectItem(json, "data");
                    if (cJSON_IsString(datetime) && (datetime->valuestring != NULL))
                    {
                        // printf("Datetime: %s\n", datetime->valuestring);
                        timeFromServer = datetime->valuestring;
                        snprintf(command, sizeof(command), "echo 1234 | sudo -S timedatectl set-time '%s'", timeFromServer);
                        // Execute the command
                        int result = system(command);
                        // Check if the command was executed successfully
                        if (result == -1 || result == 256) {
                            // app_log((char *)"[ERROR] Failed sync time from server");
                            // perror("[ERROR] Failed sync time from server");
                        } else {
                            // g_print("[INFO] Time set to: %s\n", time_from_server);
                            char *v6;
                            asprintf(&v6, "[INFO] Sync time from server => %s\n", timeFromServer);
                            app_log(v6);
                            free(v6);
                            printf("[INFO] Sync time from server => %s\n", timeFromServer);
                        }
                    }
                }
            }
            cJSON_Delete(json);
        }
        curl_easy_cleanup(curl);
    }
}

// Shift
void 
cek_shift()
{
	time_t now = time(NULL);
	struct tm *timeinfo = localtime(&now);
	char waktu[9];
	char *v7;
	strftime(waktu, sizeof(waktu), "%H:%M:%S", timeinfo);

	if (strcmp(waktu, "06:00:00") >= 0 && strcmp(waktu, "13:59:59") <= 0)
	{
		shift = 1;
	}
	else if (strcmp(waktu, "14:00:00") >= 0 && strcmp(waktu, "21:59:59") <= 0)
	{
		shift = 2;
	}
	else if (strcmp(waktu, "22:00:00") >= 0)
	{
		shift = 3;
	}
	else if (strcmp(waktu, "05:59:59") <= 0)
	{
		shift = 3;
	}

	asprintf(&v7, "[INFO] Waktu shift : %d\n", shift);
	app_log(v7);
	printf("[INFO] Waktu shift : %d\n", shift);
}

// Set LED
void 
set_led(char *port, char *xstr)
{
	int fd;
	int wlen;
	// char *xstr = "LD1T\n";
	int xlen = strlen(xstr);

	fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0)
		printf("[ERROR] Error opening %s: %s\n", port, strerror(errno));
	set_interface_attribs(fd, B9600);
	// set_mincount(fd, 0);
	wlen = write(fd, xstr, xlen);
	if (wlen != xlen)
		printf("[ERROR] Error from write LED: %d, %d\n", wlen, errno);
	tcdrain(fd);
	tcflush(fd, TCIOFLUSH);
	while (1)
	{
		char buffer[100];
		ssize_t length = read(fd, &buffer, sizeof(buffer));
		if (length == -1)
		{
			printf("[ERROR] Error set LED\n");
			break;
		}
		else if (length == 0)
		{
			printf("[INFO] No more data\n");
			break;
		}
		else
		{
			buffer[length] = '\0';
			// printf("LED: %s", buffer);
			break;
		}
	}
	close(fd);
}

// set Mode Manless
void 
set_mode(char *mode)
{
	char *v2, *v3;
	if (strcmp(mode, "Online") == 0)
	{
		app_log((char *)"|                Mode: Online        		      |\n");
		asprintf(&v2, "|                Version %s @ %s        |\n", app_version, app_builddate);
		app_log(v2);
		free(v2);
		app_log((char *)"====================================================\n");
		asprintf(&v3, "[INFO] Server '%s' : is up!\n", ip_server);
		free(v3);
		app_log((char *)"[INFO] App berhasil terhubung ke server...\n");
		app_log((char *)"[INFO] App => Online Mode...\n");

		printf("|                Mode: Online        		   |\n");
		printf("|                Version %s @ %s        |\n", app_version, app_builddate);
		printf("====================================================\n");
		printf("[INFO] Server '%s' : is up!\n", ip_server);
		printf("[INFO] App berhasil terhubung ke server...\n");
		printf("[INFO] App => Online Mode...\n");
		statusManless = 1;
		set_led(linuxSerialController, (char *)"LD7T\n");
	}
	else
	{
		app_log((char *)"|                Mode: Offline        			   |\n");
		asprintf(&v2, "|                Version %s @ %s        |\n", app_version, app_builddate);
		app_log(v2);
		free(v2);
		app_log((char *)"====================================================\n");
		asprintf(&v3, "[INFO] Server '%s' : is down!\n", ip_server);
		free(v3);
		app_log((char *)"[INFO] App gagal terhubung ke server...\n");
		app_log((char *)"[INFO] App => Offline Mode...\n");

		printf("|                Mode: Offline        			   |\n");
		printf("|                Version %s @ %s        |\n", app_version, app_builddate);
		printf("====================================================\n");
		printf("[ERROR] Server '%s' : is down!\n", ip_server);
		printf("[INFO] App gagal terhubung ke server...\n");
		printf("[INFO] App => Offline Mode...\n");
		statusManless = 2;
		set_led(linuxSerialController, (char *)"LD7F\n");
	}
}

// Print Ticket
int print_ticket(char *port, int baudrate, char *notiket, char *qr, char *waktumasuk, char *kodetiket, int mode)
{
	escpos_printer *printer = escpos_printer_serial(port, baudrate);
	if (printer != NULL)
	{
		// int w, h;
		// unsigned char *image_data = load_image("image.jpg", &w, &h);
		// escpos_printer_image(printer, image_data, w, h);

		// printf("%s\n", qr);
		// printf("%s\n", waktumasuk);

		char ESC = '\x1b';
		char GS = '\x1d';
		char LF = '\n';
		char HT = '\x09';

		char initprinter[] = {ESC + '@'};
		char cutpaper[] = {GS + 'V' + '\x00'};
		char widtharea[] = {GS + '\x57' + '\xff' + '\x01'};
		char leftmargin[] = {GS + '\x4c' + '\x25' + '\x00'};
		char center[] = {ESC + '\x61' + '\x01'};
		char left[] = {ESC + '\x61' + '\x00'};
		char right[] = {ESC + '\x61' + '\x02'};
		char font_a[] = {ESC + '\x21' + '\x00'};
		char font_b[] = {ESC + '\x21' + '\x01'};
		char font_header[] = {ESC + '\x4d' + '\x30'};
		char font_normal[] = {ESC + '\x4d' + '\x31'};
		char character_size[] = {GS + '\x21' + '\x10'};
		char doublewidth_on[] = {ESC + '\x21' + '\x20'};
		char doublewheigt_on[] = {ESC + '\x21' + '\x10'};
		char doublewidth_off[] = {ESC + '\x21' + '\x00'};
		char emphasized_on[] = {ESC + '\x45' + '\x01'};
		char emphasized_off[] = {ESC + '\x45' + '\x00'};

		char qrtiket[13];
		sprintf(qrtiket, "%s", notiket);
		// printf("%d\n", sizeof(qrtiket));
		int storelen = sizeof(qrtiket) + 3;
		char store_pl = storelen % 256;
		char store_ph = storelen / 256;
		char set_qrmodel[] = {GS, '\x28', '\x6B', '\x04', '\x00', '\x31', '\x41', '\x32', '\x00'};
		char set_qrsize[] = {GS, '\x28', '\x6B', '\x03', '\x00', '\x31', '\x43', '\x07'};
		char set_qrcorrectionlevel[] = {GS, '\x28', '\x6B', '\x03', '\x00', '\x31', '\x45', '\x30'};
		char set_qrstoredata[] = {GS, '\x28', '\x6B', store_pl, store_ph, '\x31', '\x50', '\x30'};
		char print_qr[] = {GS, '\x28', '\x6B', '\x03', '\x00', '\x31', '\x51', '\x30'};

		escpos_printer_raw(printer, "\x1b\x40", sizeof("\x1b\x40"));				 // init
		escpos_printer_raw(printer, "\x1d\x57\xff\x01", sizeof("\x1d\x57\xff\x01")); // width area
		escpos_printer_raw(printer, "\x1d\x4c\x25\x00", sizeof("\x1d\x4c\x25\x00")); // left margin
		escpos_printer_raw(printer, "\x1b\x61\x01", sizeof("\x1b\x61\x01"));		 // center
		escpos_printer_raw(printer, "\x1b\x4d\x30", sizeof("\x1b\x4d\x30"));		 // font header
		escpos_printer_raw(printer, "\x1b\x45\x01", sizeof("\x1b\x45\x01"));		 // emphasized on
		escpos_printer_raw(printer, nama_lokasi, sizeof(nama_lokasi));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, alamat_lokasi, sizeof(alamat_lokasi));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "==========================================", sizeof("=========================================="));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\x1b\x4d\x31", sizeof("\x1b\x4d\x31")); // font normal
		escpos_printer_raw(printer, "\x1b\x61\x00", sizeof("\x1b\x61\x00")); // left
		char baris1[50 + strlen(app_version)];
		sprintf(baris1, "Smart Vision Parking                  Black Box v.%s", app_version);
		escpos_printer_raw(printer, baris1, sizeof(baris1));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\x1b\x45\x00", sizeof("\x1b\x45\x00")); // emphasized off
		escpos_printer_raw(printer, "\x1b\x4d\x30", sizeof("\x1b\x4d\x30")); // font header
		char baris2[8 + strlen(gate_kind)];
		if (mode == 1)
		{
			sprintf(baris2, "PM %s* - %s", id_manless, gate_kind);
			// printf("%s\n", baris2);
			escpos_printer_raw(printer, baris2, sizeof(baris2));
		}
		else
		{
			sprintf(baris2, "PM %s# - %s", id_manless, gate_kind);
			escpos_printer_raw(printer, baris2, sizeof(baris2));
		}
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\x1b\x4d\x31", sizeof("\x1b\x4d\x31")); // font normal
		char baris3[strlen(waktumasuk)];
		sprintf(baris3, "%s", waktumasuk);
		escpos_printer_raw(printer, baris3, sizeof(baris3));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\x1b\x61\x01", sizeof("\x1b\x61\x01")); // center
		escpos_printer_raw(printer, "\x1b\x21\x20", sizeof("\x1b\x21\x20")); // double width on
		escpos_printer_raw(printer, "\x1b\x45\x01", sizeof("\x1b\x45\x01")); // emphasized on
		escpos_printer_raw(printer, kodetiket, sizeof(kodetiket));
		escpos_printer_raw(printer, "\x1b\x21\x00", sizeof("\x1b\x21\x00")); // double width off
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, set_qrmodel, sizeof(set_qrmodel));					   // qr model
		escpos_printer_raw(printer, set_qrsize, sizeof(set_qrsize));					   // qr size
		escpos_printer_raw(printer, set_qrcorrectionlevel, sizeof(set_qrcorrectionlevel)); // qr correction level
		escpos_printer_raw(printer, set_qrstoredata, sizeof(set_qrstoredata));			   // qr calculate to store data
		escpos_printer_raw(printer, qrtiket, sizeof(qrtiket));									   // qr data
		escpos_printer_raw(printer, print_qr, sizeof(print_qr));						   // qr print
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\x1b\x21\x20", sizeof("\x1b\x21\x20")); // double width on
		char baris4[17];
		sprintf(baris4, "%c%c%c %c%c%c%c %c%c%c%c %c%c", notiket[0], notiket[1], notiket[2], notiket[3], notiket[4], notiket[5], notiket[6], notiket[7], notiket[8], notiket[9], notiket[10], notiket[11], notiket[12]);
		escpos_printer_raw(printer, baris4, sizeof(baris4));
		escpos_printer_raw(printer, "\x1b\x21\x00", sizeof("\x1b\x21\x00")); // double width off
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		if (strcmp(nopol, "") != 0)
		{
			char baris4[9 + strlen(nopol)];
			sprintf(baris4, "Nopol : %s", nopol);
			escpos_printer_raw(printer, "\x1b\x4d\x30", sizeof("\x1b\x4d\x30")); // font header
			escpos_printer_raw(printer, "\x1b\x45\x01", sizeof("\x1b\x45\x01")); // emphasized on
			escpos_printer_raw(printer, baris4, sizeof(baris4));
			escpos_printer_raw(printer, "\x1b\x45\x00", sizeof("\x1b\x45\x00")); // emphasized off
			escpos_printer_raw(printer, "\n", sizeof("\n"));
		}
		escpos_printer_raw(printer, "------------------------------------------", sizeof("------------------------------------------"));
		escpos_printer_raw(printer, "\n", sizeof("\n"));
		escpos_printer_raw(printer, "\x1b\x4d\x31", sizeof("\x1b\x4d\x31")); // font normal
		escpos_printer_raw(printer, pesan_masuk, sizeof(pesan_masuk));

		escpos_printer_feed(printer, 3);
		escpos_printer_cut(printer, 1);
		escpos_printer_destroy(printer);

		memset(qrtiket, 0 , sizeof(qrtiket));
		memset(set_qrmodel, 0, sizeof(set_qrmodel));
		memset(set_qrsize, 0, sizeof(set_qrsize));
		memset(set_qrcorrectionlevel, 0, sizeof(set_qrcorrectionlevel));
		memset(print_qr, 0, sizeof(print_qr));
		return 0;
	}
	else
	{
		escpos_error err = last_error;
		printf("[ERROR] Print error code : %d\n", err);
		return 1;
	}
}

// Open Relay
int 
open_relay(char *port, char *xstr)
{
	// set LED 4
	//set_led(linuxSerialController, "LD4T");
	//msleep(100);

	int fd;
	int wlen;
	// char *xstr = "LD1T\n";
	int xlen = strlen(xstr);

	fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0)
		printf("[ERROR] Error opening %s: %s\n", port, strerror(errno));
	set_interface_attribs(fd, B9600);
	// set_mincount(fd, 0);
	wlen = write(fd, xstr, xlen);
	if (wlen != xlen)
		printf("[ERROR] Error from write: %d, %d\n", wlen, errno);
	tcdrain(fd);
	tcflush(fd, TCIOFLUSH);
	while (1)
	{
		char buffer[100];
		ssize_t length = read(fd, &buffer, sizeof(buffer));
		if (length == -1)
		{
			printf("[ERROR] Error while open the relay\n");
			break;
		}
		else if (length == 0)
		{
			printf("[INFO] No more data\n");
			break;
		}
		else
		{
			buffer[length] = '\0';
			printf("%s\n", buffer);
			app_log((char *)"[INFO] Relay -> Open\n");
			printf("[INFO] Relay -> Open\n");
			// if (buffer[0] == 'R' && buffer[1] == 'E')
			// {
			// 	if (buffer[0] == '1' && buffer[1] == 'T')
			// 	{
			// 		printf("[INFO] Relay -> True\n");
			// 	}
			// }
			//msleep(100);
			//write(fd, "LD4F", strlen("LD4F"));
			msleep(1000);
			write(fd, "OT1F\n", strlen("OT1F\n"));
            msleep(100);
            write(fd, "OT1F\n", strlen("OT1F\n"));
            msleep(100);
            write(fd, "OT1F\n", strlen("OT1F\n"));
			app_log((char *)"[INFO] Relay -> Close\n");
			printf("[INFO] Relay -> Close\n");
			break;
		}
	}
	close(fd);
	return 0;
}

// Create Ticket FULL API version
int 
create_ticket()
{
	CURL *curl;
	CURLcode res;
	bool response;
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct timespec start_t, end_t;
    double elapsed_time;
	struct MemoryStruct chunk;
    chunk.memory = (char *)malloc(1);  // Initial memory allocation
    chunk.size = 0;            // Initial size
    char *tolog;
	char *v1, *v2, *v3;
	char *message;
	char url[100];
	char waktumasuk[20];
	char noid[30];
	char ticketcode[2];

	msleep(100);
	set_led(linuxSerialController, (char *)"LD2T");
	isIdle = 0;

	time_t rawtime;
	struct tm *timeinfo;
	time(&rawtime);
	timeinfo = localtime(&rawtime);
	// printf("Current local time and date: %s", asctime(timeinfo));
	// printf("%02d:%02d:%02d", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);

	int getTanggal = timeinfo->tm_mday;
	int getBulan = timeinfo->tm_mon + 1;
	int getTahun = timeinfo->tm_year + 1900;
	int getJam = timeinfo->tm_hour;
	int getMenit = timeinfo->tm_min;

	app_log((char *)"============================================\n");
	asprintf(&v1, "     Transaction @ %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	app_log(v1);
	free(v1);
	app_log((char *)"============================================\n");

	printf("============================================\n");
	printf("     Transaction @ %04d-%02d-%02d %02d:%02d:%02d\n", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
	printf("============================================\n");

	printf("[INFO] Tipe -> %d\n", typeTrans);
	if(typeTrans == 1) {
		sprintf(nokartu, "%s", "-");
	}

	// set nopol
	sprintf(nopol, "%s", "");
	asprintf(&v2, "[INFO] Nopol -> %s\n", nopol);
	app_log(v2);
	free(v2);
	printf("[INFO] Nopol -> %s\n", nopol);

	// shift
	char myshift[2];
	sprintf(myshift, "%d", shift);

	// mode
	int mode;
	int is_server = ping(ip_server);
	if (is_server == 0)
	{
		mode = 1;
	} else {
		mode = 0;
	}

	// processing time
	clock_gettime(CLOCK_MONOTONIC, &start_t);
	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if (curl)
	{
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "id_lokasi",
					 CURLFORM_COPYCONTENTS, id_location,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "posmasuk",
					 CURLFORM_COPYCONTENTS, id_manless,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "kasirmasuk",
					 CURLFORM_COPYCONTENTS, gate_name,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "jenis_kendaraan",
					 CURLFORM_COPYCONTENTS, gate_kind,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "jenis_langganan",
					 CURLFORM_COPYCONTENTS, jenis_langganan,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "shift",
					 CURLFORM_COPYCONTENTS, myshift,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "nopol",
					 CURLFORM_COPYCONTENTS, nopol,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "nokartu",
					 CURLFORM_COPYCONTENTS, nokartu,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "ip_cam",
					 CURLFORM_COPYCONTENTS, ip_camera_1,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "user_cam",
					 CURLFORM_COPYCONTENTS, ip_camera_1_user,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "password_cam",
					 CURLFORM_COPYCONTENTS, ip_camera_1_password,
					 CURLFORM_END);
		curl_formadd(&formpost,
					 &lastptr,
					 CURLFORM_COPYNAME, "model_cam",
					 CURLFORM_COPYCONTENTS, ip_camera_1_model,
					 CURLFORM_END);

		sprintf(url, "http://%s:5000/business-parking/api/v1/ticket/create", ip_server);
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERNAME, AUTH_USER);
		curl_easy_setopt(curl, CURLOPT_PASSWORD, AUTH_PASSWORD);
    	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 2);
		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
		{
            response = -1;
            asprintf(&tolog, "[ERROR][API] Create ticket failed: %s\n", curl_easy_strerror(res));
			app_log(tolog);
			free(tolog);
			fprintf(stderr, "[ERROR][API] Create ticket failed: %s\n", curl_easy_strerror(res));
		} else {
			printf("[INFO] Response create ticket: %s\n", chunk.memory);

			// Parse JSON response
			cJSON *json = cJSON_Parse(chunk.memory);
			if (json == NULL) {
				const char *error_ptr = cJSON_GetErrorPtr();
				if (error_ptr != NULL) {
					fprintf(stderr, "[ERROR] Error before: %s\n", error_ptr);
				}
			} else {
				// Accessing values from JSON
				cJSON *status = cJSON_GetObjectItem(json, "success");
				int ret = cJSON_IsTrue(status);
				cJSON *j_message = cJSON_GetObjectItemCaseSensitive(json, "message");
				if (cJSON_IsString(j_message) && (j_message->valuestring != NULL)) {
					asprintf(&message, "%s", j_message->valuestring);
				}
				cJSON *j_data = cJSON_GetObjectItemCaseSensitive(json, "data");
				if (ret == 1) {
					cJSON *j_waktumasuk = cJSON_GetObjectItemCaseSensitive(j_data, "waktumasuk");
                    if (cJSON_IsString(j_waktumasuk) && (j_waktumasuk->valuestring != NULL)) {
                        sprintf(waktumasuk, "%s", j_waktumasuk->valuestring);
                    }
					cJSON *j_noid = cJSON_GetObjectItemCaseSensitive(j_data, "noid");
                    if (cJSON_IsString(j_noid) && (j_noid->valuestring != NULL)) {
                        sprintf(noid, "%s", j_noid->valuestring);
                    }
					cJSON *j_ticket = cJSON_GetObjectItemCaseSensitive(j_data, "ticket");
                    if (cJSON_IsString(j_ticket) && (j_ticket->valuestring != NULL)) {
                        sprintf(notiket, "%s", j_ticket->valuestring);
                    }
					cJSON *j_ticket_qr = cJSON_GetObjectItemCaseSensitive(j_data, "ticket-qr");
                    if (cJSON_IsString(j_ticket_qr) && (j_ticket_qr->valuestring != NULL)) {
                        sprintf(notiketQr, "%s", j_ticket_qr->valuestring);
                    }
					cJSON *j_ticket_code = cJSON_GetObjectItemCaseSensitive(j_data, "ticket-code");
                    if (cJSON_IsString(j_ticket_code) && (j_ticket_code->valuestring != NULL)) {
						// ticketcode[0] = '\0';
						sprintf(ticketcode, "%s", j_ticket_code->valuestring);
                    }
                    cJSON *j_capture_status = cJSON_GetObjectItem(j_data, "capture_image");
					int capture_status = cJSON_IsTrue(j_capture_status);
					if(capture_status == 0) {
						app_log((char *)"[ERROR] Failed capture image, please check the camera\n");
						printf("[ERROR] Failed capture image, please check the camera\n");
					}
					// save to database
					store_local(waktumasuk, id_manless, gate_name, gate_kind, noid, jenis_langganan, myshift, notiket, nopol, nokartu);
					// print the ticket
                    if (typeTrans == 1) {
    					int result = print_ticket(linuxSerialPrinter, atoi(printer_baudrate), notiket, notiketQr, waktumasuk, ticketcode, mode);
    					if (result == 1) {
    						app_log((char *)"[ERROR] Failed print a ticket!\n");
    						printf("[ERROR] Failed print a ticket!\n");
    					}
                    }
					set_led(linuxSerialController, (char *)"LD3F\n");
					msleep(100);
					// open relay
					open_relay(linuxSerialController, (char *)"OT1T\n");

					// bypass loop 2
					isIdle = 0;
					printf("[INFO] Loop final -> Ok!\n");
					// Get the end time
					clock_gettime(CLOCK_MONOTONIC, &end_t);
					// Calculate the elapsed time
					elapsed_time = getTimeDifference(start_t, end_t);
					// Print the processing time
					asprintf(&v3, "[INFO] Processing time: %.2f seconds\n", elapsed_time);
					app_log(v3);
					free(v3);
					printf("[INFO] Processing time: %.2f seconds\n", elapsed_time);
					msleep(100);
					write(fd, "LD2F", strlen("LD2F"));
                    if (typeTrans == 0) pthread_create(&ptd_stop, NULL, tapstop, (void *)"1");
					sleep(1);

                    memset(bank, 0, strlen(bank));
                    // memset(nokartu, 0, strlen(nokartu));
					memset(waktumasuk, 0, strlen(waktumasuk));
					memset(notiket, 0, strlen(notiket));
					memset(notiketQr, 0, strlen(notiketQr));
					response = true;
                    countBlocked = 0;
                    countNotBlocked = 0;
					typeTrans = 0;
				} else {
					printf("[ERROR] %s", message);
					response = false;
				}
				cJSON_Delete(json);
			}
		}
		free(message);
		curl_easy_cleanup(curl);
		curl_formfree(formpost);
	}
	return 0;
}

// IO
void 
controller(char *port, int baudrate, char *xstr)
{
	pthread_t ptid1, ptid2;
	int wlen;
	int xlen = strlen(xstr);

	fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
	if (fd < 0) printf("[ERROR] Error opening Controller %s: %s\n", port, strerror(errno));
	switch (baudrate)
	{
		case 9600:
			set_interface_attribs(fd, B9600);
			break;
		case 19200:
			set_interface_attribs(fd, B19200);
			break;
		case 38400:
			set_interface_attribs(fd, B38400);
			break;
		default:
			set_interface_attribs(fd, B9600);
			break;
	}
	// set_mincount(fd, 0);
	// sleep(1);
	tcflush(fd, TCIOFLUSH);
	while (1)
	{
		wlen = write(fd, xstr, xlen);
		if (wlen != xlen) printf("[ERROR] Error from write Controller: %d, %d\n", wlen, errno);
		tcdrain(fd); /* delay for output */

		char buffer[100];
		ssize_t length = read(fd, &buffer, sizeof(buffer));
		if (length == -1)
		{
			// printf("[WARNING][CONTROLLER] Error reading from serial port\n");
		}
		else if (length == 0)
		{
			app_log((char *)"[INFO][CONTROLLER] Cant get data, please restart!\n");
			printf("[INFO][CONTROLLER] Cant get data, please restart!\n");
			msleep(100);
			write(fd, "LD8F", strlen("LD8F"));
			break;
		}
		else
		{
			buffer[length] = '\0';
			printf("%s", buffer);
			if (buffer[0] == 'O' && buffer[1] == 'P')
			{
				// loop 1 kendaraan
                if (buffer[3] == '1' && buffer[4] == 'T')
                {
                    if (isIdle == 1) { 
                        printf("[INFO] Mohon tekan tombol atau tap kartu anda ...\n");
                        if (isTapin) {
                            printTicket = false;
                            pthread_create(&ptd_balance, NULL, tapin, (void *)"1");
                        }
                    }
                    // led 1
                    msleep(100);
                    write(fd, "LD1T", strlen("LD1T"));

                    // tekan tombol
                    if (buffer[9] == '3' && buffer[10] == 'T' && isIdle == 1)
                    {
						typeTrans = 1;
                        isTapin = false;
                        printTicket = true;
                        msleep(100);
                        write(fd, "LD3T", strlen("LD3T"));
                        sprintf(jenis_langganan, "CASUAL %s", gate_kind);
                        pthread_create(&ptd_stop, NULL, tapstop, (void *)"1");
                        create_ticket();
                    }
                }
                else
                {
                    isIdle = 1;
                    if (!isTapin) {
                        isTapin = true;
                    }
                    msleep(100);
                    write(fd, "LD1F", strlen("LD1F"));
                }
			}
		}
		msleep(atoi(delay_loop));
	}
	close(fd);
	app_log((char *)"[INFO] Application is closed!\n");
	app_log((char *)"----------------------------------------------------\n");
	printf("[INFO] Application is closed!\n");
	printf("----------------------------------------------------\n");
}

// Signal
void 
handle_signt(int sig)
{
	pid_t pid;
	printf("[INFO] Caught signal -> %d\n", sig);
    pthread_create(&ptd_stop, NULL, tapstop, (void *)"1");
	set_led(linuxSerialController, (char *)"LD1F");
	msleep(100);
	set_led(linuxSerialController, (char *)"LD2F");
	msleep(100);
	set_led(linuxSerialController, (char *)"LD3F");
	msleep(100);
	set_led(linuxSerialController, (char *)"LD7F");
	msleep(100);
	set_led(linuxSerialController, (char *)"LD8F");
	pid = getpid(); // Process ID of itself
	app_log((char *)"[INFO] Application is closed!\n");
	app_log((char *)"----------------------------------------------------\n");
	printf("[INFO] Application is closed!\n");
	printf("----------------------------------------------------\n");
	kill(pid, SIGUSR1); // Send SIGUSR1 to itself
}

int 
main(int argc, char *argv[])
{
	CURL *curl;
	CURLcode cres;
	sqlite3 *DB;
	sqlite3_stmt *res;
	const char *tail;
	char *zErrMsg = 0;
	const char *sqlitedata = "Sqlite Callback Function Called";

	int ret = 0;
	int mode = 0;
	int select = 0;
	int count = 0;

	char passwd[256] = "123456";
	memset(passwd, 0, sizeof(passwd));

#ifdef _WIN32
	SetConsoleCtrlHandler(handle_sig, TRUE);
#else
	signal(SIGINT, handle_signt);
	// signal(SIGTERM, handle_signt);
	// signal(SIGQUIT, handle_signt);
	// signal(SIGTSTP, handle_signt);
#endif

	// Config ========================================================================================
	int sqlite = sqlite3_open(DB_LOCAL, &DB);
	if (sqlite)
		fprintf(stderr, "[ERROR] Can't open database: %s\n", sqlite3_errmsg(DB));

	if (sqlite3_prepare_v2(DB, "SELECT * FROM config WHERE status=1", 128, &res, &tail) != SQLITE_OK)
	{
		sqlite3_close(DB);
		printf("[ERROR] Can't retrieve data: %s\n", sqlite3_errmsg(DB));
		return (1);
	}

    printf("[INFO] Initializing...\n");
	// printf("%8s | %8s | %8s\n", "id", "server");

	while (sqlite3_step(res) == SQLITE_ROW)
	{
//		 printf("[CONFIG] %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s | %8s\n",
//		 	sqlite3_column_text(res, 0),
//		 	sqlite3_column_text(res, 1),
//		 	sqlite3_column_text(res, 2),
//		 	sqlite3_column_text(res, 3),
//		 	sqlite3_column_text(res, 4),
//		 	sqlite3_column_text(res, 5),
//		 	sqlite3_column_text(res, 6),
//		 	sqlite3_column_text(res, 7),
//		 	sqlite3_column_text(res, 8),
//		 	sqlite3_column_text(res, 9),
//		 	sqlite3_column_text(res, 10),
//		 	sqlite3_column_text(res, 11),
//		 	sqlite3_column_text(res, 12),
//		 	sqlite3_column_text(res, 13),
//		 	sqlite3_column_text(res, 14),
//		 	sqlite3_column_text(res, 15),
//		 	sqlite3_column_text(res, 16),
//		 	sqlite3_column_text(res, 17),
//		 	sqlite3_column_text(res, 18),
//		 	sqlite3_column_text(res, 19),
//		 	sqlite3_column_text(res, 20),
//		 	sqlite3_column_text(res, 21),
//		 	sqlite3_column_text(res, 22),
//		 	sqlite3_column_text(res, 23),
//		 	sqlite3_column_text(res, 24),
//		 	sqlite3_column_text(res, 25),
//		 	sqlite3_column_text(res, 26),
//		 	sqlite3_column_text(res, 27),
//		 	sqlite3_column_text(res, 28),
//		 	sqlite3_column_text(res, 29),
//		 	sqlite3_column_text(res, 30),
//		 	sqlite3_column_text(res, 31),
//		 	sqlite3_column_text(res, 32),
//		 	sqlite3_column_text(res, 33),
//		 	sqlite3_column_text(res, 34),
//		 	sqlite3_column_text(res, 35));

        sprintf(id_config, "%s", sqlite3_column_text(res, 0));
		sprintf(ip_server, "%s", sqlite3_column_text(res, 1));
		sprintf(ip_manless, "%s", sqlite3_column_text(res, 2));
		sprintf(id_manless, "%s", sqlite3_column_text(res, 3));
		sprintf(id_location, "%s", sqlite3_column_text(res, 4));
		sprintf(path_image, "%s", sqlite3_column_text(res, 5));
		sprintf(gate_name, "%s", sqlite3_column_text(res, 6));
		sprintf(gate_kind, "%s", sqlite3_column_text(res, 7));
		sprintf(printer_port, "%s", sqlite3_column_text(res, 8));
		sprintf(printer_baudrate, "%s", sqlite3_column_text(res, 9));
		sprintf(controller_port, "%s", sqlite3_column_text(res, 10));
		sprintf(controller_baudrate, "%s", sqlite3_column_text(res, 11));
		sprintf(delay_loop, "%s", sqlite3_column_text(res, 12));
		sprintf(delay_led, "%s", sqlite3_column_text(res, 13));
		sprintf(paper_warning, "%s", sqlite3_column_text(res, 14));
		sprintf(service_host, "%s", sqlite3_column_text(res, 15));
		sprintf(service_port, "%s", sqlite3_column_text(res, 16));
		sprintf(reader_id, "%s", sqlite3_column_text(res, 17));
		sprintf(ip_camera_1, "%s", sqlite3_column_text(res, 18));
		sprintf(ip_camera_1_user, "%s", sqlite3_column_text(res, 19));
		sprintf(ip_camera_1_password, "%s", sqlite3_column_text(res, 20));
		sprintf(ip_camera_1_model, "%s", sqlite3_column_text(res, 21));
		sprintf(ip_camera_1_status, "%s", sqlite3_column_text(res, 22));
		sprintf(ip_camera_2, "%s", sqlite3_column_text(res, 23));
		sprintf(ip_camera_2_user, "%s", sqlite3_column_text(res, 24));
		sprintf(ip_camera_2_password, "%s", sqlite3_column_text(res, 25));
		sprintf(ip_camera_2_model, "%s", sqlite3_column_text(res, 26));
		sprintf(ip_camera_2_status, "%s", sqlite3_column_text(res, 27));
		sprintf(lpr_engine, "%s", sqlite3_column_text(res, 28));
		sprintf(lpr_engine_status, "%s", sqlite3_column_text(res, 29));
		sprintf(face_engine, "%s", sqlite3_column_text(res, 30));
//        printf("%s\n", face_engine);
		sprintf(face_engine_status, "%s", sqlite3_column_text(res, 31));
		sprintf(nama_lokasi, "%s", sqlite3_column_text(res, 32));
		sprintf(alamat_lokasi, "%s", sqlite3_column_text(res, 33));
		sprintf(pesan_masuk, "%s", sqlite3_column_text(res, 34));
        sprintf(bank_active, "%s", sqlite3_column_text(res, 35));
		sprintf(status_config, "%s", sqlite3_column_text(res, 36));
		count++;
	}
//	printf("[INFO] Rows count: %d\n", count);
	sqlite3_finalize(res);
	// ===============================================================================================

	sprintf(linuxSerialController, "/dev/ttyS%s", controller_port);
	sprintf(linuxSerialPrinter, "/dev/ttyS%s", printer_port);
	isIdle = 1;
	set_led(linuxSerialController, (char *)"LD8T");
	msleep(100);

	// Migration =====================================================================================
	// migrate_transaction((void *)"0");
	// ===============================================================================================

	app_log((char *)"====================================================\n");
	if(SDK == 1){
		app_log((char *)"|                Pos Masuk [Manless][SDK]          |\n");
	} else {
		app_log((char *)"|                Pos Masuk [Manless]               |\n");
	}
	char *v1;
	asprintf(&v1, "|                Gate: %s                        |\n", gate_name);
	app_log(v1);
	free(v1);

	printf("====================================================\n");
	if(SDK == 1){
		printf("|                Pos Masuk [Manless][SDK]          |\n");
	} else {
		printf("|                Pos Masuk [Manless]               |\n");
	}
	printf("|                Gate: %s                        |\n", gate_name);

	// Server ========================================================================================
	int is_server = ping(ip_server);
	if (is_server == 0)
	{
		set_mode((char *)"Online");
	}
	else
	{
		set_mode((char *)"Offline");
	}
	// ===============================================================================================

	// LPR ===========================================================================================
	if (SDK == 1)
	{
		if (strcmp(ip_camera_1_status, "1") == 0)
		{
//			if (is_valid_ip(ip_camera_1, 0) != 1)
//			{
//				printf("[ERROR][LPR] Ip is invalid!\n");
//				return 0;
//			}
//
//			memcpy(g_ip, ip_camera_1, strlen(ip_camera_1));
//
//			ICE_IPCSDK_SetDeviceEventCallBack(OnDeviceEvent, NULL, 0);
//
//			// Connect
//			if (mode == 0 || mode == 2)
//			{
//				hSDK = ICE_IPCSDK_OpenDevice(ip_camera_1);
//			}
//			else
//			{
//				hSDK = ICE_IPCSDK_OpenDevice_Passwd(ip_camera_1, passwd);
//			}
//
//			if (NULL == hSDK)
//			{
//				printf("[INFO] LPR Engine [%s] => Connect failed!\n", ip_camera_1);
//				// goto exit;
//			}
//			else
//			{
//				printf("[INFO] LPR Engine [%s] => Connect success!\n", ip_camera_1);
//			}
//
//			if (2 == mode)
//			{
//				ICE_IPCSDK_SetDecPwd(hSDK, passwd);
//			}
		}
		else
		{
			printf("[INFO] LPR Engine is disabled in your configuration!\n");
		}
	}
	// ===============================================================================================

	// Counter =======================================================================================
	// const char *sTicketCounter = ticket_counter();
	// char *v4;
	// asprintf(&v4, "[INFO] Last counter => %s\n", sTicketCounter);
	// app_log(v4);
	// printf("[INFO] Last counter => %s\n", sTicketCounter);
	// sprintf(lastTicketCounter, "%03d", atoi(sTicketCounter) + 1);
	// printf("[INFO] Next Ticket => %s\n", lastTicketCounter);
	// ===============================================================================================

	// Parking Available =============================================================================
	// parking_available(atoi(sTicketCounter));
	// ===============================================================================================

	// Service ======================================================================================
	char *v6;
	asprintf(&v6, "[INFO] Service => %s:%s\n", service_host, service_port);
	app_log(v6);
	free(v6);
	printf("[INFO] Service => %s:%s\n", service_host, service_port);
	// ===============================================================================================

	// Time Sync =====================================================================================
	time_sync();
	// ===============================================================================================

	// Shift =========================================================================================
	cek_shift();
	// ===============================================================================================

    // Bank Support ==================================================================================
	char *v5;
	asprintf(&v5, "[INFO] Bank Support : %s\n", bank_active);
	app_log(v5);
	free(v5);
	printf("[INFO] Bank Support : %s\n", bank_active);
	// ===============================================================================================

	// Migration =====================================================================================
	// pthread_t ptid;
	// pthread_create(&ptid, NULL, migrate_transaction, (void *)"1");
	// ===============================================================================================

	// IO Controller =================================================================================
	if (is_server == 0)
	{
		controller(linuxSerialController, atoi(controller_baudrate), (char *)"IN?\n");
	}
	else
	{
		printf("[INFO] Failed starting application ...\n");
		printf("[INFO] Application restarting in 5 seconds\n\n");
		sleep(5);
		main(argc - 1, argv);
	}
	// ===============================================================================================

	// printf("SQLITE Version %s\n", sqlite3_libversion());
	// const char *testingjson = tapin(service_host, atoi(service_port), 10);
	// printf("testing: %s\n", testingjson);
exit:

	return 0;
}