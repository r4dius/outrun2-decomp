/* Linux x86-64 diagnostic only: original x86 arithmetic in CPU compatibility mode.
 * Not a Switch runtime, an emulator, or a general Windows EXE launcher. */
.text
.code64
.global run_original32
.type run_original32,@function
run_original32:
    push %rbx
    push %rbp
    push %r12
    push %r13
    push %r14
    push %r15
    sub $520,%rsp
    fxsave64 (%rsp)
    mov %rsp,%r12
    fninit
    fldcw guest_call+64(%rip)
    ldmxcsr guest_call+60(%rip)
    mov guest_call+4(%rip),%esp
    pushq $0x23
    pushq $.Lcompat
    lretq
.code32
.Lcompat:
    mov $0x2b,%ax
    mov %ax,%ds
    mov %ax,%es
    mov guest_call+8,%eax
    mov guest_call+12,%ecx
    mov guest_call+16,%edx
    mov guest_call+20,%ebx
    mov guest_call+24,%esi
    mov guest_call+28,%edi
    mov guest_call+32,%ebp
    movss guest_call+36,%xmm0
    call *guest_call
    mov %ecx,guest_call+68  /* nonintrusive post-return address diagnostic */
    mov %eax,guest_call+40
    movss %xmm0,guest_call+44
    mov %esp,guest_call+52
    cmpl $0,guest_call+56
    je 1f
    fld %st(0)
    fstpt guest_call+72  /* original ST0 before the existing float spill */
    fstps guest_call+48
1:  pushl $0x33
    pushl $.Lnative
    lret
.code64
.Lnative:
    mov %r12,%rsp
    fxrstor64 (%rsp)
    add $520,%rsp
    pop %r15
    pop %r14
    pop %r13
    pop %r12
    pop %rbp
    pop %rbx
    ret
.size run_original32,.-run_original32
.section .note.GNU-stack,"",@progbits

.section .text
/* Controlled lower-course-query stub for differential testing of PC
 * GetYPositionSplChk. It is installed only for that oracle mode. */
.code32
.global course_query_stub32
.type course_query_stub32,@function
course_query_stub32:
    push %ebx
    push %esi
    mov course_stub_index,%eax
    cmp $4,%eax
    jae .Lcourse_overflow
    mov 16(%esp),%edx              /* point: ret + 2 saved + arg1(mask) => arg2 */
    mov (%edx),%ecx
    mov %ecx,course_stub_seen_x(,%eax,4)
    mov 4(%edx),%ecx
    mov %ecx,course_stub_seen_y(,%eax,4)
    mov 8(%edx),%ecx
    mov %ecx,course_stub_seen_z(,%eax,4)
    mov course_stub_y(,%eax,4),%ecx
    mov %ecx,4(%edx)
    mov 20(%esp),%edx              /* arg3: collision index */
    test %edx,%edx
    jz 1f
    mov course_stub_collision(,%eax,4),%ecx
    mov %ecx,(%edx)
1:  mov 28(%esp),%edx              /* arg5: flags */
    test %edx,%edx
    jz 2f
    mov course_stub_flags(,%eax,4),%ecx
    mov %ecx,(%edx)
2:  mov course_stub_ret(,%eax,4),%ecx
    inc %eax
    mov %eax,course_stub_index
    mov %ecx,%eax
    pop %esi
    pop %ebx
    ret
.Lcourse_overflow:
    movl $1,course_stub_overflow
    xor %eax,%eax
    pop %esi
    pop %ebx
    ret
.size course_query_stub32,.-course_query_stub32
.code64

.data
.align 16
.global course_stub_index,course_stub_overflow,course_stub_y,course_stub_collision,course_stub_flags,course_stub_ret
.global course_stub_seen_x,course_stub_seen_y,course_stub_seen_z
course_stub_index: .long 0
course_stub_overflow: .long 0
.align 16
course_stub_y: .long 0,0,0,0
.align 16
course_stub_collision: .long 0,0,0,0
.align 16
course_stub_flags: .long 0,0,0,0
.align 16
course_stub_ret: .long 0,0,0,0
.align 16
course_stub_seen_x: .long 0,0,0,0
.align 16
course_stub_seen_y: .long 0,0,0,0
.align 16
course_stub_seen_z: .long 0,0,0,0

/* r051 PasPlCar_Ctrl direct-child stubs.  These are deterministic diagnostic
 * boundaries only; they let the original parent execute its own branches and
 * writes while known child services return scripted values. */
.text
.code32
.global pas_transform_stub32
.type pas_transform_stub32,@function
pas_transform_stub32:
    mov pas_stub_index,%eax
    cmpl $4,%eax
    jae .Lpas_bad
    mov 4(%esp),%ecx                 /* dst */
    mov 8(%esp),%edx                 /* src */
    mov (%edx),%edx
    mov %edx,pas_seen_transform_x(,%eax,4)
    mov 8(%esp),%edx
    mov 4(%edx),%edx
    mov %edx,pas_seen_transform_y(,%eax,4)
    mov 8(%esp),%edx
    mov 8(%edx),%edx
    mov %edx,pas_seen_transform_z(,%eax,4)
    lea (%eax,%eax,2),%edx
    mov pas_transform_xyz(,%edx,4),%eax
    mov %eax,(%ecx)
    mov pas_transform_xyz+4(,%edx,4),%eax
    mov %eax,4(%ecx)
    mov pas_transform_xyz+8(,%edx,4),%eax
    mov %eax,8(%ecx)
    ret
.size pas_transform_stub32,.-pas_transform_stub32

.global pas_road_stub32
.type pas_road_stub32,@function
pas_road_stub32:
    mov pas_stub_index,%eax
    cmpl $4,%eax
    jae .Lpas_bad
    mov 8(%esp),%ecx                 /* point */
    mov (%ecx),%edx
    mov %edx,pas_seen_road_x(,%eax,4)
    mov 4(%ecx),%edx
    mov %edx,pas_seen_road_y(,%eax,4)
    mov 8(%ecx),%edx
    mov %edx,pas_seen_road_z(,%eax,4)
    mov pas_road_y(,%eax,4),%edx
    mov %edx,4(%ecx)
    mov 12(%esp),%ecx                /* polygon output */
    mov pas_road_polygon(,%eax,4),%edx
    mov %edx,(%ecx)
    mov 20(%esp),%ecx                /* result output */
    mov pas_road_ret(,%eax,4),%edx
    mov %edx,(%ecx)
    incl %eax
    mov %eax,pas_stub_index
    mov %edx,%eax
    ret
.size pas_road_stub32,.-pas_road_stub32

.global pas_tire_stub32
.type pas_tire_stub32,@function
pas_tire_stub32:
    mov 8(%esp),%eax                 /* wheel index */
    cmpl $4,%eax
    jae .Lpas_bad
    mov %eax,pas_seen_tire_index(,%eax,4)
    lea (%eax,%eax,2),%eax
    lea pas_tire_xyz(,%eax,4),%eax
    ret
.size pas_tire_stub32,.-pas_tire_stub32

.global pas_normal_stub32
.type pas_normal_stub32,@function
pas_normal_stub32:
    mov 12(%esp),%edx                /* record + 0x70 */
    mov %edx,%eax
    sub $0x39000070,%eax
    shr $8,%eax
    cmpl $4,%eax
    jae .Lpas_bad
    mov 4(%esp),%ecx
    mov %ecx,pas_seen_normal_polygon(,%eax,4)
    mov 8(%esp),%ecx
    mov %ecx,pas_seen_normal_type(,%eax,4)
    lea (%eax,%eax,2),%ecx
    mov pas_normal_xyz(,%ecx,4),%eax
    mov %eax,(%edx)
    mov pas_normal_xyz+4(,%ecx,4),%eax
    mov %eax,4(%edx)
    mov pas_normal_xyz+8(,%ecx,4),%eax
    mov %eax,8(%edx)
    ret
.size pas_normal_stub32,.-pas_normal_stub32

.Lpas_bad:
    movl $1,pas_stub_overflow
    xor %eax,%eax
    ret
.code64

.data
.align 16
.global pas_stub_index,pas_stub_overflow
.global pas_transform_xyz,pas_road_y,pas_road_polygon,pas_road_ret,pas_tire_xyz,pas_normal_xyz
.global pas_seen_transform_x,pas_seen_transform_y,pas_seen_transform_z
.global pas_seen_road_x,pas_seen_road_y,pas_seen_road_z
.global pas_seen_tire_index,pas_seen_normal_polygon,pas_seen_normal_type
pas_stub_index: .long 0
pas_stub_overflow: .long 0
.align 16
pas_transform_xyz: .zero 48
pas_road_y: .zero 16
pas_road_polygon: .zero 16
pas_road_ret: .zero 16
pas_tire_xyz: .zero 48
pas_normal_xyz: .zero 48
pas_seen_transform_x: .zero 16
pas_seen_transform_y: .zero 16
pas_seen_transform_z: .zero 16
pas_seen_road_x: .zero 16
pas_seen_road_y: .zero 16
pas_seen_road_z: .zero 16
pas_seen_tire_index: .zero 16
pas_seen_normal_polygon: .zero 16
pas_seen_normal_type: .zero 16
