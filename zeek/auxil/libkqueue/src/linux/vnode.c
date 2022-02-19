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

#ifndef NDEBUG
static char *
inotify_mask_dump(uint32_t mask)
{
    static __thread char buf[1024];

#define INEVT_MASK_DUMP(attrib) \
    if (mask & attrib) \
       strcat(buf, #attrib" ");

    snprintf(buf, sizeof(buf), "mask = %d (", mask);
    INEVT_MASK_DUMP(IN_ACCESS);
    INEVT_MASK_DUMP(IN_MODIFY);
    INEVT_MASK_DUMP(IN_ATTRIB);
    INEVT_MASK_DUMP(IN_CLOSE_WRITE);
    INEVT_MASK_DUMP(IN_CLOSE_NOWRITE);
    INEVT_MASK_DUMP(IN_OPEN);
    INEVT_MASK_DUMP(IN_MOVED_FROM);
    INEVT_MASK_DUMP(IN_MOVED_TO);
    INEVT_MASK_DUMP(IN_CREATE);
    INEVT_MASK_DUMP(IN_DELETE);
    INEVT_MASK_DUMP(IN_DELETE_SELF);
    INEVT_MASK_DUMP(IN_MOVE_SELF);
    buf[strlen(buf) - 1] = ')';

    return (buf);
}

static char *
inotify_event_dump(struct inotify_event *evt)
{
    static __thread char buf[1024];

    if (evt->len > 0)
        snprintf(buf, sizeof(buf), "wd=%d mask=%s name=%s",
                evt->wd,
                inotify_mask_dump(evt->mask),
                evt->name);
    else
        snprintf(buf, sizeof(buf), "wd=%d mask=%s",
                evt->wd,
                inotify_mask_dump(evt->mask));

    return (buf);
}

#endif /* !NDEBUG */


int
get_one_event(struct inotify_event *dst, size_t len, int inofd)
{
    ssize_t n;
    size_t want = sizeof(struct inotify_event);

    dbg_puts("reading one inotify event");
    for (;;) {
        if (len < want) {
            dbg_printf("Needed %zu bytes have %zu bytes", want, len);
            return (-1);
        }

        n = read(inofd, dst, want);
        if (n < 0) {
            switch (errno) {
            case EINVAL:
                want += sizeof(struct inotify_event);
                /* FALL-THROUGH */

            case EINTR:
                continue;
            }

            dbg_perror("read");
            return (-1);
        }
        break;
    }

    dbg_printf("read(2) from inotify wd: %ld bytes", (long)n);

    return (0);
}

static int
add_watch(struct filter *filt, struct knote *kn)
{
    int ifd;
    char path[PATH_MAX];
    uint32_t mask;

    /* Convert the fd to a pathname */
    if (linux_fd_to_path(path, sizeof(path), kn->kev.ident) < 0)
        return (-1);

    /* Convert the fflags to the inotify mask */
    mask = IN_CLOSE;
    if (kn->kev.fflags & NOTE_DELETE)
        mask |= IN_ATTRIB | IN_DELETE_SELF;
    if (kn->kev.fflags & NOTE_WRITE)
        mask |= IN_MODIFY | IN_ATTRIB;
    if (kn->kev.fflags & NOTE_EXTEND)
        mask |= IN_MODIFY | IN_ATTRIB;
    if ((kn->kev.fflags & NOTE_ATTRIB) ||
            (kn->kev.fflags & NOTE_LINK))
        mask |= IN_ATTRIB;
    if (kn->kev.fflags & NOTE_RENAME)
        mask |= IN_MOVE_SELF;
    if (kn->kev.flags & EV_ONESHOT)
        mask |= IN_ONESHOT;

    /* Create an inotify descriptor */
    ifd = inotify_init();
    if (ifd < 0) {
        if ((errno == EMFILE) || (errno == ENFILE)) {
            dbg_perror("inotify_init(2) fd_used=%u fd_max=%u", get_fd_used(), get_fd_limit());
        } else {
            dbg_perror("inotify_init(2)");
        }
        return (-1);
    }

    /* Add the watch */
    dbg_printf("inotify_add_watch(2); inofd=%d flags=%s path=%s",
            ifd, inotify_mask_dump(mask), path);
    kn->kev.data = inotify_add_watch(ifd, path, mask);
    if (kn->kev.data < 0) {
        dbg_perror("inotify_add_watch(2)");
        goto errout;
    }

    /* Add the inotify fd to the epoll set */
    KN_UDATA(kn);   /* populate this knote's kn_udata field */
    if (epoll_ctl(filter_epoll_fd(filt), EPOLL_CTL_ADD, ifd, EPOLL_EV_KN(EPOLLIN, kn)) < 0) {
        dbg_perror("epoll_ctl(2)");
        goto errout;
    }

    kn->kdata.kn_inotifyfd = ifd;

    return (0);

errout:
    inotify_rm_watch(ifd, kn->kev.data);
    kn->kdata.kn_inotifyfd = -1;
    (void) close(ifd);
    return (-1);
}

static int
delete_watch(struct filter *filt, struct knote *kn)
{
    int ifd = kn->kdata.kn_inotifyfd;

    if (ifd < 0)
        return (0);
    if (epoll_ctl(filter_epoll_fd(filt), EPOLL_CTL_DEL, ifd, NULL) < 0) {
        dbg_perror("epoll_ctl(2)");
        return (-1);
    }
    (void) close(ifd);
    kn->kdata.kn_inotifyfd = -1;

    return (0);
}

int
evfilt_vnode_copyout(struct kevent *dst, struct knote *src, void *ptr UNUSED)
{
    uint8_t buf[sizeof(struct inotify_event) + NAME_MAX + 1] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    struct inotify_event *evt;
    struct stat sb;

    evt = (struct inotify_event *)buf;
    if (get_one_event(evt, sizeof(buf), src->kdata.kn_inotifyfd) < 0)
        return (-1);

    dbg_printf("inotify event: %s", inotify_event_dump(evt));
    if (evt->mask & IN_IGNORED) {
        /* TODO: possibly return error when fs is unmounted */
        dst->filter = 0;
        return (0);
    }

    /* Check if the watched file has been closed, and
       XXX-this may not exactly match the kevent() behavior if multiple file de
scriptors reference the same file.
    */
    if (evt->mask & IN_CLOSE_WRITE || evt->mask & IN_CLOSE_NOWRITE) {
        src->kev.flags |= EV_ONESHOT; /* KLUDGE: causes the knote to be deleted */
        dst->filter = 0; /* KLUDGE: causes the event to be discarded */
        return (0);
    }

    memcpy(dst, &src->kev, sizeof(*dst));
    dst->data = 0;
    dst->fflags = 0;

    /* No error checking because fstat(2) should rarely fail */
    //FIXME: EINTR
    if (fstat(src->kev.ident, &sb) < 0 && errno == ENOENT) {
        if (src->kev.fflags & NOTE_DELETE)
            dst->fflags |= NOTE_DELETE;
    } else {
        if ((evt->mask & IN_ATTRIB || evt->mask & IN_MODIFY)) {
            if (sb.st_nlink == 0 && src->kev.fflags & NOTE_DELETE)
                dst->fflags |= NOTE_DELETE;
            if (sb.st_nlink != src->data.vnode.nlink &&
                src->kev.fflags & NOTE_LINK)
                dst->fflags |= NOTE_LINK;
#if HAVE_NOTE_TRUNCATE
            if (sb.st_nsize == 0 && src->kev.fflags & NOTE_TRUNCATE)
                dst->fflags |= NOTE_TRUNCATE;
#endif
            if (sb.st_size > src->data.vnode.size &&
                src->kev.fflags & NOTE_WRITE)
                dst->fflags |= NOTE_EXTEND;
            src->data.vnode.nlink = sb.st_nlink;
            src->data.vnode.size = sb.st_size;
        }
    }


    if (evt->mask & IN_MODIFY && src->kev.fflags & NOTE_WRITE)
        dst->fflags |= NOTE_WRITE;
    if (evt->mask & IN_ATTRIB && src->kev.fflags & NOTE_ATTRIB)
        dst->fflags |= NOTE_ATTRIB;
    if (evt->mask & IN_MOVE_SELF && src->kev.fflags & NOTE_RENAME)
        dst->fflags |= NOTE_RENAME;
    if (evt->mask & IN_DELETE_SELF && src->kev.fflags & NOTE_DELETE)
        dst->fflags |= NOTE_DELETE;

    return (0);
}

int
evfilt_vnode_knote_create(struct filter *filt, struct knote *kn)
{
    struct stat sb;

    if (fstat(kn->kev.ident, &sb) < 0) {
        dbg_puts("fstat failed");
        return (-1);
    }
    kn->data.vnode.nlink = sb.st_nlink;
    kn->data.vnode.size = sb.st_size;
    kn->kev.data = -1;

    return (add_watch(filt, kn));
}

int
evfilt_vnode_knote_modify(struct filter *filt, struct knote *kn,
        const struct kevent *kev)
{
    (void)filt;
    (void)kn;
    (void)kev;
    return (-1); /* FIXME - STUB */
}

int
evfilt_vnode_knote_delete(struct filter *filt, struct knote *kn)
{
    return delete_watch(filt, kn);
}

int
evfilt_vnode_knote_enable(struct filter *filt, struct knote *kn)
{
    return add_watch(filt, kn);
}

int
evfilt_vnode_knote_disable(struct filter *filt, struct knote *kn)
{
    return delete_watch(filt, kn);
}

const struct filter evfilt_vnode = {
    .kf_id      = EVFILT_VNODE,
    .kf_copyout = evfilt_vnode_copyout,
    .kn_create  = evfilt_vnode_knote_create,
    .kn_modify  = evfilt_vnode_knote_modify,
    .kn_delete  = evfilt_vnode_knote_delete,
    .kn_enable  = evfilt_vnode_knote_enable,
    .kn_disable = evfilt_vnode_knote_disable,
};
