#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <ctime>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <nlohmann/json.hpp>

#include "serial.h"
#include "printer.h"
#include "constants.h"

#define DB_LOCAL "local.db"

using json = nlohmann::json;
using namespace nlohmann::literals;

// List of Update:
// 1.0.0 = Basic Version
// 1.0.1 = Add E-money

// Compile g++ setting.cpp -o setting -l sqlite3

static char app_version[] = "1.0.1";
static char app_builddate[] = "28/05/2024";
static int g_run = 1;
std::string id_config = "1";
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
char reader_id[2];
char ip_camera_1[50];
char ip_camera_1_user[50];
char ip_camera_1_password[50];
char ip_camera_1_model[50];
char ip_camera_1_status[1];
char ip_camera_2[50];
char ip_camera_2_user[50];
char ip_camera_2_password[50];
char ip_camera_2_model[50];
char ip_camera_2_status[1];
char lpr_engine[255];
char lpr_engine_status[1];
char face_engine[255];
char face_engine_status[1];
char nama_lokasi[50];
char alamat_lokasi[50];
char pesan_masuk[255];
char bank[255];


struct Config
{
    std::string server;
    std::string ip_manless;
    int id_manless;
};
Config config;

struct string
{
	char *ptr;
	size_t len;
};

// Serial
void init_string(struct string *s)
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

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct string *s)
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

int set_interface_attribs(int fd, int speed)
{
	struct termios tty;

	if (tcgetattr(fd, &tty) < 0)
	{
		printf("[ERROR] From tcgetattr: %s\n", strerror(errno));
		return -1;
	}

	cfsetospeed(&tty, (speed_t)speed);
	cfsetispeed(&tty, (speed_t)speed);

	tty.c_lflag &= ~ICANON; /* Set non-canonical mode */

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
	tty.c_cc[VTIME] = 100; /* Set timeout of 10.0 seconds */
	// tty.c_cc[VTIME] = 1;

	if (tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		printf("[ERROR] From tcsetattr: %s\n", strerror(errno));
		return -1;
	}
	return 0;
}

void set_mincount(int fd, int mcount)
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

extern escpos_error escpos_last_error();
extern escpos_error last_error = ESCPOS_ERROR_NONE;

escpos_printer *escpos_printer_serial(const char *const portname, const int baudrate)
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

int escpos_printer_config(escpos_printer *printer, const escpos_config *const config)
{
	assert(printer != NULL);
	assert(config != NULL);

	printer->config = *config;
	return 0;
}

void escpos_printer_destroy(escpos_printer *printer)
{
	assert(printer != NULL);

	close(printer->sockfd);
	free(printer);
}

int escpos_printer_raw(escpos_printer *printer, const char *const message, const int len)
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

int escpos_printer_cut(escpos_printer *printer, const int lines)
{
	char buffer[4];
	strncpy(buffer, ESCPOS_CMD_CUT, 3);
	buffer[3] = lines;
	return escpos_printer_raw(printer, buffer, sizeof(buffer));
}

int escpos_printer_feed(escpos_printer *printer, const int lines)
{
	assert(lines > 0 && lines < 256);

	char buffer[3];
	strncpy(buffer, ESCPOS_CMD_FEED, 2);
	buffer[2] = lines;
	return escpos_printer_raw(printer, buffer, sizeof(buffer));
}

void set_bit(unsigned char *byte, const int i, const int bit)
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
void calculate_padding(const int size, int *padding_l, int *padding_r)
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

void convert_image_to_bits(unsigned char *pixel_bits,
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

int escpos_printer_print(escpos_printer *printer,
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

int escpos_printer_image(escpos_printer *printer,
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

int device_io(char *port, int baudrate, int timeout)
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

static int sqlite_callback_1(void* data, int argc, char** argv, char** azColName)
{
    int i;
	const char *id;
    fprintf(stderr, "%s: ", (const char*)data);

    for (i = 0; i < argc; i++) {
		id = argv[0];
		id_config = id;
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }

	// if(stderr == NULL) printf("Null Data\n");

    // printf("\n");
    return 0;
}

static int sqlite_callback_2(void* data, int argc, char** argv, char** azColName)
{
    int i;
    fprintf(stderr, "%s: ", (const char*)data);

    for (i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }

    printf("\n");
    return 0;
}

int is_valid_ip(const char *ip, int index)
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

bool is_int(char const* s) {
    int n;
    int i;
    return sscanf(s, "%d %n", &i, &n) == 1 && !s[n];
}

int get_julian_day(int day, int month, int year) {
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + (12 * a) - 3;
    int jdn = day + (153 * m + 2) / 5 + 365 * y + y / 4 - y / 100 + y / 400 - 32045;
    return jdn;
}

int msleep(long tms)
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

int tapstop()
{
	sqlite3* DB;
	sqlite3_stmt *res;
	const char *tail;
    char* zErrMsg;
	int rc;
    int exit = sqlite3_open(DB_LOCAL, &DB);

	int valread;
	char host[15];
	char port[15];
	char readerid[2];
	char buffer[1024] = { 0 };
	rc = sqlite3_prepare_v2(DB, "SELECT service_host, service_port, reader_id FROM config WHERE status=1", 128, &res, &tail);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	while (sqlite3_step(res) == SQLITE_ROW)
	{
		sprintf(host, "%s", sqlite3_column_text(res, 0));
		sprintf(port, "%s", sqlite3_column_text(res, 1));
		sprintf(reader_id, "%s", sqlite3_column_text(res, 2));
	}

	// creating socket
	int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	// set timeout
	struct timeval tv;
	tv.tv_sec = 10;
	tv.tv_usec = 0;
	setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
	setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

	// specifying address
	sockaddr_in serverAddress;
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(atoi(port));
	serverAddress.sin_addr.s_addr = inet_addr(host);

	// sending connection request
	connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

	// sending data
	// const char* message = "Hello, server!";
	json message = {
		{"taptype", "stop"},
		{"id", atoi(reader_id)},
	};
	std::string j3 = message.dump();
	send(clientSocket, j3.c_str(), strlen(j3.c_str()), 0);
	valread = read(clientSocket, buffer, 1024 - 1);
	if (valread < 0) {
		printf("[ERROR] Socket Connection Time out\n");
		return -1;
	} else {
		printf("[INFO] Received from service => %s\n", buffer);
	}

	// closing socket
	close(clientSocket);
	return 0;
}

std::string generate_nopol(int length) {
    const std::string chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, chars.size() - 1);

    std::string result;
    for (int i = 0; i < length; ++i) {
        result += chars[dis(gen)];
    }

    return result;
}

std::string generate_ticket() {
    // Define global variables
    std::string codeTahun, codeBulan, codeTanggal, codeJam, codeMenit, codeCounter, count;

    // Get current time
    time_t now = time(0);
    tm *ltm = localtime(&now);

    // Get formatted time strings
    char waktuSekarang[20];
    strftime(waktuSekarang, sizeof(waktuSekarang), "%Y-%m-%d %H:%M:%S", ltm);

    char waktuSekarangAlt[15];
    strftime(waktuSekarangAlt, sizeof(waktuSekarangAlt), "%Y%m%d%H%M%S", ltm);

    char getTanggal[3];
    strftime(getTanggal, sizeof(getTanggal), "%d", ltm);

    char getBulan[3];
    strftime(getBulan, sizeof(getBulan), "%m", ltm);

    char getTahun[5];
    strftime(getTahun, sizeof(getTahun), "%Y", ltm);

    char getJam[3];
    strftime(getJam, sizeof(getJam), "%H", ltm);

    char getMenit[3];
    strftime(getMenit, sizeof(getMenit), "%M", ltm);

    // Code tahun
    codeTahun = std::string(getTahun).substr(2, 2);

    // Code bulan
    int bulan = std::stoi(getBulan);
    switch (bulan) {
        case 1: codeBulan = "1"; break;
        case 2: codeBulan = "2"; break;
        case 3: codeBulan = "3"; break;
        case 4: codeBulan = "4"; break;
        case 5: codeBulan = "5"; break;
        case 6: codeBulan = "6"; break;
        case 7: codeBulan = "7"; break;
        case 8: codeBulan = "8"; break;
        case 9: codeBulan = "9"; break;
        case 10: codeBulan = "A"; break;
        case 11: codeBulan = "B"; break;
        case 12: codeBulan = "C"; break;
    }

    // Code tanggal
    int tanggal = std::stoi(getTanggal);
    char tanggalCode = tanggal <= 9 ? '0' + tanggal : 'A' + (tanggal - 10);
    codeTanggal = tanggalCode;

    // Code jam
    int jam = std::stoi(getJam);
    char jamCode = jam <= 9 ? '0' + jam : 'A' + (jam - 10);
    codeJam = jamCode;

    // Code menit
    codeMenit = getMenit;

    // Counter ticket
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 999);
    count = std::to_string(dis(gen) + 1);
    count.insert(count.begin(), 3 - count.length(), '0'); // ensure 3-digit format

    std::cout << "[INFO] ticket number -> " << count << std::endl;

    // Random kode ticket
    int randomKodeTicket = dis(gen) % 100;
    std::ostringstream randomKodeStream;
    randomKodeStream << std::setw(2) << std::setfill('0') << randomKodeTicket;
    std::string randomKodeTicketStr = randomKodeStream.str();

    // Generate ticket
    try {
        std::string ticketGenerate = "SV" + std::to_string(99).substr(0, 1) + codeTahun + codeBulan + codeTanggal + codeJam + codeMenit + count;
        std::cout << "[INFO] generate ticket -> " << ticketGenerate << std::endl;

        // Generate QR code
        std::string ticketGenerateQr = "SV" + std::to_string(99).substr(0, 1) + codeTahun + codeBulan + codeTanggal + codeJam + codeMenit + count + waktuSekarangAlt + "MOBIL";
        std::cout << "[INFO] generate ticket QR -> " << ticketGenerateQr << std::endl;

        return ticketGenerate;
    } catch (const std::exception &e) {
        std::cerr << "[ERROR] generate ticket -> " << e.what() << std::endl;
        return "";
    }
}

void setting(int select)
{
	sqlite3* DB;
	sqlite3_stmt *res;
	const char *tail;
	std::string query;
	char lastData[255];
    char* zErrMsg;
	int rc;
    int exit = sqlite3_open(DB_LOCAL, &DB);

	if(1 == select)
	{
		rc = sqlite3_prepare_v2(DB, "SELECT server FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data server sekarang: %s\n", lastData);
			printf("Masukan alamat server (192.168...) | [B] kembali: ");
			scanf("%s", ip_server);
			while ((getchar()) != '\n');

			printf("%s\n", ip_server);
			if (strcmp(ip_server, "B") == 0 || strcmp(ip_server, "b") == 0) {
				break;
			} else {
				if(is_valid_ip(ip_server, 0) != 1)
				{
					printf("[ERROR] IP address yang di input tidak valid!\n");
				} else {
					config.server = std::string(ip_server);
					// printf("%s", config.server.c_str());
					query = "UPDATE config SET server=";
					query += "\"";
					query += std::string(ip_server);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting Server => %s\n", ip_server);
					}
					query = "";
					sleep(3);
					break;
				}
			}
		} while (1);

	} else if(2 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ip_manless FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data IP Manless sekarang: %s\n", lastData);
			printf("Masukan IP (192.168...) | [B] kembali: ");
			scanf("%s", ip_manless);
			while ((getchar()) != '\n');

			if (strcmp(ip_manless, "B") == 0 || strcmp(ip_manless, "b") == 0) {
				break;
			} else {
				if(is_valid_ip(ip_manless, 0) != 1) {
					printf("[ERROR] IP address yang di input tidak valid!\n");
				} else {
					query = "UPDATE config SET ip_manless=";
					query += "\"";
					query += std::string(ip_manless);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting IP Manless => %s\n", ip_manless);
					}
					query = "";
					sleep(3);
					break;
				}
			}

		} while (1);

	} else if(3 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT id_manless FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data ID Manless sekarang: %s\n", lastData);
			printf("Masukan ID (1, 2 ...) | [B] kembali: ");
			scanf("%s", id_manless);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(id_manless, "B") == 0 || strcmp(id_manless, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET id_manless=";
				query += "\"";
				query += std::string(id_manless);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting ID Manless => %s\n", id_manless);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(4 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT id_location FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data ID Lokasi sekarang: %s\n", lastData);
			printf("Masukan ID Lokasi (AA, KA, BA ...) | [B] kembali: ");
			scanf("%s", id_location);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(id_location, "B") == 0 || strcmp(id_location, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET id_location=";
				query += "\"";
				query += std::string(id_location);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting ID Location => %s\n", id_location);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(5 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT path_image FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Path Image sekarang: %s\n", lastData);
			printf("Masukan Path Image (/home/it/image/) | [B] kembali: ");
			scanf("%s", path_image);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(path_image, "B") == 0 || strcmp(path_image, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET path_image=";
				query += "\"";
				query += std::string(path_image);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Path Image => %s\n", path_image);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(6 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT gate_name FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Gate Name sekarang: %s\n", lastData);
			printf("Masukan Gate Name (PM90, PM100 ...) | [B] kembali: ");
			scanf("%s", gate_name);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(gate_name, "B") == 0 || strcmp(gate_name, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET gate_name=";
				query += "\"";
				query += std::string(gate_name);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Gate Name => %s\n", gate_name);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(7 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT gate_kind FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Gate Kind sekarang: %s\n", lastData);
			printf("Masukan Gate Kind (MOBIL, MOTOR ...) | [B] kembali: ");
			scanf("%s", gate_kind);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(gate_kind, "B") == 0 || strcmp(gate_kind, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET gate_kind=";
				query += "\"";
				query += std::string(gate_kind);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Gate Kind => %s\n", gate_kind);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(8 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT printer_port FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Printer Port sekarang: %s\n", lastData);
			printf("Masukan Printer Port ttyS (0, 1, 2 ...) | [B] kembali: ");
			scanf("%s", printer_port);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(printer_port, "B") == 0 || strcmp(printer_port, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET printer_port=";
				query += "\"";
				query += std::string(printer_port);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Printer Port => %s\n", printer_port);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(9 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT printer_baudrate FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Printer Baudrate sekarang: %s\n", lastData);
			printf("Masukan Printer Baudrate (9600, 38400, 115200 ...) | [B] kembali: ");
			scanf("%s", printer_baudrate);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(printer_baudrate, "B") == 0 || strcmp(printer_baudrate, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET printer_baudrate=";
				query += "\"";
				query += std::string(printer_baudrate);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Printer Baudrate => %s\n", printer_baudrate);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(10 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT controller_port FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Controller Port sekarang: %s\n", lastData);
			printf("Masukan Controller Port ttyS(0, 1, 2 ...) | [B] kembali: ");
			scanf("%s", controller_port);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(controller_port, "B") == 0 || strcmp(controller_port, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET controller_port=";
				query += "\"";
				query += std::string(controller_port);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Controller Port => %s\n", controller_port);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(11 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT controller_baudrate FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Controller Baudrate sekarang: %s\n", lastData);
			printf("Masukan Controller Baudrate (9600, 38400, 115200 ...) | [B] kembali: ");
			scanf("%s", controller_baudrate);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(controller_baudrate, "B") == 0 || strcmp(controller_baudrate, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET controller_baudrate=";
				query += "\"";
				query += std::string(controller_baudrate);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Controller Baudrate => %s\n", controller_baudrate);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(12 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT delay_loop FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Delay Loop sekarang: %s\n", lastData);
			printf("Masukan Delay Loop (1000, 2000 ... ms) | [B] kembali: ");
			scanf("%s", delay_loop);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(delay_loop, "B") == 0 || strcmp(delay_loop, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET delay_loop=";
				query += "\"";
				query += std::string(delay_loop);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Delay Loop => %s\n", delay_loop);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(13 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT delay_led_manless FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Delay LED sekarang: %s\n", lastData);
			printf("Masukan Delay LED (1000, 2000 ... ms) | [B] kembali: ");
			scanf("%s", delay_led);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(delay_led, "B") == 0 || strcmp(delay_led, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET delay_led_manless=";
				query += "\"";
				query += std::string(delay_led);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Delay LED => %s\n", delay_led);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(14 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT paper_warning FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Paper Warning sekarang: %s\n", lastData);
			printf("Masukan Paper Warning (1000, 2000 ...) | [B] kembali: ");
			scanf("%s", paper_warning);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(paper_warning, "B") == 0 || strcmp(paper_warning, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET paper_warning=";
				query += "\"";
				query += std::string(paper_warning);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Paper Warning => %s\n", paper_warning);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(15 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT service_host FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data HOST Service sekarang: %s\n", lastData);
			printf("Masukan HOST Service (192.168.1 ...) | [B] kembali: ");
			scanf("%s", service_host);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(service_host, "B") == 0 || strcmp(service_host, "b") == 0) {
				break;
			} else {
				if(is_valid_ip(service_host, 0) != 1)
				{
					printf("[ERROR] HOST service yang di input tidak valid!\n");
				} else {
					query = "UPDATE config SET service_host=";
					query += "\"";
					query += std::string(service_host);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting HOST Service => %s\n", service_host);
					}
					query = "";
					sleep(3);
					break;
				}
			}

		} while (1);

	} else if(16 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT service_port FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data PORT Service sekarang: %s\n", lastData);
			printf("Masukan PORT service (6768, 8080, 7777 ...) | [B] kembali: ");
			scanf("%s", service_port);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(service_port, "B") == 0 || strcmp(service_port, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET service_port=";
				query += "\"";
				query += std::string(service_port);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting PORT Service => %s\n", service_port);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(17 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT reader_id FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data ID Reader sekarang: %s\n", lastData);
			printf("Masukan ID Reader (77, 76, 99 ...) | [B] kembali: ");
			scanf("%s", reader_id);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(reader_id, "B") == 0 || strcmp(reader_id, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET reader_id=";
				query += "\"";
				query += std::string(reader_id);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting ID Reader => %s\n", reader_id);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(18 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_1 FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data IP address kamera[1] sekarang: %s\n", lastData);
			printf("Masukan IP address kamera[1] (192.168.1 ...) | [B] kembali: ");
			scanf("%s", ip_camera_1);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_1, "B") == 0 || strcmp(ip_camera_1, "b") == 0) {
				break;
			} else {
				if(is_valid_ip(ip_camera_1, 0) != 1)
				{
					printf("[ERROR] IP address yang di input tidak valid!\n");
				} else {
					query = "UPDATE config SET ip_cam_1=";
					query += "\"";
					query += std::string(ip_camera_1);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting IP Camera [1] => %s\n", ip_camera_1);
					}
					query = "";
					sleep(3);
					break;
				}
			}

		} while (1);

	} else if(19 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_1_user FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data user kamera[1] sekarang: %s\n", lastData);
			printf("Masukan user kamera[1] | [B] kembali: ");
			scanf("%s", ip_camera_1_user);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_1_user, "B") == 0 || strcmp(ip_camera_1_user, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET ipcam_1_user=";
				query += "\"";
				query += std::string(ip_camera_1_user);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[1] User => %s\n", ip_camera_1_user);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(20 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_1_pass FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data password kamera[1] sekarang: %s\n", lastData);
			printf("Masukan password kamera[1] | [B] kembali: ");
			scanf("%s", ip_camera_1_password);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_1_password, "B") == 0 || strcmp(ip_camera_1_password, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET ipcam_1_pass=";
				query += "\"";
				query += std::string(ip_camera_1_password);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[1] Password => %s\n", ip_camera_1_password);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(21 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_1_model FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data model kamera[1] sekarang: %s\n", lastData);
			printf("Masukan model kamera[1] | [B] kembali: ");
			scanf("%s", ip_camera_1_model);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_1_model, "B") == 0 || strcmp(ip_camera_1_model, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET ipcam_1_model=";
				query += "\"";
				query += std::string(ip_camera_1_model);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[1] Model => %s\n", ip_camera_1_model);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(22 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_1_status FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data status kamera[1] sekarang: %s\n", lastData);
			printf("Masukan status kamera[1] (1=Aktif | 0=Tidak Aktif) | [B] kembali: ");
			scanf("%s", ip_camera_1_status);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_1_status, "B") == 0 || strcmp(ip_camera_1_status, "b") == 0) {
				break;
			} else {
				if(ip_camera_1_status == "1" || ip_camera_1_status == "0") {
					query = "UPDATE config SET ipcam_1_status=";
					query += "\"";
					query += std::string(ip_camera_1_status);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[1] Status => %s\n", ip_camera_1_status);
					}
					query = "";
					sleep(3);
					break;
				} else {
					printf("[ERROR] Input hanya boleh angka 1/0\n");
				}
			}

		} while (1);

	} else if(23 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_2 FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data IP address kamera[2] sekarang: %s\n", lastData);
			printf("Masukan IP address kamera[2] (192.168.1 ...) | [B] kembali: ");
			scanf("%s", ip_camera_2);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_2, "B") == 0 || strcmp(ip_camera_2, "b") == 0) {
				break;
			} else {
				if(is_valid_ip(ip_camera_2, 0) != 1)
				{
					printf("[ERROR] IP address yang di input tidak valid!\n");
				} else {
					query = "UPDATE config SET ipcam_2=";
					query += "\"";
					query += std::string(ip_camera_2);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting IP Camera [2] => %s\n", ip_camera_2);
					}
					query = "";
					sleep(3);
					break;
				}
			}

		} while (1);

	} else if(24 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_2_user FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data user kamera[2] sekarang: %s\n", lastData);
			printf("Masukan user kamera[2] | [B] kembali: ");
			scanf("%s", ip_camera_2_user);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_2_user, "B") == 0 || strcmp(ip_camera_2_user, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET ipcam_2_user=";
				query += "\"";
				query += std::string(ip_camera_2_user);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[2] User => %s\n", ip_camera_2_user);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(25 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_2_pass FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data password kamera[2] sekarang: %s\n", lastData);
			printf("Masukan password kamera[2] | [B] kembali: ");
			scanf("%s", ip_camera_2_password);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_2_password, "B") == 0 || strcmp(ip_camera_2_password, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET ipcam_2_pass=";
				query += "\"";
				query += std::string(ip_camera_2_password);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[2] Password => %s\n", ip_camera_2_password);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(26 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_2_model FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data model kamera[2] sekarang: %s\n", lastData);
			printf("Masukan model kamera[2] | [B] kembali: ");
			scanf("%s", ip_camera_2_model);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_2_model, "B") == 0 || strcmp(ip_camera_2_model, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET ipcam_2_model=";
				query += "\"";
				query += std::string(ip_camera_2_model);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[2] Model => %s\n", ip_camera_2_model);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(27 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT ipcam_2_status FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data status kamera[2] sekarang: %s\n", lastData);
			printf("Masukan status kamera[2] (1=Aktif | 0=Tidak Aktif) | [B] kembali: ");
			scanf("%s", ip_camera_2_status);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(ip_camera_2_status, "B") == 0 || strcmp(ip_camera_2_status, "b") == 0) {
				break;
			} else {
				if(ip_camera_2_status == "1" || ip_camera_2_status == "0") {
					query = "UPDATE config SET ipcam_2_status=";
					query += "\"";
					query += std::string(ip_camera_2_status);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting IP Cam[2] Status => %s\n", ip_camera_2_status);
					}
					query = "";
					sleep(3);
					break;
				} else {
					printf("[ERROR] Input hanya boleh angka 1/0\n");
				}
			}

		} while (1);

	} else if(28 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT alpr_url FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data lpr engine endpoint sekarang: %s\n", lastData);
			printf("Masukan lpr engine endpoint (https//lprengine.com...) | [B] kembali: ");
			scanf("%s", lpr_engine);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(lpr_engine, "B") == 0 || strcmp(lpr_engine, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET alpr_url=";
				query += "\"";
				query += std::string(lpr_engine);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting LPR Engine => %s\n", lpr_engine);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(29 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT alpr_status FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data lpr engine status sekarang: %s\n", lastData);
			printf("Masukan lpr engine status (1=Aktif | 0=Tidak Aktif) | [B] kembali: ");
			scanf("%s", lpr_engine_status);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(lpr_engine_status, "B") == 0 || strcmp(lpr_engine_status, "b") == 0) {
				break;
			} else {
				if(strcmp(lpr_engine_status, "1") == 0 || strcmp(lpr_engine_status, "0") == 0) {
					query = "UPDATE config SET alpr_status=";
					query += "\"";
					query += std::string(lpr_engine_status);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting LPR Engine Status => %s\n", lpr_engine_status);
					}
					query = "";
					sleep(3);
					break;
				} else {
					printf("[ERROR] Input hanya boleh angka 1/0\n");
				}
			}

		} while (1);

	} else if(30 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT face_url FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data face engine endpoint sekarang: %s\n", lastData);
			printf("Masukan face engine endpoint (https//faceengine.com...) | [B] kembali: ");
			scanf("%s", face_engine);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(face_engine, "B") == 0 || strcmp(face_engine, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET face_url=";
				query += "\"";
				query += std::string(face_engine);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting FACE Engine => %s\n", face_engine);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(31 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT face_status FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data face engine status sekarang: %s\n", lastData);
			printf("Masukan face engine status (1=Aktif | 0=Tidak Aktif) | [B] kembali: ");
			scanf("%d", face_engine_status);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(face_engine_status, "B") == 0 || strcmp(face_engine_status, "b") == 0) {
				break;
			} else {
				if(face_engine_status == "1" || face_engine_status == "0") {
					query = "UPDATE config SET face_status=";
					query += "\"";
					query += std::string(face_engine_status);
					query += "\"";
					query += " WHERE id=";
					query += id_config + ";";
					// std::cout << query << std::endl;
					rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
					if (rc != SQLITE_OK) {
						fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
						sqlite3_free(zErrMsg);
					} else {
						fprintf(stdout, "[INFO] Operation OK!, Setting FACE Engine Status => %s\n", face_engine_status);
					}
					query = "";
					sleep(3);
					break;
				} else {
					printf("[ERROR] Input hanya boleh angka 1/0\n");
				}
			}

		} while (1);

	} else if(32 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT nama_lokasi FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data nama lokasi sekarang: %s\n", lastData);
			printf("Masukan nama lokasi | [B] kembali: ");
			scanf("%[^\n]s", nama_lokasi);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(nama_lokasi, "B") == 0 || strcmp(nama_lokasi, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET nama_lokasi=";
				query += "\"";
				query += std::string(nama_lokasi);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Nama Lokasi => %s\n", nama_lokasi);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(33 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT alamat_lokasi FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data alamat lokasi sekarang: %s\n", lastData);
			printf("Masukan alamat lokasi | [B] kembali: ");
			scanf("%[^\n]s", alamat_lokasi);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(alamat_lokasi, "B") == 0 || strcmp(alamat_lokasi, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET alamat_lokasi=";
				query += "\"";
				query += std::string(alamat_lokasi);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Alamat Lokasi => %s\n", alamat_lokasi);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(34 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT pesan_masuk FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data pesan masuk sekarang: %s\n", lastData);
			printf("Masukan pesan masuk | [B] kembali: ");
			scanf("%[^\n]s", pesan_masuk);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(pesan_masuk, "B") == 0 || strcmp(pesan_masuk, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET pesan_masuk=";
				query += "\"";
				query += std::string(pesan_masuk);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Pesan Masuk => %s\n", pesan_masuk);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else if(35 == select) {
		int rcount = 0;
		fprintf(stdout, "[INFO] Operation OK!, Current Config:\n");
		rc = sqlite3_prepare_v2(DB, "SELECT * FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			printf("[INFO] Row[%d] => ID: %8s | Server: %8s | IP Manless: %8s | ID Manless: %8s | ID Lokasi: %8s | Path Image: %8s | Gate: %8s | Gate Tipe: %8s | Printer Port: %8s | Printer Baudrate: %8s | Controller Port: %8s | Controller Baudrate: %8s | Delay Loop: %8s | Delay LED: %8s | Paper Warning: %8s | Service Host: %8s | Service Port: %8s | ID Reader: %8s | IP Cam 1: %8s | IP Cam 1 User: %8s | IP Cam 1 Password: %8s | IP Cam 1 Model: %8s | IP Cam 1 Status: %8s | IP Cam 2: %8s | IP Cam 2 User: %8s | IP Cam 2 Password: %8s | IP Cam 2 Model: %8s | IP Cam 2 Status :%8s | LPR Url: %8s | LPR Status: %8s | FACE Url: %8s | FACE Status: %8s | Nama Lokasi: %8s | Alamat Lokasi: %8s | Pesan Masuk: %8s | Bank: %8s\n",
			rcount,
			sqlite3_column_text(res, 0),
			sqlite3_column_text(res, 1),
			sqlite3_column_text(res, 2),
			sqlite3_column_text(res, 3),
			sqlite3_column_text(res, 4),
			sqlite3_column_text(res, 5),
			sqlite3_column_text(res, 6),
			sqlite3_column_text(res, 7),
			sqlite3_column_text(res, 8),
			sqlite3_column_text(res, 9),
			sqlite3_column_text(res, 10),
			sqlite3_column_text(res, 11),
			sqlite3_column_text(res, 12),
			sqlite3_column_text(res, 13),
			sqlite3_column_text(res, 14),
			sqlite3_column_text(res, 15),
			sqlite3_column_text(res, 16),
			sqlite3_column_text(res, 17),
			sqlite3_column_text(res, 18),
			sqlite3_column_text(res, 19),
			sqlite3_column_text(res, 20),
			sqlite3_column_text(res, 21),
			sqlite3_column_text(res, 22),
			sqlite3_column_text(res, 23),
			sqlite3_column_text(res, 24),
			sqlite3_column_text(res, 25),
			sqlite3_column_text(res, 26),
			sqlite3_column_text(res, 27),
			sqlite3_column_text(res, 28),
			sqlite3_column_text(res, 29),
			sqlite3_column_text(res, 30),
			sqlite3_column_text(res, 31),
			sqlite3_column_text(res, 32),
			sqlite3_column_text(res, 33),
			sqlite3_column_text(res, 34),
			sqlite3_column_text(res, 35));
			rcount++;
		}
		sqlite3_finalize(res);
		sleep(3);

	} else if(36 == select) {
		std::string portPrinter;
		char port[3];
		char baudrate[8];
		char nopol[] = "B1234TY";
		char notiket[] = "KA22CMG17132";
		char qr[] = "KA22CMG1713220231228150210MOBIL";
		do
		{
			printf("Masukan port COM/tty (0, 1, 2 ...) | [B] kembali: ");
			scanf("%s", port);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(port, "B") == 0 || strcmp(port, "b") == 0) {
				break;
			} else {
				portPrinter = "/dev/ttyS";
				portPrinter += std::string(port);
				break;
			}

		} while (1);

		do
		{
			printf("Masukan baudrate | [B] kembali: ");
			scanf("%s", baudrate);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(baudrate, "B") == 0 || strcmp(baudrate, "b") == 0) {
				break;
			} else {
				std::string(baudrate);
				break;
			}

		} while (1);

		// printf("%s\n", portPrinter.c_str());
		// printf("%s", baudrate);

		// print code
		escpos_printer *printer = escpos_printer_serial(portPrinter.c_str(), atoi(baudrate));
		if (printer != NULL)
		{
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

			int storelen = sizeof(qr) + 3;
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
			escpos_printer_raw(printer, "SMART PARKING", sizeof("SMART PARKING"));
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "BANDUNG", sizeof("BANDUNG"));
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "==========================================", sizeof("=========================================="));
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "\x1b\x4d\x31", sizeof("\x1b\x4d\x31")); // font normal
			escpos_printer_raw(printer, "\x1b\x61\x00", sizeof("\x1b\x61\x00")); // left
			char baris1[57];
			sprintf(baris1, "Smart Vision Parking                    Blue Box v.%s", app_version);
			escpos_printer_raw(printer, baris1, sizeof(baris1));
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "\x1b\x45\x00", sizeof("\x1b\x45\x00")); // emphasized off
			escpos_printer_raw(printer, "\x1b\x4d\x30", sizeof("\x1b\x4d\x30")); // font header
			char baris2[9 + strlen("MOBIL")];
			sprintf(baris2, "PM %s* - %s", "2", "MOBIL");
			escpos_printer_raw(printer, baris2, sizeof(baris2));
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "\x1b\x61\x01", sizeof("\x1b\x61\x01")); // center
			escpos_printer_raw(printer, "\x1b\x21\x20", sizeof("\x1b\x21\x20")); // double width on
			escpos_printer_raw(printer, "\x1b\x45\x01", sizeof("\x1b\x45\x01")); // emphasized on
			escpos_printer_raw(printer, "99", sizeof("99"));
			escpos_printer_raw(printer, "\x1b\x21\x00", sizeof("\x1b\x21\x00")); // double width off
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, set_qrmodel, sizeof(set_qrmodel));					   // qr model
			escpos_printer_raw(printer, set_qrsize, sizeof(set_qrsize));					   // qr size
			escpos_printer_raw(printer, set_qrcorrectionlevel, sizeof(set_qrcorrectionlevel)); // qr correction level
			escpos_printer_raw(printer, set_qrstoredata, sizeof(set_qrstoredata));			   // qr calculate to store data
			escpos_printer_raw(printer, qr, sizeof(qr));									   // qr data
			escpos_printer_raw(printer, print_qr, sizeof(print_qr));						   // qr print
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			escpos_printer_raw(printer, "\x1b\x21\x20", sizeof("\x1b\x21\x20")); // double width on
			char baris3[17];
			sprintf(baris3, "%c%c%c %c%c%c%c %c%c%c%c %c%c", notiket[0], notiket[1], notiket[2], notiket[3], notiket[3], notiket[5], notiket[6], notiket[7], notiket[8], notiket[9], notiket[10], notiket[11], notiket[12]);
			escpos_printer_raw(printer, baris3, sizeof(baris3));
			escpos_printer_raw(printer, "\x1b\x21\x00", sizeof("\x1b\x21\x00")); // double width off
			escpos_printer_raw(printer, "\n", sizeof("\n"));
			if (strcmp(nopol, "-") != 0)
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
			escpos_printer_raw(printer, "Jangan tinggalkan tiket & barang berharga di kendaraan\nTiket hilang dikenakan denda", sizeof("Jangan tinggalkan tiket & barang berharga di kendaraan\nTiket hilang dikenakan denda"));

			escpos_printer_feed(printer, 3);
			escpos_printer_cut(printer, 1);
			escpos_printer_destroy(printer);

			memset(set_qrmodel, 0, sizeof(set_qrmodel));
			memset(set_qrsize, 0, sizeof(set_qrsize));
			memset(set_qrcorrectionlevel, 0, sizeof(set_qrcorrectionlevel));
			memset(print_qr, 0, sizeof(print_qr));
			sleep(3);
		}
		else
		{
			escpos_error err = last_error;
			printf("[ERROR] Print error code : %d\n", err);
		}

	} else if(37 == select) {
		char portIo[12];
		char port[2];
		char baudrate[8];
		do
		{
			printf("Masukan port COM/tty (0, 1, 2 ...) | [B] kembali: ");
			scanf("%s", port);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(port, "B") == 0 || strcmp(port, "b") == 0) {
				break;
			} else {
				sprintf(portIo, "/dev/ttyS%s", port);
				break;
			}

		} while (1);

		// do
		// {
		// 	printf("Masukan baudrate | [B] kembali: ");
		// 	scanf("%s", baudrate);
		// 	// std::cout << std::to_string(id_manless) << std::endl;
		// 	while ((getchar()) != '\n');

		// 	if (strcmp(baudrate, "B") == 0 || strcmp(baudrate, "b") == 0) {
		// 		break;
		// 	} else {
		// 		std::string(baudrate);
		// 		break;
		// 	}

		// } while (1);

		int fd;
		int wlen;
		char *xstr = "OT1T\n";
		int xlen = strlen(xstr);
		fd = open(portIo, O_RDWR | O_NOCTTY | O_SYNC);
		if (fd < 0) printf("[ERROR] Error opening %s: %s\n", portIo, strerror(errno));
		set_interface_attribs(fd, B9600);
		// set_mincount(fd, 0);
		write(fd, "LD4T", strlen("LD4T"));
		msleep(100);
		wlen = write(fd, xstr, xlen);
		if (wlen != xlen) printf("[ERROR] Error from write: %d, %d\n", wlen, errno);
		tcdrain(fd);
		// tcflush(fd, TCIOFLUSH);
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
				printf("[INFO] %s\n", buffer);
				printf("[INFO] Relay -> True\n");
				// if (buffer[0] == 'R' && buffer[1] == 'E')
				// {
				// 	if (buffer[0] == '1' && buffer[1] == 'T')
				// 	{
				// 		printf("[INFO] Relay -> True\n");
				// 	}
				// }
				sleep(1);
				write(fd, "OT1F", strlen("OT1F"));
				msleep(100);
				write(fd, "LD4F", strlen("LD4F"));
				break;
			}
		}
		close(fd);
		sleep(3);

	} else if(38==select) {
		int valread;
		char host[15];
		char port[15];
		char readerid[2];
		char buffer[1024] = { 0 };
		rc = sqlite3_prepare_v2(DB, "SELECT service_host, service_port, reader_id FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(host, "%s", sqlite3_column_text(res, 0));
			sprintf(port, "%s", sqlite3_column_text(res, 1));
			sprintf(reader_id, "%s", sqlite3_column_text(res, 2));
		}

		printf("HOST: %s\n", host);
		printf("PORT: %s\n", port);
		printf("Connecting...\n");

		// creating socket
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

		// set timeout
		struct timeval tv;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
		setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

		// specifying address
		sockaddr_in serverAddress;
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(atoi(port));
		serverAddress.sin_addr.s_addr = inet_addr(host);

		// sending connection request
		connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

		// sending data
		// const char* message = "Hello, server!";
		json message = {
			{"taptype", "in"},
			{"id", atoi(reader_id)},
		};
		std::string j3 = message.dump();
		send(clientSocket, j3.c_str(), strlen(j3.c_str()), 0);
		valread = read(clientSocket, buffer, 1024 - 1);
		if (valread < 0) {
			printf("[ERROR] Socket Connection Time out\n");
		} else {
			printf("[INFO] Received from service => %s\n", buffer);
			sleep(1);
			tapstop();
		}

		// closing socket
		close(clientSocket);

	} else if(39==select) {
		int valread;
		char host[15];
		char port[15];
		char readerid[2];
		char buffer[1024] = { 0 };
		rc = sqlite3_prepare_v2(DB, "SELECT service_host, service_port, reader_id FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(host, "%s", sqlite3_column_text(res, 0));
			sprintf(port, "%s", sqlite3_column_text(res, 1));
			sprintf(reader_id, "%s", sqlite3_column_text(res, 2));
		}

		printf("HOST: %s\n", host);
		printf("PORT: %s\n", port);
		printf("Connecting...\n");

		// creating socket
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

		// set timeout
		struct timeval tv;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
		setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

		// specifying address
		sockaddr_in serverAddress;
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(atoi(port));
		serverAddress.sin_addr.s_addr = inet_addr(host);

		// sending connection request
		connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

		// sending data
		// const char* message = "Hello, server!";
		auto t = std::time(nullptr);
		auto tm = *std::localtime(&t);
		std::ostringstream oss;
		oss << std::put_time(&tm, "%Y%m%d%H%M%S");
		auto trx = oss.str();
		char getTanggal[3];
		strftime(getTanggal, sizeof(getTanggal), "%d", &tm);
		char getBulan[3];
		strftime(getBulan, sizeof(getBulan), "%m", &tm);
		char getTahun[5];
		strftime(getTahun, sizeof(getTahun), "%Y", &tm);
		int julianDay = get_julian_day(atoi(getTanggal), atoi(getBulan), atoi(getTahun));
		std::string nopol = generate_nopol(13);
		std::string notiket = generate_ticket();
		json message = {
			{"taptype", "out"},
			{"id", atoi(reader_id)},
			{"julianday", std::to_string(julianDay)},
			{"trxtime", trx},
			{"ticket", notiket},
			{"nopol", nopol},
			{"amount", "1"},
		};
		std::string j3 = message.dump();
		send(clientSocket, j3.c_str(), strlen(j3.c_str()), 0);
		valread = read(clientSocket, buffer, 1024 - 1);
		if (valread < 0) {
			printf("[ERROR] Socket Connection Time out\n");
		} else {
			printf("[INFO] Received from service => %s\n", buffer);
			sleep(1);
			tapstop();
		}

		// closing socket
		close(clientSocket);

	} else if(40==select) {
		int valread;
		char host[15];
		char port[15];
		char readerid[2];
		char buffer[1024] = { 0 };
		rc = sqlite3_prepare_v2(DB, "SELECT service_host, service_port, reader_id FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(host, "%s", sqlite3_column_text(res, 0));
			sprintf(port, "%s", sqlite3_column_text(res, 1));
			sprintf(reader_id, "%s", sqlite3_column_text(res, 2));
		}

		printf("HOST: %s\n", host);
		printf("PORT: %s\n", port);
		printf("Connecting...\n");

		// creating socket
		int clientSocket = socket(AF_INET, SOCK_STREAM, 0);

		// set timeout
		struct timeval tv;
		tv.tv_sec = 10;
		tv.tv_usec = 0;
		setsockopt(clientSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
		setsockopt(clientSocket, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);

		// specifying address
		sockaddr_in serverAddress;
		serverAddress.sin_family = AF_INET;
		serverAddress.sin_port = htons(atoi(port));
		serverAddress.sin_addr.s_addr = inet_addr(host);

		// sending connection request
		connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress));

		// sending data
		// const char* message = "Hello, server!";
		json message = {
			{"taptype", "sam"},
			{"id", atoi(reader_id)},
		};
		std::string j3 = message.dump();
		send(clientSocket, j3.c_str(), strlen(j3.c_str()), 0);
		valread = read(clientSocket, buffer, 1024 - 1);
		if (valread < 0) {
			printf("[ERROR] Socket Connection Time out\n");
		} else {
			printf("[INFO] Received from service => %s\n", buffer);
		}

		// closing socket
		close(clientSocket);
	} else if(41 == select) {
		rc = sqlite3_prepare_v2(DB, "SELECT bank FROM config WHERE status=1", 128, &res, &tail);
		if (rc != SQLITE_OK) {
			fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
			sqlite3_free(zErrMsg);
		}
		while (sqlite3_step(res) == SQLITE_ROW)
		{
			sprintf(lastData, "%s", sqlite3_column_text(res, 0));
		}

		do
		{
			printf("Data Bank sekarang: %s\n", lastData);
			printf("Masukan data Bank (BNI,BRI,DKI,MANDIRI ... *pisahkan dengan tanda ',' masukan tanda '-' jika ingin di kosongkan) | [B] kembali: ");
			scanf("%[^\n]s", bank);
			// std::cout << std::to_string(id_manless) << std::endl;
			while ((getchar()) != '\n');

			if (strcmp(bank, "B") == 0 || strcmp(bank, "b") == 0) {
				break;
			} else {
				query = "UPDATE config SET bank=";
				query += "\"";
				query += std::string(bank);
				query += "\"";
				query += " WHERE id=";
				query += id_config + ";";
				// std::cout << query << std::endl;
				rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_2, NULL, NULL);
				if (rc != SQLITE_OK) {
					fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
					sqlite3_free(zErrMsg);
				} else {
					fprintf(stdout, "[INFO] Operation OK!, Setting Bank => %s\n", bank);
				}
				query = "";
				sleep(3);
				break;
			}

		} while (1);

	} else {
		printf("[INFO] Menu yang kamu pilih belum tersedia!\n");
		sleep(3);
	}
}

int main(int argc, char *argv[])
{
    // if (argc < 3)
	// {
	// 	printf("Please input correct parameters!\n");
	// 	return -1;
	// }

	int select = 0;
	// sqlite3* DB;
    // char* zErrMsg;
    // int exit = sqlite3_open(DB_LOCAL, &DB);
    // std::string query = "SELECT * FROM config WHERE status=1;";
	// int rc = sqlite3_exec(DB, query.c_str(), sqlite_callback_1, NULL, NULL);
	// if (rc != SQLITE_OK) {
	// 	fprintf(stderr, "[ERROR] SQL error: %s\n", zErrMsg);
	// 	goto exit;
	// }

	// sqlite3 db create table
	// sqlite3* DB;
    // std::string sql = "CREATE TABLE config("
    //                   "id INT PRIMARY KEY     NOT NULL, "
    //                   "ip_manless        CHAR(50), "
    //                   "id_manless        INT, "
    //                   "id_location       CHAR(10), "
    //                   "path_image        CHAR(50), "
    //                   "gate_name         CHAR(50), "
	// 				  "gate_kind		 CHAR(50), "
	// 				  "printer_port		 INT, "
	// 				  "printer_baudrate	 INT, "
	// 				  "controller_port	 INT, "
	// 				  "controller_baudrate	 INT, "
	// 				  "delay_loop 		 INT, "
	// 				  "delay_led_manless INT, "
	// 				  "paper_warning	 INT, "
	// 				  "ipcam_1			 CHAR(50),"
	// 				  "ipcam_1_user		 CHAR(50),"
	// 				  "ipcam_1_pass		 CHAR(50),"
	// 				  "ipcam_1_model	 CHAR(50),"
	// 				  "ipcam_1_status	 INT, "
	// 				  "ipcam_2			 CHAR(50),"
	// 				  "ipcam_2_user		 CHAR(50),"
	// 				  "ipcam_2_pass		 CHAR(50),"
	// 				  "ipcam_2_model	 CHAR(50),"
	// 				  "ipcam_2_status	 INT, "
	// 				  "alpr_url		 	 TEXT, "
	// 				  "alpr_status		 INT, "
	// 				  "face_url		 	 TEXT, "
	// 				  "face_status		 INT, "
	// 				  "nama_lokasi		 TEXT, "
	// 				  "alamat_lokasi     INT, "
	// 				  "pesan_masuk       TEXT "
	// 				  ");";
    // int exit = 0;
    // exit = sqlite3_open("local.db", &DB);
    // char* messaggeError;
    // exit = sqlite3_exec(DB, sql.c_str(), NULL, 0, &messaggeError);

    // if (exit != SQLITE_OK) {
    //     std::cerr << "Error Create Table" << std::endl;
    //     sqlite3_free(messaggeError);
    // }
    // else
    //     std::cout << "Table created Successfully" << std::endl;
    // sqlite3_close(DB);

    do
	{
		printf("====================================================\n");
		printf("|                Welcome to Config PM              |\n");
		printf("|                Version %s @ %s        |\n", app_version, app_builddate);
		printf("====================================================\n");
		printf("1.Server\n");
		printf("2.IP Manless\n");
		printf("3.ID Manless\n");
		printf("4.ID Location\n");
		printf("5.Path Image\n");
		printf("6.Gate Name\n");
		printf("7.Gate Kind\n");
		printf("8.Printer Port\n");
		printf("9.Printer Baudrate\n");
		printf("10.Controller Port\n");
		printf("11.Contoller Baudrate\n");
		printf("12.Delay Loop\n");
		printf("13.Delay LED\n");
		printf("14.Paper Warning\n");
		printf("15.Service Host\n");
		printf("16.Service Port\n");
		printf("17.ID Reader\n");
		printf("18.IP Camera 1\n");
		printf("19.IP Camera 1 User\n");
		printf("20.IP Camera 1 Password\n");
		printf("21.IP Camera 1 Model\n");
		printf("22.IP Camera 1 Status\n");
		printf("23.IP Camera 2\n");
		printf("24.IP Camera 2 User\n");
		printf("25.IP Camera 2 Password\n");
		printf("26.IP Camera 2 Model\n");
		printf("27.IP Camera 2 Status\n");
		printf("28.3rd LPR Engine\n");
		printf("29.3rd LPR Engine Status\n");
		printf("30.3rd FACE Engine\n");
		printf("31.3rd FACE Engine Status\n");
		printf("32.Nama Lokasi\n");
		printf("33.Alamat Lokasi\n");
		printf("34.Pesan Masuk\n");
		printf("35.Current Config\n");
		printf("36.Test Print\n");
		printf("37.Test Relay\n");
		printf("38.Test Tapin (Required Service)\n");
		printf("39.Test Deduct (Required Service)\n");
		printf("40.Check SAM (Required Service)\n");
		printf("41.Bank\n");
		printf("100.Exit\n");
		printf("Pilih menu (Angka): \n");
		scanf("%d", &select);
		while ((getchar()) != '\n');
		// fflush(stdin);
		if (100 == select)
		{
			g_run = 0;
			break;
		}

		setting(select);
	} while (g_run);

exit:

    return 0;
}