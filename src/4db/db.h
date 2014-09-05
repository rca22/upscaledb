/*
 * Copyright (C) 2005-2014 Christoph Rupp (chris@crupp.de).
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * @exception_safe: unknown
 * @thread_safe: unknown
 */

#ifndef HAM_DB_H
#define HAM_DB_H

#include "0root/root.h"

#include "ham/hamsterdb_ola.h"

// Always verify that a file of level N does not include headers > N!
#include "1base/byte_array.h"
#include "4env/env.h"

#ifndef HAM_ROOT_H
#  error "root.h was not included"
#endif

// A helper structure; ham_db_t is declared in ham/hamsterdb.h as an
// opaque C structure, but internally we use a C++ class. The ham_db_t
// struct satisfies the C compiler, and internally we just cast the pointers.
struct ham_db_t {
  int dummy;
};

namespace hamsterdb {

class Cursor;

//
// The ScanVisitor is the callback implementation for the scan call.
// It will either receive single keys or multiple keys in an array.
//
struct ScanVisitor {
  // Operates on a single key
  virtual void operator()(const void *key_data, ham_u16_t key_size, 
                  size_t duplicate_count) = 0;

  // Operates on an array of keys
  virtual void operator()(const void *key_array, size_t key_count) = 0;

  // Assigns the internal result to |result|
  virtual void assign_result(hola_result_t *result) = 0;
};

/*
 * An abstract base class for a Database; is overwritten for local and
 * remote implementations
 */
class Database
{
  public:
    enum {
      // The default number of indices in an Environment
      kMaxIndices1k = 32
    };

    // Constructor
    Database(Environment *env, ham_u16_t name, ham_u32_t flags);

    // Virtual destructor; can be overwritten by base-classes
    virtual ~Database() {
    }

    // Returns the Environment pointer
    Environment *get_env() {
      return (m_env);
    }

    // Returns the runtime-flags - the flags are "mixed" with the flags from
    // the Environment
    ham_u32_t get_rt_flags(bool raw = false) {
      if (raw)
        return (m_rt_flags);
      else
        return (m_env->get_flags() | m_rt_flags);
    }

    // Returns the database name
    ham_u16_t get_name() const {
      return (m_name);
    }

    // Sets the database name
    void set_name(ham_u16_t name) {
      m_name = name;
    }

    // Returns Database parameters (ham_db_get_parameters)
    virtual ham_status_t get_parameters(ham_parameter_t *param) = 0;

    // Checks Database integrity (ham_db_check_integrity)
    virtual ham_status_t check_integrity(ham_u32_t flags) = 0;

    // Returns the number of keys (ham_db_get_key_count)
    virtual void count(Transaction *txn, bool distinct,
                    ham_u64_t *keycount) = 0;

    // Scans the whole database, applies a processor function
    virtual void scan(Transaction *txn, ScanVisitor *visitor,
                    bool distinct) = 0;

    // Inserts a key/value pair (ham_db_insert)
    virtual ham_status_t insert(Transaction *txn, ham_key_t *key,
                    ham_record_t *record, ham_u32_t flags) = 0;

    // Erase a key/value pair (ham_db_erase)
    virtual ham_status_t erase(Transaction *txn, ham_key_t *key,
                    ham_u32_t flags) = 0;

    // Lookup of a key/value pair (ham_db_find)
    virtual ham_status_t find(Transaction *txn, ham_key_t *key,
                    ham_record_t *record, ham_u32_t flags) = 0;

    // Creates a cursor (ham_cursor_create)
    virtual Cursor *cursor_create(Transaction *txn, ham_u32_t flags);

    // Clones a cursor (ham_cursor_clone)
    virtual Cursor *cursor_clone(Cursor *src);

    // Inserts a key with a cursor (ham_cursor_insert)
    virtual ham_status_t cursor_insert(Cursor *cursor, ham_key_t *key,
                    ham_record_t *record, ham_u32_t flags) = 0;

    // Erases the key of a cursor (ham_cursor_erase)
    virtual ham_status_t cursor_erase(Cursor *cursor, ham_u32_t flags) = 0;

    // Positions the cursor on a key and returns the record (ham_cursor_find)
    virtual ham_status_t cursor_find(Cursor *cursor, ham_key_t *key,
                    ham_record_t *record, ham_u32_t flags) = 0;

    // Returns number of duplicates (ham_cursor_get_record_count)
    // TODO return count instead of status?
    virtual ham_status_t cursor_get_record_count(Cursor *cursor,
                    ham_u32_t *count, ham_u32_t flags) = 0;

    // Returns position in duplicate list (ham_cursor_get_duplicate_position)
    virtual ham_u32_t cursor_get_duplicate_position(Cursor *cursor) = 0;

    // Get current record size (ham_cursor_get_record_size)
    // TODO return size instead of status?
    virtual ham_status_t cursor_get_record_size(Cursor *cursor,
                    ham_u64_t *size) = 0;

    // Overwrites the record of a cursor (ham_cursor_overwrite)
    virtual ham_status_t cursor_overwrite(Cursor *cursor,
                    ham_record_t *record, ham_u32_t flags) = 0;

    // Moves a cursor, returns key and/or record (ham_cursor_move)
    virtual ham_status_t cursor_move(Cursor *cursor,
                    ham_key_t *key, ham_record_t *record, ham_u32_t flags) = 0;

    // Closes a cursor (ham_cursor_close)
    void cursor_close(Cursor *cursor);

    // Closes the Database (ham_db_close)
    ham_status_t close(ham_u32_t flags);

    // Returns the last error code
    ham_status_t get_error() const {
      return (m_error);
    }

    // Sets the last error code
    ham_status_t set_error(ham_status_t e) {
      return ((m_error = e));
    }

    // Returns the user-provided context pointer (ham_get_context_data)
    void *get_context_data() {
      return (m_context);
    }

    // Sets the user-provided context pointer (ham_set_context_data)
    void set_context_data(void *ctxt) {
      m_context = ctxt;
    }

    // Returns the head of the linked list with all cursors
    Cursor *get_cursor_list() {
      return (m_cursor_list);
    }

    // Returns the memory buffer for the key data
    ByteArray &get_key_arena() {
      return (m_key_arena);
    }

    // Returns the memory buffer for the record data
    ByteArray &get_record_arena() {
      return (m_record_arena);
    }

  protected:
    // Creates a cursor; this is the actual implementation
    virtual Cursor *cursor_create_impl(Transaction *txn, ham_u32_t flags) = 0;

    // Clones a cursor; this is the actual implementation
    virtual Cursor *cursor_clone_impl(Cursor *src) = 0;

    // Closes a cursor; this is the actual implementation
    virtual void cursor_close_impl(Cursor *c) = 0;

    // Closes a database; this is the actual implementation
    virtual ham_status_t close_impl(ham_u32_t flags) = 0;

    // the current Environment
    Environment *m_env;

    // the Database name
    ham_u16_t m_name;

    // the last error code
    ham_status_t m_error;

    // the user-provided context data
    void *m_context;

    // linked list of all cursors
    Cursor *m_cursor_list;

    // The database flags - a combination of the persistent flags
    // and runtime flags
    ham_u32_t m_rt_flags;

    // This is where key->data points to when returning a
    // key to the user; used if Transactions are disabled
    ByteArray m_key_arena;

    // This is where record->data points to when returning a
    // record to the user; used if Transactions are disabled
    ByteArray m_record_arena;
};

} // namespace hamsterdb

#endif /* HAM_DB_H */