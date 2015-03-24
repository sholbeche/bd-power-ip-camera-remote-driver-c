// Code: ipcamera.c
// Author: Simon Holbeche
//
// Program to send commands by http to DB Power web camera PTZ including sequences to capture images 
// Intended to be run on Raspberry Pi as Cron task
//   use  crontab -e to setup
//   */15 * * * * sudo ./ipcamera


#include <stdio.h> 
#include <stdlib.h> 
#include <stddef.h> 
#include <string.h> 
#include <curl/curl.h>
#include <time.h>
#include "ipcamera.h"

#define PAN_LEFT   6
#define PAN_RIGHT  4
#define TILT_UP    2
#define TILT_DOWN  0
#define PTZ_CENTRE 25
#define PTZ_BOT_RIGHT 90
#define PTZ_BOT_LEFT  91
#define PTZ_TOP_RIGHT 92
#define PTZ_TOP_LEFT  93
#define DEF_FILENAME "capture.jpg"

struct HttpFile {
	const char *filename;
	FILE *stream;
};

 
static size_t http_fwrite(void *buffer, size_t size, size_t nmemb, void *stream) {
	struct HttpFile *out=(struct HttpFile *)stream;
	if(out && !out->stream) {
/* open file for writing */ 
		out->stream=fopen(out->filename, "wb");
		if(!out->stream)
			return -1; 
/* failure, can't open file to write */ 
	}
	return fwrite(buffer, size, nmemb, out->stream);
}


void cameraReqResp(char activity, int arg1);
void cameraTask(int task);
void secondSleep(float seconds);
void fileCopy(char *sourceFile, char *targetFile);
char * fileDateStamp();
char *replace_str2(const char *str, const char *old, const char *new);


int main(int argc, char *argv[]){
	int i;
	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			cameraTask(atoi(argv[i]));
		}
	}else{
		cameraTask(2);
	}
	return 0;
}


void cameraTask(int task) {
	int x;
	switch(task) {
		case 1 : // start PTZ to 'centre' after several passes and drop to level
			cameraReqResp('m',PTZ_CENTRE);
			secondSleep(50); // Allow time for PTZ to complete
    			for ( x = 0; x < 8; x++ ) {
				cameraReqResp('m',TILT_DOWN);
				secondSleep(0.5);
			}
			break;
		case 2 : // Assuming centred on S, track E through to W and back to S with capture at 90 deg intervals
			cameraReqResp('c',0);
			secondSleep(2);
			fileCopy(DEF_FILENAME,"south.jpg");
	    		for ( x = 0; x < 11; x++ ) {
				cameraReqResp('m',PAN_LEFT);
				secondSleep(0.5);
			}
			cameraReqResp('c',0);
			secondSleep(2);
			fileCopy(DEF_FILENAME,"southeast.jpg");
    			for ( x = 0; x < 11; x++ ) {
				cameraReqResp('m',PAN_LEFT);
				secondSleep(0.5);
			}
			cameraReqResp('c',0);
			secondSleep(2);
			fileCopy(DEF_FILENAME,"east.jpg");
    			for ( x = 0; x < 33; x++ ) {
				cameraReqResp('m',PAN_RIGHT);
				secondSleep(0.5);
			}
			cameraReqResp('c',0);
			secondSleep(2);
			fileCopy(DEF_FILENAME,"southwest.jpg");
    			for ( x = 0; x <11; x++ ) {
				cameraReqResp('m',PAN_RIGHT);
				secondSleep(0.5);
			}
			cameraReqResp('c',0);
			secondSleep(2);
			fileCopy(DEF_FILENAME,"west.jpg");
    			for ( x = 0; x < 22; x++ ) {
				cameraReqResp('m',PAN_LEFT);
				secondSleep(0.5);
			}
//			cameraReqResp('c',0);
			break;
		case 3 : // Go to top left datum and then track to centre
			cameraReqResp('m',PTZ_TOP_LEFT);
			secondSleep(15);
    			for ( x = 0; x < 22; x++ ) {
				cameraReqResp('m',PAN_RIGHT);
				secondSleep(0.5);
				cameraReqResp('m',PAN_RIGHT);
				secondSleep(0.5);
				cameraReqResp('m',TILT_DOWN);
				secondSleep(0.5);
			}
			break;
		case 4 : // capture snapshot image
			cameraReqResp('c',0);
			secondSleep(2);
			fileCopy(DEF_FILENAME,"snapshot.jpg");
			break;
		case 5 : // pan one step left
			cameraReqResp('m',PAN_LEFT);
			secondSleep(0.5);
			break;
                case 6 : // pan one step right
                        cameraReqResp('m',PAN_RIGHT);
			secondSleep(0.5);
                        break;
                case 7 : // pan one step down
                        cameraReqResp('m',TILT_DOWN);
			secondSleep(0.5);
                        break;
                case 8 : // pan one step up
                        cameraReqResp('m',TILT_UP);
			secondSleep(0.5);
                        break;

		default :
			printf("No task found to action");
	} 
}


void secondSleep(float seconds) {
	usleep((long) (seconds*1000000.0));
}


void cameraReqResp(char activity, int arg1) {
	CURL *curl;
	CURLcode res;
	struct HttpFile httpfile={
		DEF_FILENAME, /* name to store the file as if succesful */ 
		NULL
	};
 
	char buf[256];

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if(curl) {

		snprintf(buf,sizeof buf,"%s:%s", USER, PASS);
		curl_easy_setopt(curl, CURLOPT_USERPWD, buf);
		strcpy(buf,"");
		switch(activity){
			case 'c' :
				snprintf(buf, sizeof buf, "http://%s/snapshot.cgi",CAMIP);
				curl_easy_setopt(curl, CURLOPT_URL, buf);
				curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http_fwrite);
				curl_easy_setopt(curl, CURLOPT_WRITEDATA, &httpfile);
				remove(DEF_FILENAME);
				break;
			case 'm' :
//				printf("%d \n", arg1);
				snprintf(buf, sizeof buf, "http://%s/decoder_control.cgi?onestep=1&command=%d",CAMIP,arg1);
				curl_easy_setopt(curl, CURLOPT_URL, buf);
				break;
			case 's' :
				break;
			default :
				printf("Z \n");
		}

		curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);
 
		if(CURLE_OK != res) {
// Handle failed curl request 
			fprintf(stderr, "curl told us %d\n", res);
		}
	}
   return;
}


void fileCopy(char *sourceFileFull, char *targetFileFull) {
	char buffer[100];
	char bufferBash[100];
	char fileFull[25];
	char *fileName;
	char *fileExt;  
	time_t now;
	struct tm * timeinfo;

// write storage by date directories
	now = time (NULL);
	time ( &now);
	timeinfo = localtime ( &now);
	int written = sprintf(buffer, "mkdir -p %s", IMAGEPATH);
	strftime(buffer+written, 100-written, "%Y", timeinfo);
	system(buffer);
	strftime(buffer+written, 100-written, "%Y/%Y-%m", timeinfo);
	system(buffer);

// copy source to IMAGEPATH with new target filename
	strcpy(buffer, "");
	snprintf(buffer,sizeof buffer, "cp %s %s%s",sourceFileFull,IMAGEPATH,targetFileFull);
	system(buffer);
	strcpy(fileFull,targetFileFull);
	fileName = strtok(fileFull,".");
	fileExt = strtok(NULL,".");

// copy source to history with timestamped filename
	strcpy(buffer, "");
	strftime(buffer,sizeof buffer, "cp %%s %%s%Y/%Y-%m/%%s-%Y%m%d-%H%M%S.%%s", timeinfo);
	snprintf(bufferBash,sizeof bufferBash, buffer,sourceFileFull,IMAGEPATH,fileName,fileExt);
	system(bufferBash);

   return;
}


char * fileDateStamp() {
	static char buf[150];
   time_t curtime;
   struct tm *loc_time;

   //Getting current time of system
   curtime = time (NULL);

   // Converting current time to local time
   loc_time = localtime (&curtime);

   strftime (buf, 150, "%Y%m%d%H%M", loc_time);
//	printf("%s \n",buf);
	char *x;
	x = (char*) &buf;
	return x;
}

char *replace_str2(const char *str, const char *old, const char *new)
{
	char *ret, *r;
	const char *p, *q;
	size_t oldlen = strlen(old);
	size_t count, retlen, newlen = strlen(new);
	int samesize = (oldlen == newlen);

	if (!samesize) {
		for (count = 0, p = str; (q = strstr(p, old)) != NULL; p = q + oldlen)
			count++;
		/* This is undefined if p - str > PTRDIFF_MAX */
		retlen = p - str + strlen(p) + count * (newlen - oldlen);
	} else
		retlen = strlen(str);

	if ((ret = malloc(retlen + 1)) == NULL)
		return NULL;

	r = ret, p = str;
	while (1) {
		/* If the old and new strings are different lengths - in other
		 * words we have already iterated through with strstr above,
		 * and thus we know how many times we need to call it - then we
		 * can avoid the final (potentially lengthy) call to strstr,
		 * which we already know is going to return NULL, by
		 * decrementing and checking count.
		 */
		if (!samesize && !count--)
			break;
		/* Otherwise i.e. when the old and new strings are the same
		 * length, and we don't know how many times to call strstr,
		 * we must check for a NULL return here (we check it in any
		 * event, to avoid further conditions, and because there's
		 * no harm done with the check even when the old and new
		 * strings are different lengths).
		 */
		if ((q = strstr(p, old)) == NULL)
			break;
		/* This is undefined if q - p > PTRDIFF_MAX */
		ptrdiff_t l = q - p;
		memcpy(r, p, l);
		r += l;
		memcpy(r, new, newlen);
		r += newlen;
		p = q + oldlen;
	}
	strcpy(r, p);

	return ret;
}
