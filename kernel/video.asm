	.file	"video.c"
	.text
	.section	.rodata
.LC0:
	.string	"video/video.c"
.LC1:
	.string	"s != NULL"
.LC2:
	.string	"s->bpp == 32"
	.text
	.align 16
	.globl	initVideo
	.type	initVideo, @function
initVideo:
.LFB2:
	pushq	%rbp
.LCFI0:
	movq	%rsp, %rbp
.LCFI1:
	pushq	%r12
.LCFI2:
	movq	%rdi, %r12
	subq	$8, %rsp
	testq	%rdi, %rdi
	je	.L6
.L2:
	cmpb	$32, 12(%r12)
	je	.L3
	movl	$22, %edx
	movl	$.LC0, %esi
	movl	$.LC2, %edi
	call	__assert
.L3:
	movq	%r12, %rsi
	movq	-8(%rbp), %r12
	movl	$24, %edx
	movl	$screen, %edi
	leave
.LCFI3:
	jmp	memcpy
	.align 16
.L6:
.LCFI4:
	movl	$21, %edx
	movl	$.LC0, %esi
	movl	$.LC1, %edi
	call	__assert
	jmp	.L2
.LFE2:
	.size	initVideo, .-initVideo
	.align 16
	.globl	imageLower_blit
	.type	imageLower_blit, @function
imageLower_blit:
.LFB3:
	pushq	%rbp
.LCFI5:
	movzwl	%r8w, %r8d
	movzwl	%dx, %edx
	salq	$2, %rcx
	andl	$262140, %ecx
	movq	%rsp, %rbp
.LCFI6:
	pushq	%r15
	pushq	%r14
	pushq	%r13
.LCFI7:
	leaq	0(,%r9,4), %r13
	pushq	%r12
	andl	$262140, %r13d
	pushq	%rbx
.LCFI8:
	leaq	0(,%rsi,4), %rbx
	movq	%rbx, %rsi
	andl	$262140, %esi
	subq	$24, %rsp
	movl	screen+8(%rip), %eax
	movl	8(%rdi), %r14d
	imull	%eax, %r8d
	movq	%rax, -56(%rbp)
	movzwl	16(%rbp), %eax
	imull	%r14d, %edx
	leaq	1(%rax), %r12
	addq	%r8, %rcx
	addq	screen+16(%rip), %rcx
	leaq	(%rdx,%rsi), %rbx
	movq	%rcx, %r15
	addq	16(%rdi), %rbx
	.align 16
.L8:
	movq	%rbx, %rsi
	movq	%r15, %rdi
	movq	%r13, %rdx
	addq	%r14, %rbx
	call	memcpy
	addq	-56(%rbp), %r15
	subq	$1, %r12
	jne	.L8
	addq	$24, %rsp
	popq	%rbx
.LCFI9:
	popq	%r12
.LCFI10:
	popq	%r13
.LCFI11:
	popq	%r14
.LCFI12:
	popq	%r15
.LCFI13:
	popq	%rbp
.LCFI14:
	ret
.LFE3:
	.size	imageLower_blit, .-imageLower_blit
	.section	.rodata
.LC3:
	.string	"img->bpp == 32"
.LC4:
	.string	"(size_t)dst_ptr % 8 == 0"
.LC5:
	.string	"(size_t)src_ptr % 8 == 0"
.LC6:
	.string	"(size_t)copy_size % 8 == 0"
.LC7:
	.string	"(size_t)dst_skip % 8 == 0"
.LC8:
	.string	"(size_t)src_skip % 8 == 0"
	.text
	.align 16
	.globl	imageLower_blitBinaryMask
	.type	imageLower_blitBinaryMask, @function
imageLower_blitBinaryMask:
.LFB4:
	pushq	%rbp
.LCFI15:
	movq	%rsp, %rbp
.LCFI16:
	pushq	%r15
.LCFI17:
	movq	%rdi, %r15
	pushq	%r14
	pushq	%r13
.LCFI18:
	movl	%esi, %r13d
	pushq	%r12
.LCFI19:
	movl	%edx, %r12d
	pushq	%rbx
.LCFI20:
	movl	%r8d, %ebx
	subq	$40, %rsp
	movl	16(%rbp), %eax
	cmpb	$32, 12(%rdi)
	movl	24(%rbp), %r14d
	movl	32(%rbp), %r10d
	movl	%eax, -52(%rbp)
	je	.L12
	movl	$44, %edx
	movl	$.LC0, %esi
	movl	$.LC3, %edi
	movl	%r9d, -72(%rbp)
	movl	%ecx, -64(%rbp)
	call	__assert
	movl	32(%rbp), %r10d
	movl	-72(%rbp), %r9d
	movl	-64(%rbp), %ecx
.L12:
	movl	screen+8(%rip), %esi
	movzwl	%bx, %ebx
	movzwl	%r12w, %r12d
	movzwl	%r9w, %r8d
	movl	%ebx, %edx
	leaq	0(,%rcx,4), %rbx
	sall	$2, %r8d
	imull	%esi, %edx
	movq	%rbx, %rax
	andl	$262140, %eax
	leaq	(%rdx,%rax), %rbx
	movl	8(%r15), %edx
	movl	%r12d, %eax
	leaq	0(,%r13,4), %r12
	movq	%r12, %r13
	addq	screen+16(%rip), %rbx
	imull	%edx, %eax
	andl	$262140, %r13d
	leaq	(%rax,%r13), %r12
	movslq	%r8d, %rax
	addq	16(%r15), %r12
	movl	%esi, %r13d
	movl	%edx, %r15d
	subq	%rax, %r13
	subq	%rax, %r15
	testb	$7, %bl
	jne	.L44
	testb	$7, %r12b
	jne	.L45
.L14:
	andl	$4, %r8d
	jne	.L46
.L15:
	testb	$7, %r13b
	jne	.L47
.L16:
	testb	$7, %r15b
	jne	.L48
.L17:
	movzwl	-52(%rbp), %edx
	andq	$-4, %r15
	andq	$-4, %r13
	shrq	$2, %rax
	leaq	1(%rdx), %rsi
	movq	%rax, %rdx
	je	.L11
	leaq	0(,%rax,4), %rax
	leaq	(%rax,%r15), %r8
	leaq	(%rax,%r13), %rdi
	.align 16
.L23:
	xorl	%eax, %eax
	.align 16
.L22:
	cmpb	$0, (%r12,%rax,4)
	jne	.L19
	movl	%r14d, (%rbx,%rax,4)
	addq	$1, %rax
	cmpq	%rax, %rdx
	jne	.L22
	addq	%r8, %r12
	addq	%rdi, %rbx
	subq	$1, %rsi
	jne	.L23
.L11:
	addq	$40, %rsp
	popq	%rbx
.LCFI21:
	popq	%r12
.LCFI22:
	popq	%r13
.LCFI23:
	popq	%r14
.LCFI24:
	popq	%r15
.LCFI25:
	popq	%rbp
.LCFI26:
	ret
	.align 16
.L19:
.LCFI27:
	movl	%r10d, (%rbx,%rax,4)
	addq	$1, %rax
	cmpq	%rdx, %rax
	jne	.L22
	addq	%r8, %r12
	addq	%rdi, %rbx
	subq	$1, %rsi
	jne	.L23
	jmp	.L11
.L44:
	movl	$63, %edx
	movl	$.LC0, %esi
	movl	$.LC4, %edi
	movl	%r10d, 32(%rbp)
	movq	%rax, -72(%rbp)
	movl	%r8d, -64(%rbp)
	call	__assert
	movl	32(%rbp), %r10d
	movq	-72(%rbp), %rax
	movl	-64(%rbp), %r8d
	testb	$7, %r12b
	je	.L14
.L45:
	movl	$64, %edx
	movl	$.LC0, %esi
	movl	$.LC5, %edi
	movl	%r10d, 32(%rbp)
	movq	%rax, -72(%rbp)
	movl	%r8d, -64(%rbp)
	call	__assert
	movl	-64(%rbp), %r8d
	movl	32(%rbp), %r10d
	movq	-72(%rbp), %rax
	andl	$4, %r8d
	je	.L15
.L46:
	movl	$66, %edx
	movl	$.LC0, %esi
	movl	$.LC6, %edi
	movl	%r10d, 32(%rbp)
	movq	%rax, -64(%rbp)
	call	__assert
	movl	32(%rbp), %r10d
	movq	-64(%rbp), %rax
	testb	$7, %r13b
	je	.L16
.L47:
	movl	$67, %edx
	movl	$.LC0, %esi
	movl	$.LC7, %edi
	movl	%r10d, 32(%rbp)
	movq	%rax, -64(%rbp)
	call	__assert
	movl	32(%rbp), %r10d
	movq	-64(%rbp), %rax
	testb	$7, %r15b
	je	.L17
.L48:
	movl	$68, %edx
	movl	$.LC0, %esi
	movl	$.LC8, %edi
	movl	%r10d, 32(%rbp)
	movq	%rax, -64(%rbp)
	call	__assert
	movl	32(%rbp), %r10d
	movq	-64(%rbp), %rax
	jmp	.L17
.LFE4:
	.size	imageLower_blitBinaryMask, .-imageLower_blitBinaryMask
	.align 16
	.globl	imageFillRect
	.type	imageFillRect, @function
imageFillRect:
.LFB6:
	pushq	%rbp
.LCFI28:
	movl	%edi, %r9d
	movq	%rsp, %rbp
.LCFI29:
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	subq	$8, %rsp
.LCFI30:
	testq	%rsi, %rsi
	je	.L50
	movswl	(%rsi), %eax
	movzwl	4(%rsi), %r14d
	movzwl	6(%rsi), %r13d
	movswl	2(%rsi), %r8d
	addl	%eax, %r14d
	addl	%r8d, %r13d
.L51:
	cmpq	%r8, %r13
	jbe	.L49
	subl	%eax, %r14d
	leal	0(,%rax,4), %r15d
	movl	%r9d, %esi
	movq	screen+16(%rip), %rbx
	movl	%r14d, %r11d
	leaq	1(%r11), %rax
	movq	%rax, -48(%rbp)
	movq	%r9, %rax
	salq	$32, %rax
	orq	%rax, %rsi
	.align 16
.L57:
	movl	screen+8(%rip), %eax
	imulq	%r8, %rax
	addq	%r15, %rax
	leaq	(%rbx,%rax), %rdx
	movq	%rdx, %rcx
	shrq	$2, %rcx
	andl	$1, %ecx
	cmpl	$3, %r14d
	jbe	.L59
	testq	%rcx, %rcx
	je	.L60
	movl	%r9d, (%rdx)
	leaq	4(%rdx), %r12
	movq	%rcx, %rdi
.L54:
	movq	-48(%rbp), %r10
	subq	%rcx, %r10
	leaq	(%rax,%rcx,4), %rcx
	xorl	%eax, %eax
	leaq	-2(%r10), %rdx
	addq	%rbx, %rcx
	shrq	%rdx
	addq	$1, %rdx
	.align 16
.L55:
	movq	%rsi, (%rcx,%rax,8)
	addq	$1, %rax
	cmpq	%rax, %rdx
	ja	.L55
	leaq	(%rdx,%rdx), %rax
	leaq	(%r12,%rdx,8), %rdx
	addq	%rax, %rdi
	cmpq	%rax, %r10
	je	.L56
.L53:
	leaq	1(%rdi), %rax
	movl	%r9d, (%rdx)
	cmpq	%rax, %r11
	jb	.L56
	leaq	2(%rdi), %rax
	movl	%r9d, 4(%rdx)
	cmpq	%rax, %r11
	jb	.L56
	addq	$3, %rdi
	movl	%r9d, 8(%rdx)
	cmpq	%rdi, %r11
	jb	.L56
	movl	%r9d, 12(%rdx)
.L56:
	addq	$1, %r8
	cmpq	%r13, %r8
	jne	.L57
.L49:
	addq	$8, %rsp
	popq	%rbx
.LCFI31:
	popq	%r12
.LCFI32:
	popq	%r13
.LCFI33:
	popq	%r14
.LCFI34:
	popq	%r15
.LCFI35:
	popq	%rbp
.LCFI36:
	ret
	.align 16
.L60:
.LCFI37:
	xorl	%edi, %edi
	movq	%rdx, %r12
	jmp	.L54
	.align 16
.L59:
	xorl	%edi, %edi
	jmp	.L53
.L50:
	movl	screen(%rip), %r14d
	movl	screen+4(%rip), %r13d
	xorl	%r8d, %r8d
	xorl	%eax, %eax
	jmp	.L51
.LFE6:
	.size	imageFillRect, .-imageFillRect
	.align 16
	.globl	imageBlit
	.type	imageBlit, @function
imageBlit:
.LFB8:
	pushq	%rbp
.LCFI38:
	movq	%rsp, %rbp
.LCFI39:
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	subq	$24, %rsp
.LCFI40:
	testq	%rdx, %rdx
	je	.L74
	xorl	%eax, %eax
	cmpw	$0, (%rdx)
	movl	%eax, %r10d
	cmovns	(%rdx), %r10w
	cmpw	$0, 2(%rdx)
	cmovns	2(%rdx), %ax
	movswl	%r10w, %r10d
	movswl	%ax, %edx
.L65:
	testq	%rcx, %rcx
	je	.L75
	movswl	(%rcx), %r8d
	movzwl	4(%rcx), %r13d
	movzwl	6(%rcx), %eax
	movzwl	2(%rcx), %r12d
	testw	%r8w, %r8w
	js	.L67
	leal	0(,%r8,4), %ebx
	movl	%r10d, %r9d
	movslq	%ebx, %r11
.L68:
	movswl	%r12w, %ebx
	movl	%edx, %ecx
	testw	%r12w, %r12w
	jns	.L66
	addl	%ebx, %edx
	subl	%ebx, %eax
	xorl	%ebx, %ebx
	movzwl	%dx, %ecx
.L66:
	movl	(%rdi), %r14d
	leal	(%r10,%r13), %r15d
	movl	%r14d, %r12d
	subl	%r10d, %r12d
	cmpl	%r14d, %r15d
	movl	4(%rdi), %r14d
	cmova	%r12d, %r13d
	movl	%r14d, %r10d
	leal	(%rdx,%r13), %r15d
	subl	%edx, %r10d
	cmpl	%r14d, %r15d
	movl	8(%rsi), %r15d
	cmova	%r10d, %eax
	addl	%r13d, %r8d
	cmpl	(%rsi), %r8d
	movl	8(%rdi), %r8d
	cmova	%r12d, %r13d
	leal	(%rbx,%rax), %edx
	cmpl	4(%rsi), %edx
	cmova	%r10d, %eax
	imull	%r8d, %ecx
	leal	0(,%r9,4), %edx
	salq	$2, %r13
	imull	%r15d, %ebx
	movslq	%edx, %rdx
	andl	$262140, %r13d
	movzwl	%ax, %eax
	addq	%rdx, %rcx
	addq	16(%rdi), %rcx
	leaq	1(%rax), %r12
	movl	%r8d, %edi
	movq	%rdi, -56(%rbp)
	addq	%r11, %rbx
	movq	%rcx, %r14
	addq	16(%rsi), %rbx
	.align 16
.L73:
	movq	%r14, %rsi
	movq	%rbx, %rdi
	movq	%r13, %rdx
	addq	%r15, %rbx
	call	memcpy
	addq	-56(%rbp), %r14
	subq	$1, %r12
	jne	.L73
	addq	$24, %rsp
	popq	%rbx
.LCFI41:
	popq	%r12
.LCFI42:
	popq	%r13
.LCFI43:
	popq	%r14
.LCFI44:
	popq	%r15
.LCFI45:
	popq	%rbp
.LCFI46:
	ret
	.align 16
.L67:
.LCFI47:
	addl	%r8d, %r10d
	subl	%r8d, %r13d
	xorl	%r11d, %r11d
	xorl	%r8d, %r8d
	movzwl	%r10w, %r9d
	jmp	.L68
	.align 16
.L75:
	movl	%r10d, %r9d
	movl	%edx, %ecx
	xorl	%r11d, %r11d
	movl	$2147483647, %eax
	movl	$2147483647, %r13d
	xorl	%ebx, %ebx
	xorl	%r8d, %r8d
	jmp	.L66
	.align 16
.L74:
	xorl	%edx, %edx
	xorl	%r10d, %r10d
	jmp	.L65
.LFE8:
	.size	imageBlit, .-imageBlit
	.align 16
	.globl	imageDraw
	.type	imageDraw, @function
imageDraw:
.LFB7:
	movq	%rdx, %rcx
	movq	%rsi, %rdx
	movl	$screen, %esi
	jmp	imageBlit
.LFE7:
	.size	imageDraw, .-imageDraw
	.align 16
	.globl	alloc_image
	.type	alloc_image, @function
alloc_image:
.LFB9:
	pushq	%rbp
.LCFI48:
	movq	%rsp, %rbp
.LCFI49:
	pushq	%r15
	pushq	%r14
.LCFI50:
	movl	%edi, %r14d
	movl	$24, %edi
	pushq	%r13
.LCFI51:
	movl	%edx, %r13d
	movq	%r14, %r15
	pushq	%r12
	pushq	%rbx
.LCFI52:
	movl	%esi, %ebx
	movq	%rbx, %rax
	salq	$32, %rax
	subq	$8, %rsp
	orq	%rax, %r14
	call	kmalloc
	movl	%r13d, %edi
	shrl	$3, %edi
	movq	%rax, %r12
	movq	%r14, (%rax)
	imull	%r15d, %edi
	movb	%r13b, 12(%r12)
	movl	%edi, %eax
	addq	$16, %rax
	andq	$-16, %rax
	testb	$15, %dil
	cmovne	%eax, %edi
	movl	%edi, 8(%r12)
	imull	%ebx, %edi
	call	kmalloc
	movq	%rax, 16(%r12)
	addq	$8, %rsp
	movq	%r12, %rax
	popq	%rbx
.LCFI53:
	popq	%r12
.LCFI54:
	popq	%r13
.LCFI55:
	popq	%r14
.LCFI56:
	popq	%r15
.LCFI57:
	popq	%rbp
.LCFI58:
	ret
.LFE9:
	.size	alloc_image, .-alloc_image
	.section	.rodata
.LC9:
	.string	"im != NULL"
	.text
	.align 16
	.globl	free_image
	.type	free_image, @function
free_image:
.LFB10:
	pushq	%rbp
.LCFI59:
	movq	%rsp, %rbp
.LCFI60:
	pushq	%rbx
.LCFI61:
	movq	%rdi, %rbx
	subq	$8, %rsp
	testq	%rdi, %rdi
	je	.L88
	movq	16(%rbx), %rdi
	movq	-8(%rbp), %rbx
	leave
.LCFI62:
	jmp	kfree
	.align 16
.L88:
.LCFI63:
	movl	$.LC9, %edi
	movl	$250, %edx
	movl	$.LC0, %esi
	call	__assert
	movq	16(%rbx), %rdi
	movq	-8(%rbp), %rbx
	leave
.LCFI64:
	jmp	kfree
.LFE10:
	.size	free_image, .-free_image
	.align 16
	.globl	getScreenImage
	.type	getScreenImage, @function
getScreenImage:
.LFB12:
	movl	$screen, %eax
	ret
.LFE12:
	.size	getScreenImage, .-getScreenImage
	.section	.rodata
	.align 8
.LC10:
	.string	"header->bpp == 8 || header->bpp == 24"
.LC11:
	.string	"0"
.LC12:
	.string	"header->bpp == 8"
	.text
	.align 16
	.globl	loadBMP
	.type	loadBMP, @function
loadBMP:
.LFB15:
	pushq	%rbp
.LCFI65:
	movq	%rsp, %rbp
.LCFI66:
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
.LCFI67:
	movq	%rdi, %r12
	pushq	%rbx
	subq	$24, %rsp
.LCFI68:
	testq	%rdi, %rdi
	je	.L91
	cmpw	$19778, (%rdi)
	je	.L91
	movl	18(%rdi), %edx
	testl	%edx, %edx
	jle	.L91
	movl	22(%rdi), %eax
	testl	%eax, %eax
	jle	.L91
	cmpw	$1, 26(%rdi)
	jne	.L91
	xorl	%r12d, %r12d
.L90:
	addq	$24, %rsp
	movq	%r12, %rax
	popq	%rbx
.LCFI69:
	popq	%r12
.LCFI70:
	popq	%r13
.LCFI71:
	popq	%r14
.LCFI72:
	popq	%r15
.LCFI73:
	popq	%rbp
.LCFI74:
	ret
	.align 16
.L91:
.LCFI75:
	movzwl	28(%r12), %eax
	movl	%eax, %edx
	andl	$-17, %edx
	cmpw	$8, %dx
	jne	.L136
.L93:
	movl	10(%r12), %esi
	leaq	(%r12,%rsi), %rbx
	movq	%rbx, -56(%rbp)
	cmpw	$8, %ax
	jne	.L137
	movl	$372, %edx
	movl	$.LC0, %esi
	movl	$.LC11, %edi
	call	__assert
	cmpw	$8, 28(%r12)
	jne	.L138
.L97:
	movl	22(%r12), %r14d
	movl	$24, %edi
	movl	18(%r12), %r13d
	call	kmalloc
	movq	%rax, %r12
	movq	%r14, %rax
	movq	%r13, %r15
	salq	$32, %rax
	movb	$8, 12(%r12)
	orq	%r13, %rax
	movq	%rax, (%r12)
	leaq	16(%r13), %rax
	andq	$-16, %rax
	testb	$15, %r13b
	cmovne	%eax, %r15d
	movl	%r15d, %edi
	movl	%r15d, 8(%r12)
	imull	%r14d, %edi
	call	kmalloc
	movl	8(%r12), %r8d
	movq	%rax, 16(%r12)
	movq	%rax, %rcx
	testq	%r14, %r14
	je	.L90
	leal	-1(%r14), %esi
	testq	%r13, %r13
	je	.L90
	imulq	%r13, %rsi
	salq	$2, %r8
	addq	-56(%rbp), %rsi
	xorl	%edi, %edi
	.align 16
.L100:
	xorl	%eax, %eax
	.align 16
.L99:
	movzbl	(%rsi,%rax), %edx
	movl	%edx, (%rcx,%rax,4)
	addq	$1, %rax
	cmpq	%r13, %rax
	jne	.L99
	addq	$1, %rdi
	subq	%r13, %rsi
	addq	%r8, %rcx
	cmpq	%r14, %rdi
	jne	.L100
	jmp	.L90
.L137:
	cmpw	$24, %ax
	jne	.L139
	movl	22(%r12), %r13d
	movl	$24, %edi
	movl	18(%r12), %ebx
	call	kmalloc
	movq	%rax, %r12
	movq	%r13, %rax
	leal	0(,%rbx,4), %ecx
	salq	$32, %rax
	movb	$32, 12(%r12)
	orq	%rbx, %rax
	movq	%rax, (%r12)
	movq	%rcx, %rax
	addq	$16, %rcx
	andq	$-16, %rcx
	testb	$12, %al
	cmovne	%ecx, %eax
	movl	%eax, 8(%r12)
	imull	%r13d, %eax
	movl	%eax, %edi
	call	kmalloc
	movl	8(%r12), %r9d
	movq	%rax, 16(%r12)
	movq	%rax, %r10
	shrl	$2, %r9d
	testq	%r13, %r13
	je	.L90
	testq	%rbx, %rbx
	je	.L90
	leaq	0(,%rbx,4), %rax
	movq	%rbx, %r11
	xorl	%r8d, %r8d
	xorl	%edi, %edi
	imulq	%r13, %rbx
	subq	%rax, %r11
	leaq	(%rbx,%rbx,2), %rax
	movq	-56(%rbp), %rbx
	addq	%rax, %rbx
	.align 16
.L95:
	movq	%rbx, %rsi
	addq	%r11, %rbx
	leaq	(%r10,%r8,4), %rcx
	movq	%rbx, %rax
	.align 16
.L96:
	movl	(%rax), %edx
	addq	$3, %rax
	addq	$4, %rcx
	andl	$16777215, %edx
	movl	%edx, -4(%rcx)
	cmpq	%rax, %rsi
	jne	.L96
	addq	$1, %rdi
	addq	%r9, %r8
	cmpq	%rdi, %r13
	jne	.L95
	jmp	.L90
.L138:
	movl	$325, %edx
	movl	$.LC0, %esi
	movl	$.LC12, %edi
	call	__assert
	jmp	.L97
.L136:
	movl	$363, %edx
	movl	$.LC0, %esi
	movl	$.LC10, %edi
	call	__assert
	movzwl	28(%r12), %eax
	jmp	.L93
.L139:
	movl	$376, %edx
	movl	$.LC0, %esi
	movl	$.LC11, %edi
	call	__assert
.LFE15:
	.size	loadBMP, .-loadBMP
	.local	screen
	.comm	screen,24,16
	.section	.eh_frame,"aw",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1
.LSCIE1:
	.long	0
	.byte	0x3
	.string	"zR"
	.byte	0x1
	.byte	0x78
	.byte	0x10
	.byte	0x1
	.byte	0x3
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x90
	.byte	0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1
.LASFDE1:
	.long	.LASFDE1-.Lframe1
	.long	.LFB2
	.long	.LFE2-.LFB2
	.byte	0
	.byte	0x4
	.long	.LCFI0-.LFB2
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI1-.LCFI0
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI2-.LCFI1
	.byte	0x8c
	.byte	0x3
	.byte	0x4
	.long	.LCFI3-.LCFI2
	.byte	0xa
	.byte	0xc6
	.byte	0xcc
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x4
	.long	.LCFI4-.LCFI3
	.byte	0xb
	.align 8
.LEFDE1:
.LSFDE3:
	.long	.LEFDE3-.LASFDE3
.LASFDE3:
	.long	.LASFDE3-.Lframe1
	.long	.LFB3
	.long	.LFE3-.LFB3
	.byte	0
	.byte	0x4
	.long	.LCFI5-.LFB3
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI6-.LCFI5
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI7-.LCFI6
	.byte	0x8f
	.byte	0x3
	.byte	0x8e
	.byte	0x4
	.byte	0x8d
	.byte	0x5
	.byte	0x4
	.long	.LCFI8-.LCFI7
	.byte	0x8c
	.byte	0x6
	.byte	0x83
	.byte	0x7
	.byte	0x4
	.long	.LCFI9-.LCFI8
	.byte	0xc3
	.byte	0x4
	.long	.LCFI10-.LCFI9
	.byte	0xcc
	.byte	0x4
	.long	.LCFI11-.LCFI10
	.byte	0xcd
	.byte	0x4
	.long	.LCFI12-.LCFI11
	.byte	0xce
	.byte	0x4
	.long	.LCFI13-.LCFI12
	.byte	0xcf
	.byte	0x4
	.long	.LCFI14-.LCFI13
	.byte	0xc6
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 8
.LEFDE3:
.LSFDE5:
	.long	.LEFDE5-.LASFDE5
.LASFDE5:
	.long	.LASFDE5-.Lframe1
	.long	.LFB4
	.long	.LFE4-.LFB4
	.byte	0
	.byte	0x4
	.long	.LCFI15-.LFB4
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI16-.LCFI15
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI17-.LCFI16
	.byte	0x8f
	.byte	0x3
	.byte	0x4
	.long	.LCFI18-.LCFI17
	.byte	0x8e
	.byte	0x4
	.byte	0x8d
	.byte	0x5
	.byte	0x4
	.long	.LCFI19-.LCFI18
	.byte	0x8c
	.byte	0x6
	.byte	0x4
	.long	.LCFI20-.LCFI19
	.byte	0x83
	.byte	0x7
	.byte	0x4
	.long	.LCFI21-.LCFI20
	.byte	0xa
	.byte	0xc3
	.byte	0x4
	.long	.LCFI22-.LCFI21
	.byte	0xcc
	.byte	0x4
	.long	.LCFI23-.LCFI22
	.byte	0xcd
	.byte	0x4
	.long	.LCFI24-.LCFI23
	.byte	0xce
	.byte	0x4
	.long	.LCFI25-.LCFI24
	.byte	0xcf
	.byte	0x4
	.long	.LCFI26-.LCFI25
	.byte	0xc6
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x4
	.long	.LCFI27-.LCFI26
	.byte	0xb
	.align 8
.LEFDE5:
.LSFDE7:
	.long	.LEFDE7-.LASFDE7
.LASFDE7:
	.long	.LASFDE7-.Lframe1
	.long	.LFB6
	.long	.LFE6-.LFB6
	.byte	0
	.byte	0x4
	.long	.LCFI28-.LFB6
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI29-.LCFI28
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI30-.LCFI29
	.byte	0x8f
	.byte	0x3
	.byte	0x8e
	.byte	0x4
	.byte	0x8d
	.byte	0x5
	.byte	0x8c
	.byte	0x6
	.byte	0x83
	.byte	0x7
	.byte	0x4
	.long	.LCFI31-.LCFI30
	.byte	0xa
	.byte	0xc3
	.byte	0x4
	.long	.LCFI32-.LCFI31
	.byte	0xcc
	.byte	0x4
	.long	.LCFI33-.LCFI32
	.byte	0xcd
	.byte	0x4
	.long	.LCFI34-.LCFI33
	.byte	0xce
	.byte	0x4
	.long	.LCFI35-.LCFI34
	.byte	0xcf
	.byte	0x4
	.long	.LCFI36-.LCFI35
	.byte	0xc6
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x4
	.long	.LCFI37-.LCFI36
	.byte	0xb
	.align 8
.LEFDE7:
.LSFDE9:
	.long	.LEFDE9-.LASFDE9
.LASFDE9:
	.long	.LASFDE9-.Lframe1
	.long	.LFB8
	.long	.LFE8-.LFB8
	.byte	0
	.byte	0x4
	.long	.LCFI38-.LFB8
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI39-.LCFI38
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI40-.LCFI39
	.byte	0x8f
	.byte	0x3
	.byte	0x8e
	.byte	0x4
	.byte	0x8d
	.byte	0x5
	.byte	0x8c
	.byte	0x6
	.byte	0x83
	.byte	0x7
	.byte	0x4
	.long	.LCFI41-.LCFI40
	.byte	0xa
	.byte	0xc3
	.byte	0x4
	.long	.LCFI42-.LCFI41
	.byte	0xcc
	.byte	0x4
	.long	.LCFI43-.LCFI42
	.byte	0xcd
	.byte	0x4
	.long	.LCFI44-.LCFI43
	.byte	0xce
	.byte	0x4
	.long	.LCFI45-.LCFI44
	.byte	0xcf
	.byte	0x4
	.long	.LCFI46-.LCFI45
	.byte	0xc6
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x4
	.long	.LCFI47-.LCFI46
	.byte	0xb
	.align 8
.LEFDE9:
.LSFDE11:
	.long	.LEFDE11-.LASFDE11
.LASFDE11:
	.long	.LASFDE11-.Lframe1
	.long	.LFB7
	.long	.LFE7-.LFB7
	.byte	0
	.align 8
.LEFDE11:
.LSFDE13:
	.long	.LEFDE13-.LASFDE13
.LASFDE13:
	.long	.LASFDE13-.Lframe1
	.long	.LFB9
	.long	.LFE9-.LFB9
	.byte	0
	.byte	0x4
	.long	.LCFI48-.LFB9
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI49-.LCFI48
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI50-.LCFI49
	.byte	0x8f
	.byte	0x3
	.byte	0x8e
	.byte	0x4
	.byte	0x4
	.long	.LCFI51-.LCFI50
	.byte	0x8d
	.byte	0x5
	.byte	0x4
	.long	.LCFI52-.LCFI51
	.byte	0x8c
	.byte	0x6
	.byte	0x83
	.byte	0x7
	.byte	0x4
	.long	.LCFI53-.LCFI52
	.byte	0xc3
	.byte	0x4
	.long	.LCFI54-.LCFI53
	.byte	0xcc
	.byte	0x4
	.long	.LCFI55-.LCFI54
	.byte	0xcd
	.byte	0x4
	.long	.LCFI56-.LCFI55
	.byte	0xce
	.byte	0x4
	.long	.LCFI57-.LCFI56
	.byte	0xcf
	.byte	0x4
	.long	.LCFI58-.LCFI57
	.byte	0xc6
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 8
.LEFDE13:
.LSFDE15:
	.long	.LEFDE15-.LASFDE15
.LASFDE15:
	.long	.LASFDE15-.Lframe1
	.long	.LFB10
	.long	.LFE10-.LFB10
	.byte	0
	.byte	0x4
	.long	.LCFI59-.LFB10
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI60-.LCFI59
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI61-.LCFI60
	.byte	0x83
	.byte	0x3
	.byte	0x4
	.long	.LCFI62-.LCFI61
	.byte	0xa
	.byte	0xc6
	.byte	0xc3
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x4
	.long	.LCFI63-.LCFI62
	.byte	0xb
	.byte	0x4
	.long	.LCFI64-.LCFI63
	.byte	0xc6
	.byte	0xc3
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.align 8
.LEFDE15:
.LSFDE17:
	.long	.LEFDE17-.LASFDE17
.LASFDE17:
	.long	.LASFDE17-.Lframe1
	.long	.LFB12
	.long	.LFE12-.LFB12
	.byte	0
	.align 8
.LEFDE17:
.LSFDE19:
	.long	.LEFDE19-.LASFDE19
.LASFDE19:
	.long	.LASFDE19-.Lframe1
	.long	.LFB15
	.long	.LFE15-.LFB15
	.byte	0
	.byte	0x4
	.long	.LCFI65-.LFB15
	.byte	0xe
	.byte	0x10
	.byte	0x86
	.byte	0x2
	.byte	0x4
	.long	.LCFI66-.LCFI65
	.byte	0xd
	.byte	0x6
	.byte	0x4
	.long	.LCFI67-.LCFI66
	.byte	0x8f
	.byte	0x3
	.byte	0x8e
	.byte	0x4
	.byte	0x8d
	.byte	0x5
	.byte	0x8c
	.byte	0x6
	.byte	0x4
	.long	.LCFI68-.LCFI67
	.byte	0x83
	.byte	0x7
	.byte	0x4
	.long	.LCFI69-.LCFI68
	.byte	0xa
	.byte	0xc3
	.byte	0x4
	.long	.LCFI70-.LCFI69
	.byte	0xcc
	.byte	0x4
	.long	.LCFI71-.LCFI70
	.byte	0xcd
	.byte	0x4
	.long	.LCFI72-.LCFI71
	.byte	0xce
	.byte	0x4
	.long	.LCFI73-.LCFI72
	.byte	0xcf
	.byte	0x4
	.long	.LCFI74-.LCFI73
	.byte	0xc6
	.byte	0xc
	.byte	0x7
	.byte	0x8
	.byte	0x4
	.long	.LCFI75-.LCFI74
	.byte	0xb
	.align 8
.LEFDE19:
	.ident	"GCC: (GNU) 10.1.0"
