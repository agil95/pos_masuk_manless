#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

int ser;
int device_io(char *port, int baudrate, int timeout) {
    ser = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
    if (ser == -1) {
        perror("Error opening serial port");
        return -1;
    }

    struct termios options;
    tcgetattr(ser, &options);
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
    tcsetattr(ser, TCSANOW, &options);

    return ser;
}

char randomTicketCode[3];
int random_number(int min_num, int max_num)
{
    int result = 0, low_num = 0, hi_num = 0;

    if (min_num < max_num)
    {
        low_num = min_num;
        hi_num = max_num + 1; // include max_num in output
    } else {
        low_num = max_num + 1; // include max_num in output
        hi_num = min_num;
    }

    srand(time(NULL));
    result = (rand() % (hi_num - low_num)) + low_num;
    return result;
}

int fileCheck(const char *fileName)
{
    if(!access(fileName, F_OK )){
        printf("The File -> %s\t was Found\n",fileName);
        return 1;
    }else{
        printf("The File -> %s\t not Found\n",fileName);
        return 0;
    }
}

int main() {
    // int ser = device_io("/dev/ttyS0", 9600, 10);
    // if (ser == -1) {
    //     return 1;
    // }

    // char ESC = '\x1b';
    // char GS = '\x1d';
    // char LF = '\n';
    // char HT = '\x09';
    // char ticketQr[33] = "KA223CJB4309020231219114314MOBIL";
    // int storelen = strlen(ticketQr) + 3;
    // char store_pl = storelen % 256;
    // char store_ph = storelen / 256;
    // char set_qrmodel[] = {GS, '\x28', '\x6B', '\x04', '\x00', '\x31', '\x41', '\x32', '\x00'};
    // char set_qrsize[] = {GS, '\x28', '\x6B', '\x03', '\x00', '\x31', '\x43', '\x06'};
    // char set_qrcorrectionlevel[] = {GS, '\x28', '\x6B', '\x03', '\x00', '\x31', '\x45', '\x30'};
    // char set_qrstoredata[] = {GS, '\x28', '\x6B', store_pl, store_ph, '\x31', '\x50', '\x30'};
    // char print_qr[] = {GS, '\x28', '\x6B', '\x03', '\x00', '\x31', '\x51', '\x30'};

    // printf("%s", set_qrstoredata);

    // write(ser, set_qrmodel, sizeof(set_qrmodel));
    // write(ser, set_qrsize, sizeof(set_qrsize));
    // write(ser, set_qrcorrectionlevel, sizeof(set_qrcorrectionlevel));
    // write(ser, set_qrstoredata, sizeof(set_qrstoredata));
    // write(ser, ticketQr, sizeof(ticketQr));
    // write(ser, print_qr, sizeof(print_qr));

    // if (write(ser, buffer, sizeof(buffer)) == -1) {
    //     perror("Error writing to serial port");
    //     close(ser);
    //     return 1;
    // }

    // if (tcdrain(ser) == -1) {
    //     perror("Error waiting for output to be transmitted");
    //     close(ser);
    //     return 1;
    // }

    // close(ser);

    // int pos, len;
 
    // // Testcase1
    // char string[16] = "KA223CJB43090";
    // char substringa[4];
    // char substringb[4];
    // char substringc[4];
    // char substringd[4];
    // char substringe[3];
    // char my[4];
 
    // // Initialize pos, len i.e., starting
    // // index and len upto which we have to
    // // get substring respectively.
    // printf("String: %s ", string);
    // printf("\nsubstring is: ");
 
    // // Using strncpy function to 
    // // copy the substring
    // strncpy(substringa,string+(1-1),3);
    // strncpy(substringb,string+(3-1),3);
    // strncpy(substringc,string+(6-1),3);
    // strncpy(substringd,string+(9-1),3);
    // strncpy(substringe,string+(12-1),2);
    
    // // char my[20];

    // // printf("%c", string[0]);
    // sprintf(my, "%c%c%c", string[0], string[1], string[2]);
    // printf("%s\n", my);
    
    // // printf("Min : 100 Max : 1000 %02d\n",random_number(1,99));

    // char wm[] = "2023-12-22 15:54:23.000";
    // char wmsubstring[11];
    // strncpy(wmsubstring, wm+(1-1), 10);
    // printf("%s\n", wmsubstring);

    // int isFile = fileCheck("tmp/2023-12-21/KA223CLA13122-B.jpg");
    // printf("%d\n", isFile);   

    // char wm2[] = "tmp/2023-12-21/KA223CLA13122-B.jpg";
    // char source[100];
    // strncpy(source, wm2+(16-1), 13);
    // printf("%s\n", source);

    char bank_active[] = "MANDIRI,BRI,BNI";
    char delimiter[] = ",";
    char* token;
    char bank[] = "BRI";
    token = strtok(bank_active, delimiter);
    while (token != NULL) {
        printf("%s\n", token);
        if (strstr(token, bank) == NULL){
            printf("NOT FOUND\n");
        } else {
            printf("FOUND\n");
        }
        token = strtok(NULL, delimiter);
    }
    return 0;
}