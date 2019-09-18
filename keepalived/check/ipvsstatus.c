#include <stddef.h>
#include "global_data.h"
#include "check_data.h"

#include "memory.h"
#include "scheduler.h"
#include "logger.h"
#include "tcp_server.h"

static int ipvs_status_dumper(thread_t*);
static thread_t *dumper_thread;
static pthread_t tcp_server_thread_id = 0;
#define BUF_ALLOC_SIZE 1024

int start_status_server() {
	int ret;
	ret = pthread_create(&tcp_server_thread_id, NULL, &tcp_server, global_data->status_port);

	if (ret == 0) {
		printf("Thread created successfully %d.\n", tcp_server_thread_id);
		//if (!pthread_detach(tcp_server_thread_id))
	//	printf("Thread detached successfully !!!\n");
	} else {
		printf("Thread not created.\n");
		return 0;
	}
}

int status_socket_init(void) {
	char **res;
	dumper_thread = thread_add_timer(master, ipvs_status_dumper, NULL,
			5 * TIMER_HZ);
	log_message(LOG_INFO, "registered dumper_thread");
	if (tcp_server_thread_id == 0)
		start_status_server();
	//dump_config(res);
	return 0;
}

int status_socket_close(void) {
	// TODO: close socket
	log_message(LOG_INFO, "terminating dumper_thread");
	thread_cancel(dumper_thread);
	return 0;
}

int status_socket_reload(void) {
	if (1 /* TODO: socket config changed */) {
		log_message(LOG_INFO, "reloading dumper_thread");

		status_socket_close();
		status_socket_init();
	}
	return 0;
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

int snprintf2(int *allocated, int *str_pos, char *str, const char *format, ...) {
/*
 * враппер над  snprintf для автоматической аллокации
 * allocated - сколько аллоцировано для str
 * str_pos - позиция в res
 * str - начало целевой строки
 */
	va_list va;
	int status, n;
	size_t size = 3000;
	status = check_increase_buf(*allocated - *str_pos, allocated, &str);
	if (status != 0)
		goto ret;
	va_start (va, format);
	n = vsnprintf(str + *str_pos, size, format, va);
	va_end (va);
	*str_pos += n;
	if (n == *allocated - *str_pos) {
		status = -2;
		goto ret;
	}
	ret:
		return status;
}

int dump_config(char **out_res, int *size) {
	element e1, e2;
	virtual_server_t *vs = NULL;
	real_server_t *rs = NULL;
	char vs_addr[100];
	int n;

	char *res = NULL;
	res = malloc(sizeof(char) * 1024);
	int res_pos = 0;
	int allocated = 0;
	int status = 0;
	int json_comma = 0;
	if (LIST_ISEMPTY(check_data->vs))
	{
		log_message(LOG_INFO, "empty check_data");
		return 0;
	}

	for (e1 = LIST_HEAD(check_data->vs); e1; ELEMENT_NEXT(e1)) {

		vs = ELEMENT_DATA(e1);
		inet_sockaddrtopair(&vs->addr, &vs_addr);

		status = snprintf2(&allocated, &res_pos, res,
				"conf:\n"
				"  - vip: %s\n"
				"    port: %d\n"
				"    quorum_state: %d\n"
				"    quorum_up: %s\n"
				"    quorum: %d\n",
				vs_addr,
				ntohs(inet_sockaddrport(&vs->addr)),
				vs->quorum_state,
				vs->quorum_up,
				vs->quorum);

		if (!LIST_ISEMPTY(vs->rs))
			for (e2 = LIST_HEAD(vs->rs); e2; ELEMENT_NEXT(e2)) {
				rs = ELEMENT_DATA(e2);
				inet_sockaddrtopair(&rs->addr, &vs_addr);
				status = snprintf2(&allocated, &res_pos, res,
						"    rs:\n"
						"      - {ip: %s, port: %d, alive: %d}\n",
						vs_addr,
						ntohs(inet_sockaddrport(&vs->addr)),
						rs->alive);
				if (status) {
					goto error;
				}
			}
	}
	*size = res_pos;
	*out_res = res;
	return status;

	error:
		FREE(res);
		return status;
}

static int ipvs_status_dumper(thread_t *thread) {
	global_data = alloc_global_data();
	char **res;
	log_message(LOG_INFO, "dumper_thread timer event");
	//dump_check_data()
	//dump_global_data(global_data);

	//dump_config(res);
	dumper_thread = thread_add_timer(master, ipvs_status_dumper, NULL,
			5 * TIMER_HZ);
	return 0;
}
