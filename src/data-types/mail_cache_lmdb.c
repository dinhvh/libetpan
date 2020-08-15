/*
 * libEtPan! -- a mail stuff library
 *
 * Copyright (C) 2001, 2005 - DINH Viet Hoa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the libEtPan! project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: mail_cache_lmdb.c,v 1.20 2018/10/12 13:39:40 lay Exp $
 */

#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#ifdef LMDB

#include "mail_cache_db.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <lmdb.h>

#include "libetpan-config.h"

#include "maillock.h"


static struct mail_cache_db * mail_cache_lmdb_new(MDB_env *env)
{
  struct mail_cache_db * cache_db;

  cache_db = malloc(sizeof(* cache_db));
  if (cache_db == NULL)
    return NULL;
  cache_db->internal_database = env;

  return cache_db;
}

static void mail_cache_db_free(struct mail_cache_db * cache_db)
{
  free(cache_db);
}


void mail_cache_db_close(struct mail_cache_db * cache_db)
{
  MDB_env *env;

  env = cache_db->internal_database;
  mdb_env_close(env);
  mail_cache_db_free(cache_db);
}

int mail_cache_db_open(const char * filename,
    struct mail_cache_db ** pcache_db)
{
  int r;
  struct mail_cache_db * cache_db;
  MDB_env *env;

  r = mdb_env_create(&env);
  if (r != 0)
    return -1;

  r = mdb_env_set_mapsize(env, 512*1024*1024 /*max mmap and file size*/);
  if (r != 0)
    return -1;

  r = mdb_env_open(env, filename, MDB_NOSUBDIR, 0660);
  if (r != 0)
    goto close_db;

  cache_db = mail_cache_lmdb_new(env);
  if (cache_db == NULL)
    goto close_db;

  * pcache_db = cache_db;

  return 0;

  close_db:
    mdb_env_close(env);
    return -1;

}

int mail_cache_db_open_lock(const char * filename,
    struct mail_cache_db ** pcache_db)
{
  int r;
  struct mail_cache_db * cache_db;
  
  r = maillock_write_lock(filename, -1);
  if (r < 0)
    goto err;
  r = mail_cache_db_open(filename, &cache_db);
  if (r < 0)
    goto unlock;
  
  * pcache_db = cache_db;

  return 0;

 unlock:
  maillock_write_unlock(filename, -1);
 err:
  return -1;
}

void mail_cache_db_close_unlock(const char * filename,
    struct mail_cache_db * cache_db)
{
  mail_cache_db_close(cache_db);
  maillock_write_unlock(filename, -1);
}


int mail_cache_db_put(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, const void * value, size_t value_len)
{
  int r;
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val mdb_key;
  MDB_val mdb_val;

  env = cache_db->internal_database;

  mdb_key.mv_size = key_len;
  mdb_key.mv_data = (void *) key;
  mdb_val.mv_size = value_len;
  mdb_val.mv_data = (void *) value;

  r = mdb_txn_begin(env, NULL, 0, &txn);
  if (r != 0)
    return -1;
  r = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (r != 0)
    goto error;

  r = mdb_put(txn, dbi, &mdb_key, &mdb_val, 0);
  if (r != 0)
    goto error;

  mdb_txn_commit(txn);
  return 0;

  error:
    mdb_txn_abort(txn);
    return -1;
}

int mail_cache_db_get(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, void ** pvalue, size_t * pvalue_len)
{
  int r;
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val mdb_key;
  MDB_val mdb_val;

  env = cache_db->internal_database;

  mdb_key.mv_size = key_len;
  mdb_key.mv_data = (void *) key;

  r = mdb_txn_begin(env, NULL, 0, &txn);
  if (r != 0)
    return -1;
  r = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (r != 0)
    goto error;

  r = mdb_get(txn, dbi, &mdb_key, &mdb_val);
  if (r != 0)
    goto error;

  * pvalue = mdb_val.mv_data;
  * pvalue_len = mdb_val.mv_size;
  mdb_txn_commit(txn);
  return 0;

  error:
    mdb_txn_abort(txn);
    return -1;
}

int mail_cache_db_del(struct mail_cache_db * cache_db,
    const void * key, size_t key_len)
{
  int r;
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val mdb_key;

  env = cache_db->internal_database;

  mdb_key.mv_size = key_len;
  mdb_key.mv_data = (void *) key;

  r = mdb_txn_begin(env, NULL, 0, &txn);
  if (r != 0)
    return -1;
  r = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (r != 0)
    goto error;

  r = mdb_del(txn, dbi, &mdb_key, NULL);
  if (r != 0)
    goto error;

  return 0;

  error:
    mdb_txn_abort(txn);
    return -1;
}

int mail_cache_db_clean_up(struct mail_cache_db * cache_db,
    chash * exist)
{
  int r;
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_cursor *cursor;
  MDB_val mdb_key;
  MDB_val mdb_val;

  env = cache_db->internal_database;

  r = mdb_txn_begin(env, NULL, 0, &txn);
  if (r != 0)
    return -1;
  r = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (r != 0)
    goto error;

  r = mdb_cursor_open(txn, dbi, &cursor);
  if (r != 0)
    goto error;

  r = mdb_cursor_get(cursor, &mdb_key, &mdb_val, MDB_FIRST);
  if (r != 0)
    goto cursor_error;

  while (r == 0) {
    chashdatum hash_key;
    chashdatum hash_data;

    hash_key.data = mdb_key.mv_data;
    hash_key.len = (unsigned int) mdb_key.mv_size;

    r = chash_get(exist, &hash_key, &hash_data);
    if (r < 0) {
      r = mdb_cursor_del(cursor, 0);
      if (r != 0)
        goto cursor_error;
    }
    r = mdb_cursor_get(cursor, &mdb_key, &mdb_val, MDB_NEXT);
  }

  mdb_txn_commit(txn);
  return 0;
  cursor_error:
    mdb_cursor_close(cursor);
  error:
    mdb_txn_abort(txn);
  return -1;
}

int mail_cache_db_get_size(struct mail_cache_db * cache_db,
    const void * key, size_t key_len, size_t * pvalue_len)
{
  int r;
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_val mdb_key;
  MDB_val mdb_val;

  env = cache_db->internal_database;

  mdb_key.mv_size = key_len;
  mdb_key.mv_data = (void *) key;

  r = mdb_txn_begin(env, NULL, 0, &txn);
  if (r != 0)
    return -1;
  r = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (r != 0)
    goto error;

  r = mdb_get(txn, dbi, &mdb_key, &mdb_val);
  if (r != 0)
    goto error;

  * pvalue_len = mdb_val.mv_size;
  mdb_txn_commit(txn);
  return 0;

  error:
    mdb_txn_abort(txn);
    return -1;
}

int mail_cache_db_get_keys(struct mail_cache_db * cache_db,
    chash * keys)
{
  int r;
  MDB_env *env;
  MDB_txn *txn;
  MDB_dbi dbi;
  MDB_cursor *cursor;
  MDB_val mdb_key;
  MDB_val mdb_val;

  env = cache_db->internal_database;

  r = mdb_txn_begin(env, NULL, 0, &txn);
  if (r != 0)
    return -1;
  r = mdb_dbi_open(txn, NULL, 0, &dbi);
  if (r != 0)
    goto error;

  r = mdb_cursor_open(txn, dbi, &cursor);
  if (r != 0)
    goto error;

  r = mdb_cursor_get(cursor, &mdb_key, &mdb_val, MDB_FIRST);
  if (r != 0)
    goto cursor_error;

  while (r == 0) {
    chashdatum hash_key;
    chashdatum hash_data;

    hash_key.data = mdb_key.mv_data;
    hash_key.len = (unsigned int) mdb_key.mv_size;
    hash_data.data = NULL;
    hash_data.len = 0;

    r = chash_set(keys, &hash_key, &hash_data, NULL);
    if (r != 0)
      goto cursor_error;
    r = mdb_cursor_get(cursor, &mdb_key, &mdb_val, MDB_NEXT);
  }

  mdb_txn_commit(txn);
  return 0;

  cursor_error:
    mdb_cursor_close(cursor);
  error:
    mdb_txn_abort(txn);
  return -1;
}
#endif
