#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <threads.h>
#include <inttypes.h>

#include <unistd.h>

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>

#define MAX_EVENT 32

#define err(MSG) perror(MSG), exit(EXIT_FAILURE)
#define prr(...) fprintf(stderr, __VA_ARGS__)

#define TIMER(TIMEOUT, EVENT) (&(struct timer) {			\
	.timeout = (TIMEOUT),						\
	.evfd = (EVENT)							\
})

#define EVENTF(EVENTS, FD) EVENT(EVENTS, (epoll_data_t) { .fd = (FD) })

#define EVENT(EVENTS, DATA) (&(struct epoll_event) {			\
	.events = (EVENTS),						\
	.data = (DATA)							\
})

typedef struct timer {
	int timeout;
	int evfd;
} *Timer;

int timer_thread(void *arg)
{
	Timer timer = arg;

	for (int i = 0; i < 3; i++) {
		prr("wait %d second(s)... \n", timer->timeout);

		sleep(timer->timeout);

		prr("raise event: %d\n", timer->timeout);

		if (eventfd_write(timer->evfd, timer->timeout) == -1)
			prr("failed to eventfd_write(): ");
	}

	prr("timer_thread() done.");

	// write `-1` cause error.
	if (eventfd_write(timer->evfd, -2) == -1)
		prr("failed to eventfd_write(): ");

	return 0;
}

int main(int argc, char *argv[])
{
	int epfd, evfd;
	thrd_t tid;

	if ((epfd = epoll_create(1)) == -1)
		err("failed to epoll_create(): ");

	if ((evfd = eventfd(0, 0)) == -1)
		err("failed to eventfd(): ");

	if (epoll_ctl(epfd, EPOLL_CTL_ADD, evfd, EVENTF(EPOLLIN, evfd)) == -1)
		err("failed to epoll_ctl(): ");

	if (thrd_create(&tid, timer_thread, TIMER(3, evfd)) == -1)
		err("failed to thrd_create(): ");

	while (true) {
		struct epoll_event events[MAX_EVENT];

		int retval = epoll_wait(epfd, events, MAX_EVENT, -1);
		if (retval == -1)
			err("failed to epoll_wait(): ");

		prr("%d event(s) occured.\n", retval);
		for (int i = 0; i < retval; i++)
		{
			struct epoll_event *event = &events[i];

			if (events->events & EPOLLIN) {
				uint64_t data;

				// you have to read the data to clear eventfd
				// `eventfd_read()` makes value to zero.
				if (eventfd_read(events->data.fd, &data) == -1)
					err("failed to eventfd_read(): ");

				prr("read: %" PRIu64 "\n", data);

				if (data == -2)
					goto CLOSE_REQUEST;
			}
		}

		continue;

	CLOSE_REQUEST:	break;
	}

	prr("main thread done.\n");

	if (thrd_join(tid, NULL) == -1)
		err("failed to thrd_join(): ");

	if (epoll_ctl(epfd, EPOLL_CTL_DEL, evfd, NULL) == -1)
		err("failed to epoll_ctl(): ");

	close(evfd);
	close(epfd);

	return 0;
}
