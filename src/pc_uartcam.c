/*
 ============================================================================
 Name        : ucam_pc.c
 Author      : hj park
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include<time.h>


#define SEND_PIC_AT_ONCE 0
#define MODE_HD 0
#define PRINT_VALUES 0


#define PIC_PACKET_SIZE 2048
#define CHECK_SUM_BYTES 4
#define MAX_CMD_LENGTH 80


static  char cmdbuf[MAX_CMD_LENGTH];
enum UCMD {
		JPEG_MODE,
		BASE64_MODE,
		FULL_HD_MODE,
		HD_MODE,
		SEND_PIC_SIZE,
		TRANSFER_PIC_AT_ONCE,
		TRANSFER_PIC_PACKET,
		RETRANSFER_PIC_PACKET,
		RETURN_ACK,
		RETURN_NACK

};

static char *cmd_strings[] = {
		"IJ#",
		"IB#",
		"IF#",
		"IH#",
		"IS#",
		"IT#",
		"IP#",
		"IR#",
		"IA#",
		"IN#"
};

static int jpeg_or_base64_mode = JPEG_MODE;
//static int jpeg_or_base64_mode = BASE64_MODE;


int set_interface_attribs(int fd, int speed)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error from tcgetattr: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed(&tty, (speed_t)speed);
    cfsetispeed(&tty, (speed_t)speed);

    tty.c_cflag |= (CLOCAL | CREAD);    /* ignore modem controls */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;         /* 8-bit characters */
    tty.c_cflag &= ~PARENB;     /* no parity bit */
    tty.c_cflag &= ~CSTOPB;     /* only need 1 stop bit */
    tty.c_cflag &= ~CRTSCTS;    /* no hardware flowcontrol */

    /* setup for non-canonical mode */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tty.c_oflag &= ~OPOST;

    /* fetch bytes as they become available */
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 1;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error from tcsetattr: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

void set_mincount(int fd, int mcount)
{
    struct termios tty;

    if (tcgetattr(fd, &tty) < 0) {
        printf("Error tcgetattr: %s\n", strerror(errno));
        return;
    }

    tty.c_cc[VMIN] = mcount ? 1 : 0;
    tty.c_cc[VTIME] = 5;        /* half second timer */

    if (tcsetattr(fd, TCSANOW, &tty) < 0)
        printf("Error tcsetattr: %s\n", strerror(errno));
}



char *read_command(int fd)
{
	char rdbuf[MAX_CMD_LENGTH];

	int rdlen, count=0;
	char   *p;

	while(1)
	{
		rdlen = read(fd, rdbuf, sizeof(rdbuf)-1);
		if (rdlen > 0)
		 {
			for (p = rdbuf; rdlen-- > 0; p++)
			{
				printf("%c\n", *p);
				//if( *p >= 33/*!*/ &&  *p <=96 /* '*/)   /* filter special char */
				{
					cmdbuf[count] = *p;
					count ++;
				}
			}
		 }
		if(cmdbuf[count-1] == '#')
		{
			cmdbuf[count-1] = 0;
			break;
		}
		if (count >= MAX_CMD_LENGTH - 1)
		{
			cmdbuf[MAX_CMD_LENGTH - 1] = 0x0;
			break;
		}
	}

	return cmdbuf;
}



int write_command(int fd, char *cmd, int size)
{
	int wlen;

	wlen = write(fd, cmd, size);
   tcdrain(fd);    /* delay for output */
   printf("write_command= %s\n", cmd);

   return wlen;
}


int read_image_at_once(int fd, unsigned char *buffer, int size)
{
	unsigned int checksum = 0;
	unsigned int got_checksum = 0;
	unsigned int count=0;
	unsigned char buf[80];
	int rdlen;
	int jpeg_size=0;
	int i;


	do {
		rdlen = read(fd, buf, sizeof(buf) - 1);
		if (rdlen > 0)
		{
			unsigned char *p; //important : unsigned char
	   //     printf("Read %d:", rdlen);
			for (p = buf; rdlen-- > 0; p++)
			{
#if PRINT_VALUES
				printf("%02x ", *p);
#endif
				buffer[count] = *p;
				if(count < size-CHECK_SUM_BYTES)
				   checksum += *p;

				/*JPEG*/
				if((buffer[count] == 0xd9) && (buffer[count-1] == 0xff))
				{
					jpeg_size = count+1;
				}

				count ++;
#if PRINT_VALUES
				if(count%28 == 1)
					printf("\n");
#endif
				if(size == count)
				{
					printf( "checksum = 0x%x \n", checksum );
					for( i = 0; i < CHECK_SUM_BYTES ; i++)
					{
						//printf( "checksum = 0x%x \n", buffer[count - 1 - i] );
						got_checksum |= buffer[count - 1 - i] << (i*8);
					}
					//printf( "got_checksum = 0x%x \n", got_checksum  );


					if(got_checksum == checksum)
						printf( "checksum is right = 0x%x = 0x%x  \n", got_checksum, checksum  );
					else
					{
						printf( "checksum error = 0x%x <> 0x%x  \n", got_checksum, checksum  );
						return 1;
					}



					return 0;//break;
				}
			}
   //     printf("\n");

		}
		else if (rdlen < 0)
		{
			printf("Error from read: %d: %s\n", rdlen, strerror(errno));
		}
		/* repeat read to get full message */
	} while (1);

	return 0;
}


int read_image_packet(int fd, unsigned char *buffer, int size)
{
	unsigned int checksum = 0;
	unsigned int got_checksum = 0;
	unsigned int count=0;
	unsigned char buf[80];
	int rdlen;
	int i;

	do {
		rdlen = read(fd, buf, sizeof(buf) - 1);
		if (rdlen > 0)
		{
			unsigned char *p; //important : unsigned char
	   //     printf("Read %d:", rdlen);
			for (p = buf; rdlen-- > 0; p++)
			{
#if PRINT_VALUES
				printf("%02x ", *p);
#endif
				buffer[count] = *p;

				if(count < (size - CHECK_SUM_BYTES))
				   checksum += *p;

				count ++;
#if PRINT_VALUES
				if(count%28 == 1)
					printf("\n");
#endif
				if(size == count)
				{
					printf( "checksum = 0x%x \n", checksum );
					for(i = 0; i < CHECK_SUM_BYTES ; i++)
					{
						got_checksum |= buffer[count - 1 - i] << (i*8);
					}

					if(got_checksum == checksum)
						printf( "checksum is right = 0x%x = 0x%x  \n", got_checksum, checksum  );
					else
					{
						printf( "checksum error = 0x%x <> 0x%x  \n", got_checksum, checksum  );
						return 1;
					}
					return 0;//break;
				}
			}
   //     printf("\n");
		}
		else if (rdlen < 0)
		{
			printf("Error from read: %d: %s\n", rdlen, strerror(errno));
		}
		/* repeat read to get full message */
	} while (1);

	return 0;
}

int main()
{
    char *portname = "/dev/ttyUSB0";
    int fd, error=0, i;
    unsigned char *buffer; //important : unsigned char
    char *rdstr, *pch;
    unsigned int pic_size = 0, nofpackets=0, restbytes=0;
    FILE *write_ptr;
    unsigned char tmpbuf[80];

   // printf("%d, %d \n", strlen(cmd_strings[JPEG_MODE]), sizeof(cmd_strings[JPEG_MODE]));
  // return 0;

   	clock_t begin=clock();

    fd = open(portname, O_RDWR | O_NOCTTY | O_SYNC); // UBUNTU PC
    //fd = open(portname, O_RDWR ); //vbox ubuntu
    if (fd < 0) {
        printf("Error opening %s: %s\n", portname, strerror(errno));
        return -1;
    }
    /*baudrate 115200, 8 bits, no parity, 1 stop bit */
   set_interface_attribs(fd, B115200);
    //set_mincount(fd, 0);                /* set to pure timed read */

/*Send resolution mode change */
#if MODE_HD
   write_command(fd, cmd_strings[HD_MODE], strlen(cmd_strings[HD_MODE]) );
#else
   write_command(fd, cmd_strings[FULL_HD_MODE], strlen(cmd_strings[FULL_HD_MODE]) );
#endif

   rdstr = read_command(fd);

   if(strcmp(rdstr, cmd_strings[RETURN_ACK]))
    {
    	printf("ACK Return cmd = %s\n ", rdstr);
   }else
    {
    	printf("NAK Return cmd = %s\n ", rdstr);
    }


/*Send data mode change */
   if (jpeg_or_base64_mode == JPEG_MODE)
    {
    	write_ptr = fopen("test.jpeg","wb");  // w for write, b for binary
      	write_command(fd, cmd_strings[JPEG_MODE], strlen(cmd_strings[JPEG_MODE]) );
    }
   else
    {
    	write_ptr = fopen("test.html","wt");  // w for write, t for text
    	write_command(fd, cmd_strings[BASE64_MODE], strlen(cmd_strings[BASE64_MODE]) );
    }

   /*Get ACK */
    rdstr = read_command(fd);

    if(strcmp(rdstr, cmd_strings[RETURN_ACK]))
    {
    	printf("ACK Return cmd = %s\n ", rdstr);
    }else
    {
    	printf("NAK Return cmd = %s\n ", rdstr);
    }

    while(1)
     {
		/*Send get picture size */
		write_command(fd, cmd_strings[SEND_PIC_SIZE], strlen(cmd_strings[SEND_PIC_SIZE]) );

		/*Read picture size */
		rdstr = read_command(fd);
		printf("cmd = %s\n ", rdstr);
		if(rdstr[1] == 'N')
		 {
			printf("Return NAK = %s, retry\n ", rdstr);
		 }else
			 break;
	 }

    if(rdstr[1] == 'S')
     {
		pch = strtok(rdstr, "IS");
		pic_size = atoi(pch);
		printf("pch = %s, pic_size = %d \n", pch, pic_size);

		/*Malloc picture buffer */
		buffer = malloc(pic_size * sizeof(unsigned char) );
		if(buffer == NULL)
		{
			 printf("Error! memory not allocated.");
			 exit(0);
		}
#if SEND_PIC_AT_ONCE
		/*Send picture at once command */
		write_command(fd, cmd_strings[TRANSFER_PIC_AT_ONCE], strlen(cmd_strings[TRANSFER_PIC_AT_ONCE]) );

		/*Read picture */
		error = read_image_at_once(fd, buffer, pic_size + CHECK_SUM_BYTES);
		if ( error )
		{
			write_command(fd, cmd_strings[TRANSFER_PIC_AT_ONCE], strlen(cmd_strings[TRANSFER_PIC_AT_ONCE]) );
			error = read_image_at_once(fd, buffer, pic_size + CHECK_SUM_BYTES);
		}
#else
		/*Send picture packet */

		unsigned char pic_packet_buf[PIC_PACKET_SIZE + CHECK_SUM_BYTES];
		nofpackets = pic_size/ PIC_PACKET_SIZE;
		restbytes  = pic_size%PIC_PACKET_SIZE;

		for(i=0; i <= nofpackets ; i++)
		{
			printf("i=%d, pic_size = %d, packet_size = %d, number of packets = %d, restbytes = %d \n",i, pic_size, PIC_PACKET_SIZE, nofpackets,restbytes );
			sprintf(tmpbuf, "IP%d#", i );

			write_command(fd, tmpbuf, strlen(tmpbuf) );
			//sleep(1);
			if( i == nofpackets)
				error = read_image_packet(fd, pic_packet_buf, restbytes + CHECK_SUM_BYTES);
			else
				error = read_image_packet(fd, pic_packet_buf, PIC_PACKET_SIZE + CHECK_SUM_BYTES);

			if ( error )
			{
				--i;
			}
			else
			{
				if( i == nofpackets)
				{
					memcpy(buffer + (i * PIC_PACKET_SIZE), pic_packet_buf, restbytes);
					printf("i=%d, last_packet_size = %d \n", i,restbytes );
				}
				else
					memcpy(buffer + (i * PIC_PACKET_SIZE), pic_packet_buf, PIC_PACKET_SIZE);
			}

		}

#endif
		if (jpeg_or_base64_mode == JPEG_MODE)
		{
			fwrite(buffer, pic_size,1,write_ptr); // don't 4 byte is check sum
			printf("\nWrite jpeg file size = %d\n", pic_size );
			fclose(write_ptr);
			system("see test.jpeg");
		}
		else
		{
			fwrite(buffer, pic_size,1,write_ptr); //don't add 4 byte is check sum
			printf("\nWrite base64 file size = %d\n", pic_size);
			fclose(write_ptr);
			system("gedit test.html");
		}
		/*Free picture buffer */
		free(buffer);
    }

	clock_t end=clock();
	printf("Time taken:%lf\n",(double)(end-begin)/CLOCKS_PER_SEC);

}

