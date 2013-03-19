/*
 * Copyright (C) 2009-2011 Citrix Ltd.
 * Author Vincent Hanquez <vincent.hanquez@eu.citrix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; version 2.1 only. with the special
 * exception on linking described in file LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 */

#include <stdlib.h>

#define CAML_NAME_SPACE
#include <caml/alloc.h>
#include <caml/memory.h>
#include <caml/signals.h>
#include <caml/fail.h>
#include <caml/callback.h>

#include <sys/mman.h>
#include <stdint.h>
#include <string.h>

#include <libxl.h>
#include <libxl_utils.h>

#define CTX ((libxl_ctx *)ctx)

static char * dup_String_val(value s)
{
	int len;
	char *c;
	len = caml_string_length(s);
	c = calloc(len + 1, sizeof(char));
	if (!c)
		caml_raise_out_of_memory();
	memcpy(c, String_val(s), len);
	return c;
}

static value Val_error(int error)
{
	switch (error) {
	case ERROR_NONSPECIFIC: return Val_int(0);
	case ERROR_VERSION:     return Val_int(1);
	case ERROR_FAIL:        return Val_int(2);
	case ERROR_NI:          return Val_int(3);
	case ERROR_NOMEM:       return Val_int(4);
	case ERROR_INVAL:       return Val_int(5);
	case ERROR_BADFAIL:     return Val_int(6);
	case ERROR_GUEST_TIMEDOUT: return Val_int(7);
	case ERROR_TIMEDOUT:    return Val_int(8);
	case ERROR_NOPARAVIRT:  return Val_int(9);
	case ERROR_NOT_READY:   return Val_int(10);
	case ERROR_OSEVENT_REG_FAIL: return Val_int(11);
	case ERROR_BUFFERFULL:  return Val_int(12);
	case ERROR_UNKNOWN_CHILD: return Val_int(13);
#if 0 /* Let the compiler catch this */
	default:
		caml_raise_sys_error(caml_copy_string("Unknown libxl ERROR"));
		break;
#endif
	}
	/* Should not reach here */
	abort();
}

static void failwith_xl(int error, char *fname)
{
	CAMLlocal1(arg);
	value *exc = caml_named_value("Xenlight.Error");

	if (!exc)
		caml_invalid_argument("Exception Xenlight.Error not initialized, please link xl.cma");

	arg = caml_alloc_small(2, 0);

	Field(arg, 0) = Val_error(error);
	Field(arg, 1) = caml_copy_string(fname);

	caml_raise_with_arg(*exc, arg);
}

CAMLprim value stub_raise_exception(value unit)
{
	CAMLparam1(unit);
	failwith_xl(ERROR_FAIL, "test exception");
	CAMLreturn(Val_unit);
}

CAMLprim value stub_libxl_ctx_alloc(value logger)
{
	CAMLparam1(logger);
	libxl_ctx *ctx;
	int ret;

	ret = libxl_ctx_alloc(&ctx, LIBXL_VERSION, 0, (struct xentoollog_logger *) logger);
	if (ret != 0) \
		failwith_xl(ERROR_FAIL, "cannot init context");
	CAMLreturn((value)ctx);
}

CAMLprim value stub_libxl_ctx_free(value ctx)
{
	CAMLparam1(ctx);
	libxl_ctx_free(CTX);
	CAMLreturn(Val_unit);
}

static int list_len(value v)
{
	int len = 0;
	while ( v != Val_emptylist ) {
		len++;
		v = Field(v, 1);
	}
	return len;
}

static int libxl_key_value_list_val(libxl_key_value_list *c_val,
				    value v)
{
	CAMLparam1(v);
	CAMLlocal1(elem);
	int nr, i;
	libxl_key_value_list array;

	nr = list_len(v);

	array = calloc((nr + 1) * 2, sizeof(char *));
	if (!array)
		caml_raise_out_of_memory();

	for (i=0; v != Val_emptylist; i++, v = Field(v, 1) ) {
		elem = Field(v, 0);

		array[i * 2] = dup_String_val(Field(elem, 0));
		array[i * 2 + 1] = dup_String_val(Field(elem, 1));
	}

	*c_val = array;
	CAMLreturn(0);
}

static int libxl_string_list_val(libxl_string_list *c_val, value v)
{
	CAMLparam1(v);
	int nr, i;
	libxl_key_value_list array;

	nr = list_len(v);

	array = calloc(nr + 1, sizeof(char *));
	if (!array)
		caml_raise_out_of_memory();

	for (i=0; v != Val_emptylist; i++, v = Field(v, 1) )
		array[i] = dup_String_val(Field(v, 0));

	*c_val = array;
	CAMLreturn(0);
}

/* Option type support as per http://www.linux-nantes.org/~fmonnier/ocaml/ocaml-wrapping-c.php */
#define Val_none Val_int(0)
#define Some_val(v) Field(v,0)

static value Val_some(value v)
{
	CAMLparam1(v);
	CAMLlocal1(some);
	some = caml_alloc(1, 0);
	Store_field(some, 0, v);
	CAMLreturn(some);
}

static value Val_mac (libxl_mac *c_val)
{
	CAMLparam0();
	CAMLlocal1(v);
	int i;

	v = caml_alloc_tuple(6);

	for(i=0; i<6; i++)
		Store_field(v, i, Val_int((*c_val)[i]));

	CAMLreturn(v);
}

static int Mac_val(libxl_mac *c_val, value v)
{
	CAMLparam1(v);
	int i;

	for(i=0; i<6; i++)
		(*c_val)[i] = Int_val(Field(v, i));

	CAMLreturn(0);
}

static value Val_bitmap (libxl_bitmap *c_val)
{
	CAMLparam0();
	CAMLlocal1(v);
	int i;

	v = caml_alloc(8 * (c_val->size), 0);
	libxl_for_each_bit(i, *c_val) {
		if (libxl_bitmap_test(c_val, i))
			Store_field(v, i, Val_true);
		else
			Store_field(v, i, Val_false);
	}
	CAMLreturn(v);
}

static int Bitmap_val(libxl_ctx *ctx, libxl_bitmap *c_val, value v)
{
	CAMLparam1(v);
	int i, len = Wosize_val(v);

	c_val->size = 0;
	if (len > 0 && !libxl_bitmap_alloc(ctx, c_val, len))
		failwith_xl(ERROR_NOMEM, "cannot allocate bitmap");
	for (i=0; i<len; i++) {
		if (Int_val(Field(v, i)))
			libxl_bitmap_set(c_val, i);
		else
			libxl_bitmap_reset(c_val, i);
	}
	CAMLreturn(0);
}

static value Val_uuid (libxl_uuid *c_val)
{
	CAMLparam0();
	CAMLlocal1(v);
	uint8_t *uuid = libxl_uuid_bytearray(c_val);
	int i;

	v = caml_alloc_tuple(16);

	for(i=0; i<16; i++)
		Store_field(v, i, Val_int(uuid[i]));

	CAMLreturn(v);
}

static int Uuid_val(libxl_uuid *c_val, value v)
{
	CAMLparam1(v);
	int i;
	uint8_t *uuid = libxl_uuid_bytearray(c_val);

	for(i=0; i<16; i++)
		uuid[i] = Int_val(Field(v, i));

	CAMLreturn(0);
}

static value Val_defbool(libxl_defbool c_val)
{
	CAMLparam0();
	CAMLlocal1(v);

	if (libxl_defbool_is_default(c_val))
		v = Val_none;
	else {
		bool b = libxl_defbool_val(c_val);
		v = Val_some(b ? Val_bool(true) : Val_bool(false));
	}
	CAMLreturn(v);
}

static libxl_defbool Defbool_val(value v)
{
	CAMLparam1(v);
	libxl_defbool db;
	if (v == Val_none)
		libxl_defbool_unset(&db);
	else {
		bool b = Bool_val(Some_val(v));
		libxl_defbool_set(&db, b);
	}
	return db;
}

static value Val_hwcap(libxl_hwcap *c_val)
{
	CAMLparam0();
	CAMLlocal1(hwcap);
	int i;

	hwcap = caml_alloc_tuple(8);
	for (i = 0; i < 8; i++)
		Store_field(hwcap, i, caml_copy_int32((*c_val)[i]));

	CAMLreturn(hwcap);
}

static value Val_string_option(char *c_val)
{
	CAMLparam0();
	if (c_val)
		CAMLreturn(Val_some(caml_copy_string(c_val)));
	else
		CAMLreturn(Val_none);
}

static char *String_option_val(value v)
{
	char *s = NULL;
	if (v != Val_none)
		s = dup_String_val(Some_val(v));
	return s;
}

#include "_libxl_types.inc"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define _DEVICE_ADDREMOVE(type,op)					\
value stub_xl_device_##type##_##op(value ctx, value info, value domid)	\
{									\
	CAMLparam3(ctx, info, domid);					\
	libxl_device_##type c_info;					\
	int ret, marker_var;						\
									\
	device_##type##_val(CTX, &c_info, info);			\
									\
	ret = libxl_device_##type##_##op(CTX, Int_val(domid), &c_info, 0); \
									\
	libxl_device_##type##_dispose(&c_info);				\
									\
	if (ret != 0)							\
		failwith_xl(ret, STRINGIFY(type) "_" STRINGIFY(op));	\
									\
	CAMLreturn(Val_unit);						\
}

#define DEVICE_ADDREMOVE(type) \
	_DEVICE_ADDREMOVE(type, add) \
 	_DEVICE_ADDREMOVE(type, remove) \
 	_DEVICE_ADDREMOVE(type, destroy)

DEVICE_ADDREMOVE(disk)
DEVICE_ADDREMOVE(nic)
DEVICE_ADDREMOVE(vfb)
DEVICE_ADDREMOVE(vkb)
DEVICE_ADDREMOVE(pci)

value stub_xl_physinfo_get(value ctx)
{
	CAMLparam1(ctx);
	CAMLlocal1(physinfo);
	libxl_physinfo c_physinfo;
	int ret;

	ret = libxl_get_physinfo(CTX, &c_physinfo);

	if (ret != 0)
		failwith_xl(ret, "get_physinfo");

	physinfo = Val_physinfo(&c_physinfo);

	libxl_physinfo_dispose(&c_physinfo);

	CAMLreturn(physinfo);
}

value stub_xl_cputopology_get(value ctx)
{
	CAMLparam1(ctx);
	CAMLlocal2(topology, v);
	libxl_cputopology *c_topology;
	int i, nr;

	c_topology = libxl_get_cpu_topology(CTX, &nr);

	if (!c_topology)
		failwith_xl(ERROR_FAIL, "get_cpu_topologyinfo");

	topology = caml_alloc_tuple(nr);
	for (i = 0; i < nr; i++) {
		if (c_topology[i].core != LIBXL_CPUTOPOLOGY_INVALID_ENTRY)
			v = Val_some(Val_cputopology(&c_topology[i]));
		else
			v = Val_none;
		Store_field(topology, i, v);
	}

	libxl_cputopology_list_free(c_topology, nr);

	CAMLreturn(topology);
}

value stub_xl_domain_sched_params_get(value ctx, value domid)
{
	CAMLparam2(ctx, domid);
	CAMLlocal1(scinfo);
	libxl_domain_sched_params c_scinfo;
	int ret;

	ret = libxl_domain_sched_params_get(CTX, Int_val(domid), &c_scinfo);
	if (ret != 0)
		failwith_xl(ret, "domain_sched_params_get");

	scinfo = Val_domain_sched_params(&c_scinfo);

	libxl_domain_sched_params_dispose(&c_scinfo);

	CAMLreturn(scinfo);
}

value stub_xl_domain_sched_params_set(value ctx, value domid, value scinfo)
{
	CAMLparam3(ctx, domid, scinfo);
	libxl_domain_sched_params c_scinfo;
	int ret;

	domain_sched_params_val(CTX, &c_scinfo, scinfo);

	ret = libxl_domain_sched_params_set(CTX, Int_val(domid), &c_scinfo);

	libxl_domain_sched_params_dispose(&c_scinfo);

	if (ret != 0)
		failwith_xl(ret, "domain_sched_params_set");

	CAMLreturn(Val_unit);
}

value stub_xl_send_trigger(value ctx, value domid, value trigger, value vcpuid)
{
	CAMLparam4(ctx, domid, trigger, vcpuid);
	int ret;
	libxl_trigger c_trigger = LIBXL_TRIGGER_UNKNOWN;

	trigger_val(CTX, &c_trigger, trigger);

	ret = libxl_send_trigger(CTX, Int_val(domid),
				 c_trigger, Int_val(vcpuid));

	if (ret != 0)
		failwith_xl(ret, "send_trigger");

	CAMLreturn(Val_unit);
}

value stub_xl_send_sysrq(value ctx, value domid, value sysrq)
{
	CAMLparam3(ctx, domid, sysrq);
	int ret;

	ret = libxl_send_sysrq(CTX, Int_val(domid), Int_val(sysrq));

	if (ret != 0)
		failwith_xl(ret, "send_sysrq");

	CAMLreturn(Val_unit);
}

value stub_xl_send_debug_keys(value ctx, value keys)
{
	CAMLparam2(ctx, keys);
	int ret;
	char *c_keys;

	c_keys = dup_String_val(keys);

	ret = libxl_send_debug_keys(CTX, c_keys);
	if (ret != 0)
		failwith_xl(ret, "send_debug_keys");

	free(c_keys);

	CAMLreturn(Val_unit);
}

/*
 * Local variables:
 *  indent-tabs-mode: t
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
