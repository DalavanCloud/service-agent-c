/******************************************************************************
   Copyright [2016] [Comcast]

   Comcast Proprietary and Confidential

   All Rights Reserved.

   Unauthorized copying of this file, via any medium is strictly prohibited

******************************************************************************/

#include "svcagt_log.h"
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "pthread.h"
#include "svcagt_time.h"


#define LEVEL_ERROR 0
#define LEVEL_INFO  1
#define LEVEL_DEBUG 2


#ifdef TEST_ENVIRONMENT
#define MAX_FILE_SIZE 75000
int current_level = LEVEL_DEBUG;
#else
#define MAX_FILE_SIZE 75000
int current_level = LEVEL_INFO;
#endif

/* log handling */
svcagtLogHandler     log_handler =NULL;

pthread_mutex_t log_mutex=PTHREAD_MUTEX_INITIALIZER;

#ifdef TEST_ENVIRONMENT
const char *test_log_date = NULL; // set this for testing
#endif

const char *jwt_log_dir = NULL;

const char *level_names[3] = {"ERROR", "INFO ", "DEBUG"};

int current_fd = -1;

int current_file_num = -1;

char current_file_date[8];

char current_file_name[128];

long current_file_size = 0;

void print_errno (void)
{
		char errbuf[100];
		printf ("%s\n", strerror_r (errno, errbuf, 100));
}

void dbg (const char *fmt, ...)
{
#ifdef TEST_ENVIRONMENT
    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vprintf(fmt, arg_ptr);
    va_end(arg_ptr);
#endif
}

void dbg_err (int err, const char *fmt, ...)
{
#ifdef TEST_ENVIRONMENT
		char errbuf[100];

    va_list arg_ptr;
    va_start(arg_ptr, fmt);
    vprintf(fmt, arg_ptr);
    va_end(arg_ptr);
		printf ("%s\n", strerror_r (err, errbuf, 100));
#endif
}

static int my_current_date (char *date)
{
#ifdef TEST_ENVIRONMENT
	if (test_log_date != NULL) {	
		if (strlen (test_log_date) == 8) {
			strncpy (date, test_log_date, 8);
			return 0;
		}
		test_log_date += 1;
	}
#endif
	return get_current_date (date);
}


static void make_file_name (void)
{
	sprintf (current_file_name, "%s/log%8.8s.%d", 
		jwt_log_dir, current_file_date, current_file_num);
}

bool log_level_is_debug (void)
{
	return (current_level == LEVEL_DEBUG);
}

int get_valid_file_num (const char *file_name, const char *date)
{
#define IS_DIGIT(c) (((c)>='0') && ((c)<='9'))
	char c;
	int i, val;
	if (strncmp (file_name, "log", 3) != 0)
		return -1;
	if (strncmp (file_name+3, date, 8) != 0)
		return -1;
	if (file_name[11] != '.')
		return -1;
	val = 0;
	if (file_name[12] == 0)
		return -1;
	for (i=12; (c=file_name[i]) != 0; i++) {
		if (!IS_DIGIT (c))
			return -1;
		val = (val*10) + (c-'0');
	}
	if (val == 0)
		return -1;
	return val;
}

int get_last_file_num_in_dir (const char *date, const char *log_dir)
{
	int file_num = -1;
	struct dirent *dent;
	DIR *dirp = opendir (log_dir);

	if (dirp == NULL) {
		dbg_err (errno, "Could not open log directory %s\n", log_dir);
		return -1;
	}

	while (1) {
		int new_file_num = -1;

		dent = readdir (dirp);
		if (dent == NULL)
			break;
		if (dent->d_type != DT_REG)
			continue;
		new_file_num = get_valid_file_num (dent->d_name, date);
		if (new_file_num > file_num)
			file_num = new_file_num;
	}

	closedir (dirp);
	return file_num;
}

int get_last_file_num (const char *date)
{
	return get_last_file_num_in_dir (date, jwt_log_dir);
}

static void close_current_file (void)
{
	if (current_fd != -1) {
		close (current_fd);
		current_fd = -1;
	}
}

static int get_current_file_size (void)
{
	int err;
	struct stat file_stat;

	err = fstat (current_fd, &file_stat);
	if (err != 0) {
		dbg_err (errno, "Could not stat file %s\n", current_file_name);
		close_current_file ();
		return -1;
	}
	current_file_size = (long) file_stat.st_size;
	return 0;
}

static int open_next_file (void);

static int open_file (bool init)
{
	int flags = O_WRONLY | O_CREAT | O_SYNC;

	if (init)
		flags |= O_APPEND;
	else
		flags |= O_TRUNC;

	make_file_name ();
	current_fd = open (current_file_name, flags, 0666);
	if (current_fd == -1) {
		dbg_err (errno, "Unable to open log file %s\n", current_file_name);
		return -1;
	}
	current_file_size = 0;
	if (init) {
		if (get_current_file_size () != 0)
			return -1;
		if (current_file_size >= MAX_FILE_SIZE)
			return open_next_file ();
	}
	return 0;
}


static int open_next_file (void)
{
	int err;
	char current_date[8];

	close_current_file ();

	err = my_current_date (current_date);
	if (err != 0)
		return err;
	if (strncmp (current_date, current_file_date, 8) == 0) {
		current_file_num++;
	} else {
		strncpy (current_file_date, current_date, 8);
		current_file_num = 1;
	}

	return open_file (false);
}


int log_init (const char *log_directory)
{
	int err;

	if (NULL == log_directory) {
		dbg ("Null log directory given\n");
		return -1;
	}
	jwt_log_dir = log_directory;
	err = mkdir (jwt_log_dir, 0777);
	if ((err == -1) && (errno != EEXIST)) {
		dbg_err (errno, "Failed to create directory %s\n", log_directory);
		return -1;
	}
	err = my_current_date (current_file_date);
	if (err != 0)
		return err;
	dbg ("Current date in logger is %8.8s\n", current_file_date);
	current_file_num = get_last_file_num (current_file_date);
	if (current_file_num <= 0)
		current_file_num = 1;
	return open_file (true);
}

int log_shutdown (void)
{
	close_current_file ();
	return 0;
}

#define MSG_BUF_SIZE 512

int output_log (int level, const char *fmt, va_list arg_ptr, int err_code)
{
	int err = 0;
	char timestamp[TIMESTAMP_BUFLEN];
	char msg_buf[MSG_BUF_SIZE];
	char errbuf[100];
	const char *err_msg;
	int header_len, error_len, buf_limit, nbytes;
	
	err = make_current_timestamp (timestamp);
	if (err != 0)
		return err;

	header_len = sprintf (msg_buf, "[%s] %s: ", timestamp, level_names[level]);
	if (header_len < 0)
		return -1;

	buf_limit = MSG_BUF_SIZE - header_len;
	error_len = 0;
	if (err_code != 0) {
		err_msg = strerror_r (errno, errbuf, 100);
		error_len = strlen (err_msg)+2;
		buf_limit -= error_len;
	}

  nbytes = vsnprintf(msg_buf+header_len, buf_limit, fmt, arg_ptr);
	if (nbytes < 0)
		return -1;
	nbytes = strlen(msg_buf);
	if (err_code != 0) {
		strcpy (msg_buf+nbytes, ": ");
		strcpy (msg_buf+nbytes+2, err_msg);
		nbytes += error_len; 
	}

#ifdef TEST_ENVIRONMENT
	write (STDOUT_FILENO, msg_buf, nbytes);
#endif

	pthread_mutex_lock (&log_mutex);
	
	if (current_fd != -1) {
		write (current_fd, msg_buf, nbytes);
		current_file_size += nbytes;
		if (current_file_size >= MAX_FILE_SIZE) {
			err = open_next_file ();
		}
	}
	pthread_mutex_unlock (&log_mutex);
	return err;
}

#define BUF_PRINT(level, fmt, err_code) \
    va_list arg_ptr; \
    va_start(arg_ptr, fmt); \
		output_log (level, fmt, arg_ptr, err_code); \
    va_end(arg_ptr);

void log_not (const char *fmt, ...)
{
}

void log_dbg (const char *fmt, ...)
{
	if (current_level >= LEVEL_DEBUG) {
		BUF_PRINT (LEVEL_DEBUG, fmt, 0);
	}
}

void log_info (const char *fmt, ...)
{
	if (current_level >= LEVEL_INFO) {
		BUF_PRINT (LEVEL_INFO, fmt, 0);
	}
}

void log_error (const char *fmt, ...)
{
	BUF_PRINT (LEVEL_ERROR, fmt, 0);
}

void log_errno (int err, const char *fmt, ...)
{
	BUF_PRINT (LEVEL_ERROR, fmt, err);
}



/**
 * @brief Allows to define a log handler that will receive all logs
 * produced under the provided content.
 *
 * @param handler The handler to be called for each log to be
 * notified. Passing in NULL is allowed to remove any previously
 * configured handler.
 *
 */
void  svcagt_log_set_handler (svcagtLogHandler handler)
{
        log_handler   = handler;
        return;
}


void svcagt_log (  int level, const char *msg, ...)
{
	char *pTempChar = NULL;
	int ret = 0;
	
	va_list arg_ptr; 
	
	pTempChar = (char *)malloc(4096);	
	if(pTempChar)
	{
		va_start(arg_ptr, msg); 
		ret = vsnprintf(pTempChar, 4096, msg, arg_ptr);
		if(ret < 0)
		{
			perror(pTempChar);
		}
		
		va_end(arg_ptr);
		
		
		if(log_handler != NULL)
		{
			log_handler (level, pTempChar);
		
			if(pTempChar !=NULL)
			{
				free(pTempChar);
				pTempChar = NULL;
			}
			
			return;
		}
		else
		{
			switch(level)
			{
				case 0:
					log_error (pTempChar);
					break;
				case 1:
					log_info (pTempChar);
					break;
				case 2:
					log_dbg (pTempChar);
					break;
			}
			
			if(pTempChar !=NULL)
			{
				free(pTempChar);
				pTempChar = NULL;
			}
			
			return;
		}	
	}
	return;	
}

