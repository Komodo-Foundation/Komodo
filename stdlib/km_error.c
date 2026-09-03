/*
 * km_error — runtime support for Komodo error handling.
 *
 *   throw <msg>      lowers to a call to km_throw(msg)
 *   f(args)?         wraps the call site in _setjmp against a jmp_buf
 *                    reserved on the caller's stack, registered here
 *
 * km_throw _longjmps to the innermost registered landing pad (removing
 * it from the stack as it goes).  With no pad registered a thrown error
 * is fatal: the message is printed and the process exits non-zero.
 *
 * Single-threaded — the pad stack is a plain global, matching the rest
 * of the Komodo runtime.
 */
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

#define KM_MAX_PADS 256

static jmp_buf    *km_pads[KM_MAX_PADS];
static int         km_pad_n = 0;
static const char *km_err_msg = NULL;

void km_try_push(jmp_buf *b)
{
    if (km_pad_n < KM_MAX_PADS)
        km_pads[km_pad_n++] = b;
}

void km_try_pop(void)
{
    if (km_pad_n > 0)
        km_pad_n--;
}

const char *km_error_message(void)
{
    return km_err_msg;
}

void km_throw(const char *msg)
{
    km_err_msg = msg;
    if (km_pad_n > 0)
        _longjmp(*km_pads[--km_pad_n], 1);
    fprintf(stderr, "Uncaught error: %s\n", msg ? msg : "");
    exit(1);
}
