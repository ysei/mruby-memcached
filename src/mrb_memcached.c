/*
** mrb_memcached.c - Memcached class
**
** Copyright (c) MATSUMOTO Ryosuke 2014
**
** See Copyright Notice in LICENSE
*/

#include "mruby.h"
#include "mruby/data.h"
#include "mruby/string.h"
#include "mrb_memcached.h"
#include "libmemcached/memcached.h"

#define DONE mrb_gc_arena_restore(mrb, 0);

typedef struct {
  memcached_server_st *msv;
  memcached_st *mst;
  memcached_return mrt;
} mrb_memcached_data;

static void mrb_memcached_data_free(mrb_state *mrb, void *p)
{   
  mrb_memcached_data *data = (mrb_memcached_data *)p;
  memcached_server_list_free(data->msv);
  memcached_free(data->mst);
}

static const struct mrb_data_type mrb_memcached_data_type = {
  "mrb_memcached_data", mrb_memcached_data_free,
};

static mrb_value mrb_memcached_init(mrb_state *mrb, mrb_value self)
{
  mrb_memcached_data *data;
  memcached_server_st *msv;
  memcached_st *mst;
  memcached_return mrt;
  char *host;
  mrb_int port;

  data = (mrb_memcached_data *)DATA_PTR(self);
  if (data) {
    mrb_free(mrb, data);
  }
  DATA_TYPE(self) = &mrb_memcached_data_type;
  DATA_PTR(self) = NULL;

  mrb_get_args(mrb, "zi", &host, &port);

  mst = memcached_create(NULL);  
  msv = memcached_server_list_append(NULL, host, port, &mrt);
  if (mrt != MEMCACHED_SUCCESS) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "libmemcached error: %S"
      , mrb_str_new_cstr(mrb, memcached_strerror(mst, mrt)));
  }
  mrt = memcached_server_push(mst, msv);
  if (mrt != MEMCACHED_SUCCESS) {
    mrb_raisef(mrb, E_RUNTIME_ERROR, "libmemcached error: %S"
      , mrb_str_new_cstr(mrb, memcached_strerror(mst, mrt)));
  }

  data = (mrb_memcached_data *)mrb_malloc(mrb, sizeof(mrb_memcached_data));
  data->msv = msv;
  data->mst = mst;
  data->mrt = mrt;
  DATA_PTR(self) = data;

  return self;
}

static mrb_value mrb_memcached_server_list_append(mrb_state *mrb, mrb_value self)
{
  mrb_memcached_data *data = DATA_PTR(self);
  char *host;
  mrb_int port;

  mrb_get_args(mrb, "zi", &host, &port);
  data->msv = memcached_server_list_append(data->msv, host, port, &(data->mrt));
  if (data->mrt != MEMCACHED_SUCCESS) {
    // can't add server to memcached server list
    mrb_raisef(mrb, E_RUNTIME_ERROR, "libmemcached error: %S"
      , mrb_str_new_cstr(mrb, memcached_strerror(data->mst, data->mrt)));
  }
  data->mrt = memcached_server_push(data->mst, data->msv);
  if (data->mrt != MEMCACHED_SUCCESS) {
    // can't push server list to memcached context
    mrb_raisef(mrb, E_RUNTIME_ERROR, "libmemcached error: %S"
      , mrb_str_new_cstr(mrb, memcached_strerror(data->mst, data->mrt)));
  }

  return mrb_fixnum_value(data->mrt);
}

static mrb_value mrb_memcached_close(mrb_state *mrb, mrb_value self)
{
  mrb_memcached_data *data = DATA_PTR(self);
  memcached_server_list_free(data->msv);
  memcached_free(data->mst);
  return self;
}

static mrb_value mrb_memcached_set(mrb_state *mrb, mrb_value self)
{
  mrb_value key, val;
  mrb_memcached_data *data = DATA_PTR(self);

  mrb_get_args(mrb, "oo", &key, &val);
  data->mrt = memcached_set(data->mst, RSTRING_PTR(key), RSTRING_LEN(key), RSTRING_PTR(val), RSTRING_LEN(val), (time_t)600, (uint32_t)0);
  if (data->mrt != MEMCACHED_SUCCESS && data->mrt != MEMCACHED_BUFFERED) {
    // set failed
    return mrb_nil_value();
  }
  return mrb_fixnum_value(data->mrt);
}

static mrb_value mrb_memcached_get(mrb_state *mrb, mrb_value self)
{
  mrb_value key;
  char *val;
  size_t len;
  uint32_t flags;
  mrb_memcached_data *data = DATA_PTR(self);

  mrb_get_args(mrb, "o", &key);
  val = memcached_get(data->mst, RSTRING_PTR(key), RSTRING_LEN(key), &len, &flags, &(data->mrt));
  if (data->mrt != MEMCACHED_SUCCESS && data->mrt != MEMCACHED_BUFFERED) {
    free(val);
    // value not found
    return mrb_nil_value();
  }
  return mrb_str_new(mrb, val, len);
}

void mrb_mruby_memcached_gem_init(mrb_state *mrb)
{
    struct RClass *memcached;
    memcached = mrb_define_class(mrb, "Memcached", mrb->object_class);
    mrb_define_method(mrb, memcached, "initialize", mrb_memcached_init, MRB_ARGS_REQ(1));
    mrb_define_method(mrb, memcached, "add_server", mrb_memcached_server_list_append, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, memcached, "close", mrb_memcached_close, MRB_ARGS_NONE());
    mrb_define_method(mrb, memcached, "set", mrb_memcached_set, MRB_ARGS_REQ(2));
    mrb_define_method(mrb, memcached, "get", mrb_memcached_get, MRB_ARGS_REQ(1));
    DONE;
}

void mrb_mruby_memcached_gem_final(mrb_state *mrb)
{
}
