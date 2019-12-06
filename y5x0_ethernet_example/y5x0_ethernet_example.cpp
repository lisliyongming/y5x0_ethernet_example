// y5x0_ethernet_example.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <stdlib.h>
#include <assert.h>

#if _WIN64 || _WIN32
#include <winsock2.h>
#include <windows.h>
#include <Ws2tcpip.h>
#include <inttypes.h>
#pragma  comment(lib, "ws2_32.lib")
#else 
#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#define SOCKET int
#define SOCKADDR_IN (struct sockaddr_in)
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#endif

//#define NBYTE_PER_FRAME (13*1024 * 1024)
#define NBYTE_PER_FRAME (1*1024*1024)
#define TESTING_PPS_TIMES 60
#define CMD_START_SAMPLING 0x102
#define CMD_STOP_SAMPLING  0x103
#define CMD_READ_GPS       0x101
#define CMD_CONFIG_PPS_SEL        0x104
#define CMD_LOL_FREQ_CONFIG       0x105
#define CMD_RX1_GAIN_CONFIG       0x106
#define CMD_RX2_GAIN_CONFIG       0x107

#define TEST_GPS
#define TEST_DATA_SAMPLE
#define SAVING_DATA
//#define TEST_LOL_FREQ
//#define TEST_PPS_SEL
#define TEST_RX1_GAIN
//#define TEST_RX2_GAIN

#define MAX_TEST_CYCLE 1

#define GPS_FIXED 0x00
#define NO_GNSS_TIME  0x01
#define NO_USABLE_SATELLITE	 0x08
#define ONLY_ONE_USABLE_SAT	 0x09
#define ONLY_TWO_USABLE_SAT	 0x0A
#define ONLY_THREE_USABLE_SAT	 0x0B
#define CHOSENED_SAT_UNUSABLE	 0x0C
#pragma pack(2)

typedef struct {
	uint16_t head;
	uint16_t dev_id;
	uint16_t cmd_id;
	uint32_t data0;
	uint32_t data1;
	uint32_t data2;
	uint32_t data3;
	uint32_t data4;
	uint16_t tail;
} CMD;

#if defined(__GNUC__)
typedef struct gps_info {
	uint64_t   time;
	uint64_t   latitude;
	uint64_t   longitude;
	uint64_t   altitude;
}__attribute__((gcc_struct, packed)) GPS;
#elif defined(_MSC_VER)
#pragma pack(2)
typedef struct gps_info {
	uint64_t   time;
	uint64_t   latitude;
	uint64_t   longitude;
	uint64_t   altitude;
} GPS;
typedef struct gps_status {
	uint8_t   receiver_mode;
	uint8_t   disciplining_mode;
	uint16_t  minor_alarms;
	uint8_t   gnss_decoding_status;
	uint8_t   disciplining_activity;
	uint8_t   pps_indication;
	uint8_t   pps_reference;
} GPS_STATUS;
#pragma pack()
#endif

typedef struct yunsdr_meta {
	uint32_t head;
	uint32_t nsamples;
	uint32_t timestamp_l;
	uint32_t timestamp_h;
	uint32_t payload[0];
}YUNSDR_META;

typedef struct rf_frequency {
	uint32_t fre_high;
	uint32_t fre_low;
}RF_FREQUENCY;

typedef union long_t FLOATType;

union long_t {
	float f;
	uint32_t l;
	struct {
		uint8_t llo;
		uint8_t lo;
		uint8_t hi;
		uint8_t hhi;
	}b;
};

typedef union long64_t DOUBLEType;
union long64_t {
	double f;
	uint64_t l;
	struct {
		uint8_t llo1;
		uint8_t lo1;
		uint8_t hi1;
		uint8_t hhi1;

		uint8_t llo2;
		uint8_t lo2;
		uint8_t hi2;
		uint8_t hhi2;
	}b;
};

float myntohf(uint32_t p)
{
	FLOATType tmpf;
	tmpf.b.llo = p & 0xff;
	tmpf.b.lo = p >> 8 & 0xff;
	tmpf.b.hi = p >> 16 & 0xff;
	tmpf.b.hhi = p >> 24 & 0xff;

	return tmpf.f;
}

double ntohlf(uint64_t p)
{
	DOUBLEType tmpf;
	tmpf.b.llo1 = p >> 56 & 0xff;
	tmpf.b.lo1 = p >> 48 & 0xff;
	tmpf.b.hi1 = p >> 40 & 0xff;
	tmpf.b.hhi1 = p >> 32 & 0xff;
	tmpf.b.llo2 = p >> 24 & 0xff;
	tmpf.b.lo2 = p >> 16 & 0xff;
	tmpf.b.hi2 = p >> 8 & 0xff;
	tmpf.b.hhi2 = p & 0xff;

	return tmpf.f;
}



SOCKET tcp_init(char* ip, int port)
{

	int err;
#ifdef WIN32
	WSADATA wsa;
	WORD wVersionRequested;
	WSADATA wsaData;

	WSAStartup(MAKEWORD(2, 2), &wsa);
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		printf("WSAStartup failed with error: %d\n", err);
		return INVALID_SOCKET;
	}

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		printf("Could not find a usable version of Winsock.dll\n");
		WSACleanup();
		return INVALID_SOCKET;
	}
	else
		;//printf("The Winsock 2.2 dll was found okay\n");
#endif

	SOCKET sockfd;
	SOCKADDR_IN addr;

	memset(&addr, 0, sizeof(addr));

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
	{
		printf("Create data socket failed!\n");
		return INVALID_SOCKET;
	}
#ifndef WIN32
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(ip);
	addr.sin_port = htons(port);
#else
	addr.sin_addr.S_un.S_addr = inet_addr(ip);
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
#endif
	int nSendBufLen = 64 * 1024;
	int nRecvBufLen = 64 * 1024;
	int len = sizeof(nRecvBufLen);
	err = setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, (const char*)& nSendBufLen, sizeof(int));
	if (err < 0) {
		printf("setsockopt SO_SNDBUF failed, %s!\n", strerror(errno));
	}

	err = setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (const char*)& nSendBufLen, sizeof(int));
	if (err < 0) {
		printf("setsockopt SO_RCVBUF failed, %s!\n", strerror(errno));
	}
	err = getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)& nRecvBufLen, &len);
	if (err < 0) {
		printf("getsockopt SO_RCVBUF failed, %s!\n", strerror(errno));
	}
	else {
		printf("socket recv buf size:%d\n", nRecvBufLen);
	}

	err = connect(sockfd, (struct sockaddr*) & addr, sizeof(addr));

	if (err < 0) {
		printf("Connect yunsdr failed!\n");
		return INVALID_SOCKET;
	}

	return sockfd;
}
int main(int argc, char** argv)
{
    std::cout << "Hello World!\n";
	setbuf(stdout, NULL);
	int i = 0;
	SOCKET sockfd;
	uint32_t nwrite;
	uint32_t nread;
	uint8_t *buf;
	FILE *fp;
	FILE *fp_time;
	uint32_t *pps_time;
	int j = 0;
	if(argc < 3) {
		printf("Usage: %s <ipaddr> <port>\n"
			"\t%s 192.168.0.27 5001", argv[0], argv[0]);
		exit(0);
	}

	fp = fopen("rx_iq.dat", "wb+");
	if (fp == NULL) {
		printf("Can't open file [rx_iq.dat]\n");
		exit(-1);
	}
	sockfd = tcp_init(argv[1], atoi(argv[2]));

	GPS g_info;
	GPS_STATUS g_status;
	RF_FREQUENCY fre_config;

	//FR local frequency config value, unit is Hz, range from 0~6000000000
	uint64_t lol_freq_value = 2500000000;
	fre_config.fre_high = (uint32_t)(lol_freq_value >> 32);
	fre_config.fre_low = (uint32_t)lol_freq_value;

	//PPS mode: pps_sel
	//select_flag = 0 : internal logic generate PPS signal
	//select_flag = 1 : GPS generate PPS signal
	//select_flag = 2 : external trigger input PPS signal
	uint32_t pps_sel = 1; //PPS selection config flag

	//RX gain configuration value, range from 0~63, represent actual gain value 0~31.5
	uint32_t rx1_gain_value = 12;
	uint32_t rx2_gain_value = 16;

	CMD cmd_start = { htons(0xAA55), htons(0x01), htons(CMD_START_SAMPLING), htonl(NBYTE_PER_FRAME / sizeof(int)), 0, 0, 0, 0, htons(0x55AA) };
	CMD cmd_stop = { htons(0xAA55), htons(0x01), htons(CMD_STOP_SAMPLING), 0, 0, 0, 0, 0, htons(0x55AA) };
	CMD cmd_gps = { htons(0xAA55), htons(0x01), htons(CMD_READ_GPS), htonl(NBYTE_PER_FRAME / sizeof(int)), 0, 0, 0, 0, htons(0x55AA) };
	CMD cmd_freq = { htons(0xAA55), htons(0x01), htons(CMD_LOL_FREQ_CONFIG), htonl(NBYTE_PER_FRAME / sizeof(int)), 0, 0, htonl(fre_config.fre_high), htonl(fre_config.fre_low), htons(0x55AA) };
	CMD cmd_pps = { htons(0xAA55), htons(0x01), htons(CMD_CONFIG_PPS_SEL), htonl(NBYTE_PER_FRAME / sizeof(int)), 0, 0, 0, htonl(pps_sel), htons(0x55AA) };
	CMD cmd_rx1gain = { htons(0xAA55), htons(0x01), htons(CMD_RX1_GAIN_CONFIG), htonl(NBYTE_PER_FRAME / sizeof(int)), 0, 0, 0, htonl(rx1_gain_value), htons(0x55AA) };
	CMD cmd_rx2gain = { htons(0xAA55), htons(0x01), htons(CMD_RX2_GAIN_CONFIG), htonl(NBYTE_PER_FRAME / sizeof(int)), 0, 0, 0, htonl(rx2_gain_value), htons(0x55AA) };


	pps_time = (uint32_t*)malloc((sizeof(uint32_t))*TESTING_PPS_TIMES);
	fp = fopen("rx_iq.dat", "wb+");
	if(fp == NULL) {
		printf("Can't open file [rx_iq.dat]\n");
		exit(-1);
	}
#ifdef TEST_GPS
	for(int i = 0; i < TESTING_PPS_TIMES; i++) {
#ifdef TEST_LOL_FREQ
	/** config local freq via tcp **/
	nwrite = send(sockfd, (char*)& cmd_freq, sizeof(cmd_freq), 0);
	assert(nwrite >= 0);
	Sleep(1000);
#endif

#ifdef TEST_PPS_SEL
	/** config PPS selection via tcp **/
	nwrite = send(sockfd, (char*)& cmd_pps, sizeof(cmd_pps), 0);
	assert(nwrite >= 0);
	Sleep(1000);
#endif

#ifdef TEST_RX1_GAIN
	/** config rx1 gain via tcp **/
	nwrite = send(sockfd, (char*)& cmd_rx1gain, sizeof(cmd_rx1gain), 0);
	assert(nwrite >= 0);
	//Sleep(10);
#endif

#ifdef TEST_RX2_GAIN
	/** config rx2 gain via tcp **/
	nwrite = send(sockfd, (char*)& cmd_rx2gain, sizeof(cmd_rx2gain), 0);
	assert(nwrite >= 0);
	Sleep(1000);
#endif
		/** read gps information from tcp **/
		printf("The testing num is %d\n", i);
		nwrite = send(sockfd, (char *)&cmd_gps, sizeof(cmd_gps), 0);
		assert(nwrite >= 0);

		nread = recv(sockfd, (char*)& g_info, sizeof(GPS), 0);
		nread = recv(sockfd, (char*)& g_status, sizeof(GPS_STATUS), 0);
		assert(nread >= 0);

		printf("%02d:%02d:%02d %02d %02d %02d GPS Time(hour:minute:second day month year)\t\n",
			((g_info.time >> 16) & 0xff) + 8, (g_info.time >> 8) & 0xff, g_info.time & 0xff,
			(g_info.time >> 24) & 0xff, (g_info.time >> 32) & 0xff, ntohs((g_info.time >> 40) & 0xffff));

		printf("GPS time (in hex) is 0x%" PRIx64 "\n", g_info.time);
		printf("latitude:%.8f\t", ntohlf(g_info.latitude) * 180 / 3.1415926);
		printf("longitude:%.8f\t", ntohlf(g_info.longitude) * 180 / 3.1415926);
		printf("altitude:%.8f\n", ntohlf(g_info.altitude));

		printf("receiver_mode : %02x\n", g_status.receiver_mode);
		printf("disciplining_mode : %02x\n", g_status.disciplining_mode);
		printf("minor_alarms : %04x\n", g_status.minor_alarms);
		printf("gnss_decoding_status : %02x\n", g_status.gnss_decoding_status);
		printf("disciplining_activity : %02x\n", g_status.disciplining_activity);
		printf("pps_indication : %02x\n", g_status.pps_indication);
		printf("pps_reference : %02x\n", g_status.pps_reference);

		switch (g_status.gnss_decoding_status) {
		case GPS_FIXED:
		{
			printf("The GPS is locked!\n");
			break;
		}
		case NO_GNSS_TIME:
		{
			printf("The GPS is unlocked and there aren't stable time!\n");
			break;
		}
		case NO_USABLE_SATELLITE:
		{
			printf("The GPS is unlocked and there are not usable satellites!\n");
			break;
		}
		case ONLY_ONE_USABLE_SAT:
		{
			printf("The GPS is unstable and there are only one usable satellites!\n\n");
			break;
		}
		case ONLY_TWO_USABLE_SAT:
		{
			printf("The GPS is unstable and there are only two usable satellites!\n\n");
			break;
		}
		case ONLY_THREE_USABLE_SAT:
		{
			printf("The GPS is unstable and there are only three usable satellites!\n\n");
			break;
		}
		case CHOSENED_SAT_UNUSABLE:
		{
			printf("The GPS is unlocked and the linking satellites are unusable!\n");
			break;
		}
		default:
		{
			printf("NOT useful information!\n");
			break;
		}
	}
#ifndef WIN32
		sleep(1);
#else
		//Sleep(10);
#endif

#endif

#ifdef TEST_DATA_SAMPLE
	buf = (uint8_t*)malloc(NBYTE_PER_FRAME + sizeof(YUNSDR_META));
	nwrite = send(sockfd, (char*)& cmd_start, sizeof(cmd_start), 0);
	assert(nwrite >= 0);

		//		do {
		uint32_t sum = NBYTE_PER_FRAME + sizeof(YUNSDR_META);
		uint8_t* ptr = buf;
		uint32_t count = 0;
		do {
			/** read samples from tcp **/
			nread = recv(sockfd, (char*)ptr, sum - count, 0);
			assert(nread >= 0);
			count += nread;
			ptr += nread;
		} while (sum != count);
		printf("[%d]read bytes:%d\n", j, count);
		//			j++;
		//		} while(j != MAX_TEST_CYCLE);
#ifdef SAVING_DATA
		if (fwrite(buf, count, 1, fp) < 0) {
			printf("Can't write samples to rx_iq.dat\n");
			fclose(fp);
			exit(-1);
		}
#endif

		int end_position = NBYTE_PER_FRAME/4;
		printf("%x\n",*((uint32_t *)buf+end_position));
		//		printf("%x\n",*((uint32_t *)buf+end_position+1));
		printf("%x\n",*((uint32_t *)buf+end_position+2));
		//		printf("%x\n",*((uint32_t *)buf+end_position+3));
		//		nwrite = send(sockfd, (char *)&cmd_stop, sizeof(cmd_stop), 0);

		assert(nwrite >= 0);
		*(pps_time+i) = *((uint32_t *)buf+end_position+2);
		free(buf);
#endif
}
	nwrite = send(sockfd, (char *)&cmd_stop, sizeof(cmd_stop), 0);

	fp_time = fopen("rx_time.dat", "wb");
	if(fp_time == NULL) {
		printf("Can't open file [rx_time.dat]\n");
		exit(-1);
	}
	if(fwrite(pps_time, (sizeof(uint32_t))*TESTING_PPS_TIMES, 1, fp_time) < 0) {
		printf("Can't write samples to rx_time.dat\n");
		fclose(fp_time);
		exit(-1);
	}
	fclose(fp_time);
	free(pps_time);
	fclose(fp);
	printf("Test successful!\n");
	closesocket(sockfd);

	return EXIT_SUCCESS;
}
