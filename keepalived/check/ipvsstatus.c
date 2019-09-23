#include <stddef.h>
#include "global_data.h"
#include "check_data.h"

#include "memory.h"
#include "utils.h"
#include "scheduler.h"
#include "logger.h"
#include "tcp_server.h"
#include "ipvsstatus.h"

static pthread_t tcp_server_thread_id = 0;

#define BUF_ALLOC_SIZE 1024

int start_status_server() {
	int ret;
	ret = pthread_create(&tcp_server_thread_id, NULL, &tcp_server, global_data->status_port);

	if (ret == 0) {
		printf("Thread created successfully %lu.\n", tcp_server_thread_id);
		//if (!pthread_detach(tcp_server_thread_id))
	//	printf("Thread detached successfully !!!\n");
	} else {
		printf("Thread not created.\n");
	}
	return ret;
}

int check_increase_buf(int buf_left, int *allocated, char **res) {
	/*
	 * проверяем что в res достаточно свободного места и делаем realloc если нет
	 */
	char *new_res = NULL;
	int status = 0;
	if (buf_left < BUF_ALLOC_SIZE) {
		*allocated = *allocated + BUF_ALLOC_SIZE * 2;
		new_res = realloc(*res, *allocated);
		if (new_res == NULL)
			status = -1;
		else
			*res = new_res;
	}
	return status;
}

int snprintf2(int *allocated, int *str_pos, char **str, const char *format, ...) {
/*
 * враппер над  snprintf для автоматической аллокации
 * allocated - сколько аллоцировано для str
 * str_pos - позиция в res
 * str - начало целевой строки
 */
	va_list va;
	int status, n;
	size_t size = 3000;
	status = check_increase_buf(*allocated - *str_pos, allocated, str);
	if (status != 0)
		goto ret;
	va_start (va, format);
	n = vsnprintf(*str + *str_pos, size, format, va);
	va_end (va);
	*str_pos += n;
	if (n == *allocated - *str_pos) {
		status = -2;
		goto ret;
	}
	ret:
		return status;
}


int dump_config(char **out_res, int *size, dump_format format) {
	element e1, e2;
	virtual_server_t *vs = NULL;
	real_server_t *rs = NULL;
	char addr[100];
	char *res = NULL;
	char *vs_fmt = NULL;
	char *rs_fmt = NULL;
	int res_pos = 0;
	int allocated = BUF_ALLOC_SIZE;
	int status = 0;

	res = malloc(BUF_ALLOC_SIZE);
	if (LIST_ISEMPTY(check_data->vs))
	{
		log_message(LOG_INFO, "empty check_data");
		return 0;
	}
	if (format == JSON_FORMAT)
	{
		status = snprintf2(&allocated, &res_pos, &res, "{\"conf\":[");
		if (status)
			goto error;
		vs_fmt = "{\"vip\":\"%s\",\"port\":%d,\"quorum_state\":%d,\"quorum_up\":\"%s\",\"quorum\":%d,\"rs\":[";
		rs_fmt = "{\"ip\":\"%s\",\"port\":%d,\"alive\":%d},";
	}
	else
	{
		status = snprintf2(&allocated, &res_pos, &res, "conf:\n");
		if (status)
			goto error;
		vs_fmt =
				"  - vip: %s\n"
				"    port: %d\n"
				"    quorum_state: %d\n"
				"    quorum_up: %s\n"
				"    quorum: %d\n";
		rs_fmt =
				"    rs:\n"
				"      - {ip: %s, port: %d, alive: %d}\n";
	}

	for (e1 = LIST_HEAD(check_data->vs); e1; ELEMENT_NEXT(e1)) {
		vs = ELEMENT_DATA(e1);
		inet_sockaddrtopair(&vs->addr, &addr);

		status = snprintf2(&allocated, &res_pos, &res,
				vs_fmt,
				addr,
				ntohs(inet_sockaddrport(&vs->addr)),
				vs->quorum_state,
				vs->quorum_up,
				vs->quorum);
		if (status)
			goto error;
		if (!LIST_ISEMPTY(vs->rs))
		{
			for (e2 = LIST_HEAD(vs->rs); e2; ELEMENT_NEXT(e2)) {
				rs = ELEMENT_DATA(e2);
				inet_sockaddrtopair(&rs->addr, &addr);
				status = snprintf2(&allocated, &res_pos, &res,
						rs_fmt,
						addr,
						ntohs(inet_sockaddrport(&vs->addr)),
						rs->alive);
				if (status)
					goto error;
			}
			if (format == JSON_FORMAT)
			{
				// замена последней запятой на ]
				res[res_pos-1] = ']';
			}
		}
		if (format == JSON_FORMAT)
			if (snprintf2(&allocated, &res_pos, &res, "},"))
				goto error;
	}
	// замена последней запятой на ]
	res[res_pos-1] = ']';

	if (format == JSON_FORMAT)
		if (snprintf2(&allocated, &res_pos, &res, "}"))
			goto error;
	*size = res_pos;
	*out_res = res;
	return status;

	error:
		log_message(LOG_INFO, "error during dump_config()");
		FREE(res);
		return status;
}
