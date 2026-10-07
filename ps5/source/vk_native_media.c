// PacBrew media libraries use BSD entry points absent from the RADV SDK's
// import surface. Implement them through the available socket/locale APIs.
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stddef.h>
#include <sys/socket.h>
#include <time.h>
#include <ps5platform/libc.h>
#include <ps5platform/videoout.h>
#include <stdio.h>
// Bounded native presentation diagnostics: the WSI flip runs on its own
// thread, so a successful QueuePresent does not expose VideoOut's result.
extern int __real_sceVideoOutSubmitFlip(int32_t,int32_t,uint32_t,int64_t);
int __wrap_sceVideoOutSubmitFlip(int32_t handle,int32_t buffer,uint32_t mode,int64_t argument){
    const int result=__real_sceVideoOutSubmitFlip(handle,buffer,mode,argument);
    static unsigned reports;
    if(reports<4){++reports;uint64_t status[PS5_VIDEO_OUT_FLIP_STATUS_WORDS]={0};
        const int status_rc=sceVideoOutGetFlipStatus(handle,status);
        fprintf(stdout,"[vk-flip] handle=%d buffer=%d serial=%lld rc=%08x status=%08x shown=%llu\n",
                handle,buffer,(long long)argument,(unsigned)result,(unsigned)status_rc,
                (unsigned long long)status[PS5_VIDEO_OUT_FLIP_STATUS_SHOWN_ARGUMENT]);}
    return result;
}
// A setjmp wrapper must not add a stack frame: its saved return address belongs
// to the caller. Tail trampolines also give the title converter real imports.
__asm__(".text\n"
        ".globl or2_ps5_setjmp\n.type or2_ps5_setjmp,@function\n"
        "or2_ps5_setjmp:\n jmp setjmp@PLT\n.size or2_ps5_setjmp,.-or2_ps5_setjmp\n"
        ".globl or2_ps5_longjmp\n.type or2_ps5_longjmp,@function\n"
        "or2_ps5_longjmp:\n jmp longjmp@PLT\n.size or2_ps5_longjmp,.-or2_ps5_longjmp\n");
int or2_ps5_mb_cur_max(void){return ps5____mb_cur_max_l(NULL);}
ssize_t or2_ps5_sendmmsg(int fd,struct mmsghdr* messages,size_t count,int flags){
    size_t i;
    for(i=0;i<count;++i){const ssize_t sent=sendmsg(fd,&messages[i].msg_hdr,flags);
        if(sent<0)return i?(ssize_t)i:-1;messages[i].msg_len=(unsigned)sent;}
    return (ssize_t)i;
}
ssize_t or2_ps5_recvmmsg(int fd,struct mmsghdr* messages,size_t count,int flags,const struct timespec* timeout){
    struct timespec start={0,0};
    if(timeout&&(timeout->tv_sec<0||timeout->tv_nsec<0||timeout->tv_nsec>=1000000000)){errno=EINVAL;return -1;}
    if(timeout&&clock_gettime(CLOCK_MONOTONIC,&start)<0)return -1;
    size_t i;
    for(i=0;i<count;++i){
        if(timeout&&!(flags&MSG_DONTWAIT)){
            struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now)<0)return i?(ssize_t)i:-1;
            double remaining=(double)timeout->tv_sec*1000+timeout->tv_nsec/1000000.0-
                ((double)(now.tv_sec-start.tv_sec)*1000+(now.tv_nsec-start.tv_nsec)/1000000.0);
            if(remaining<0)remaining=0;const int milliseconds=remaining>INT_MAX?INT_MAX:(int)(remaining+0.999999);
            struct pollfd p={fd,POLLIN,0};const int ready=poll(&p,1,milliseconds);
            if(ready<0)return i?(ssize_t)i:-1;if(!ready)return (ssize_t)i;
        }
        const ssize_t received=recvmsg(fd,&messages[i].msg_hdr,flags&~MSG_WAITFORONE);
        if(received<0)return i?(ssize_t)i:-1;messages[i].msg_len=(unsigned)received;
        if(flags&MSG_WAITFORONE)flags|=MSG_DONTWAIT;
    }
    return (ssize_t)i;
}
