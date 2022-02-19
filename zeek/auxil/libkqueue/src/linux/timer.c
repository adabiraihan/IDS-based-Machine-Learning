/*
 * Copyright (c) 2009 Mark Heily <mark@heily.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "private.h"

#ifndef HAVE_SYS_TIMERFD_H

/* Android 4.0 does not have this header, but the kernel supports timerfds */
#ifndef SYS_timerfd_create
#ifdef __ARM_EABI__
#define __NR_timerfd_create             (__NR_SYSCALL_BASE+350)
#define __NR_timerfd_settime            (__NR_SYSCALL_BASE+353)
#define __NR_timerfd_gettime            (__NR_SYSCALL_BASE+354)
#else
#error Unsupported architecture, need to get the syscall numbers
#endif

#define SYS_timerfd_create __NR_timerfd_create
#define SYS_timerfd_settime __NR_timerfd_settime
#define SYS_timerfd_gettime __NR_timerfd_gettime
#endif /* ! SYS_timerfd_create */

/* XXX-FIXME
   These are horrible hacks that are only known to be true on RHEL 5 x86.
 */
#ifndef SYS_timerfd_settime
#define SYS_timerfd_settime (SYS_timerfd_create + 1)
#endif
#ifndef SYS_timerfd_gettime
#define SYS_timerfd_gettime (SYS_timerfd_create + 2)
#endif

int timerfd_create(int clockid, int flags)
{
  return syscall(SYS_timerfd_create, clockid, flags);
}

int timerfd_settime(int ufc, int flags, const struct itimerspec *utmr,
                    struct itimerspec *otmr)
{
  return syscall(SYS_timerfd_settime, ufc, flags, utmr, otmr);
}

int timerfd_gettime(int ufc, struct itimerspec *otmr)
{
  return syscall(SYS_timerfd_gettime, ufc, otmr);
}

#endif

#ifndef NDEBUG
static char *
itimerspec_dump(struct itimerspec *ts)
{
    static __thread char buf[1024];

    snprintf(buf, sizeof(buf),
            "itimer: [ interval=%lu s %lu ns, next expire=%lu s %lu ns ]",
            ts->it_interval.tv_sec,
            ts->it_interval.tv_nsec,
            ts->it_value.tv_sec,
            ts->it_value.tv_nsec
           );

    return (buf);
}
#endif

/* Convert time data into seconds+nanoseconds */

#define NOTE_TIMER_MASK (NOTE_ABSOLUTE-1)

static void
convert_timedata_to_itimerspec(struct itimerspec *dst, long src,
                               unsigned int flags, int oneshot)
{
    time_t sec, nsec;

    switch (flags & NOTE_TIMER_MASK) {
    case NOTE_USECONDS:
        sec = src / 1000000;
        nsec = (src % 1000000);
        break;

    case NOTE_NSECONDS:
        sec = src / 1000000000;
        nsec = (src % 1000000000);
        break;

    case NOTE_SECONDS:
        sec = src;
        nsec = 0;
        break;

    default: /* milliseconds */
        sec = src / 1000;
        nsec = (src % 1000) * 1000000;
    }

    /* Set the interval */
    if (oneshot) {
        dst->it_interval.tv_sec = 0;
        dst->it_interval.tv_nsec = 0;
    } else {
        dst->it_interval.tv_sec = sec;
        dst->it_interval.tv_nsec = nsec;
    }

    /* Set the initial expiration */
    dst->it_value.tv_sec = sec;
    dst->it_value.tv_nsec = nsec;
    dbg_printf("%s", itimerspec_dump(dst));
}

int
evfilt_timer_copyout(struct kevent *dst, struct knote *src, void *ptr)
{
    struct epoll_event * const ev = (struct epoll_event *) ptr;
    uint64_t expired;
    ssize_t n;

    memcpy(dst, &src->kev, sizeof(*dst));
    if (ev->events & EPOLLERR)
        dst->fflags = 1; /* FIXME: Return the actual timer error */

    /* On return, data contains the number of times the
       timer has been trigered.
     */
    n = read(src->data.pfd, &expired, sizeof(expired));
    if (n != sizeof(expired)) {
        dbg_puts("invalid read from timerfd");
        expired = 1;  /* Fail gracefully */
    }
    dst->data = expired;

    return (0);
}

int
evfilt_timer_knote_create(struct filter *filt, struct knote *kn)
{
    struct itimerspec ts;
    int tfd;
    int flags;
    int events;

    kn->kev.flags |= EV_CLEAR;

    tfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tfd < 0) {
        if ((errno == EMFILE) || (errno == ENFILE)) {
            dbg_perror("timerfd_create(2) fd_used=%u fd_max=%u", get_fd_used(), get_fd_limit());
        } else {
            dbg_perror("timerfd_create(2)");
        }
        return (-1);
    }
    dbg_printf("timer_fd=%i - created", tfd);

    convert_timedata_to_itimerspec(&ts, kn->kev.data, kn->kev.fflags,
                                   kn->kev.flags & EV_ONESHOT);
    flags = (kn->kev.fflags & NOTE_ABSOLUTE) ? TFD_TIMER_ABSTIME : 0;
    if (timerfd_settime(tfd, flags, &ts, NULL) < 0) {
        dbg_printf("timerfd_settime(2): %s", strerror(errno));
        close(tfd);
        return (-1);
    }

    events = EPOLLIN | EPOLLET;
    if (kn->kev.flags & (EV_ONESHOT | EV_DISPATCH))
        events |= EPOLLONESHOT;

    KN_UDATA(kn);   /* populate this knote's kn_udata field */
    if (epoll_ctl(filter_epoll_fd(filt), EPOLL_CTL_ADD, tfd, EPOLL_EV_KN(events, kn)) < 0) {
        dbg_printf("epoll_ctl(2): %d", errno);
        close(tfd);
        return (-1);
    }

    kn->data.pfd = tfd;
    return (0);
}

int
evfilt_timer_knote_modify(struct filter *filt, struct knote *kn,
        const struct kevent *kev)
{
    (void)filt;
    (void)kn;
    (void)kev;
    return (0); /* STUB */
}

int
evfilt_timer_knote_delete(struct filter *filt, struct knote *kn)
{
    int rv = 0;

    if (kn->data.pfd == -1)
        return (0);

    if (epoll_ctl(filter_epoll_fd(filt), EPOLL_CTL_DEL, kn->data.pfd, NULL) < 0) {
        dbg_printf("epoll_ctl(2): %s", strerror(errno));
        rv = -1;
    }

    dbg_printf("timer_fd=%i - closed", kn->data.pfd);
    if (close(kn->data.pfd) < 0) {
        dbg_printf("close(2): %s", strerror(errno));
        rv = -1;
    }

    kn->data.pfd = -1;
    return (rv);
}

int
evfilt_timer_knote_enable(struct filter *filt, struct knote *kn)
{
    return evfilt_timer_knote_create(filt, kn);
}

int
evfilt_timer_knote_disable(struct filter *filt, struct knote *kn)
{
    return evfilt_timer_knote_delete(filt, kn);
}

const struct filter evfilt_timer = {
    .kf_id      = EVFILT_TIMER,
    .kf_copyout = evfilt_timer_copyout,
    .kn_create  = evfilt_timer_knote_create,
    .kn_modify  = evfilt_timer_knote_modify,
    .kn_delete  = evfilt_timer_knote_delete,
    .kn_enable  = evfilt_timer_knote_enable,
    .kn_disable = evfilt_timer_knote_disable,
};
