/*
 * ============================================================================
 * Copyright (c) 2014 Hardy-Francis Enterprises Inc.
 * This file is part of SharedHashFile.
 *
 * SharedHashFile is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version.
 *
 * SharedHashFile is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see www.gnu.org/licenses/.
 * ----------------------------------------------------------------------------
 * To use SharedHashFile in a closed-source product, commercial licenses are
 * available; email office [@] sharedhashfile [.] com for more information.
 * ============================================================================
 */

// include PHP API
#include <php.h>

#include <shf.private.h>
#include <shf.h>

PHP_MINIT_FUNCTION(sharedhashfile) {
  sharedhashfile_init_sharedhashfile(TSRMLS_C);
}

// header file we'll create below
#include "sharedhashfile.php.h"

// define the function(s) we want to add
zend_function_entry sharedhashfile_functions[] = {
  PHP_FE(helloworld, NULL)
  PHP_FE(dummy1, NULL)
  PHP_FE(dummy2, NULL)
  PHP_FE(dummy3, NULL)
  PHP_FE(dummy4, NULL)
  PHP_FE(dummy5, NULL)
  { NULL, NULL, NULL }
};

// "sharedhashfile_functions" refers to the struct defined above
// we'll be filling in more of this later: you can use this to specify
// globals, php.ini info, startup and teardown functions, etc.
zend_module_entry sharedhashfile_module_entry = {
  STANDARD_MODULE_HEADER,
  PHP_SHAREDHASHFILE_EXTNAME,
  sharedhashfile_functions,
  PHP_MINIT(sharedhashfile),
  NULL,
  NULL,
  NULL,
  NULL,
  PHP_SHAREDHASHFILE_VERSION,
  STANDARD_MODULE_PROPERTIES
};

// install module
ZEND_GET_MODULE(sharedhashfile)

// actual non-template code!
PHP_FUNCTION(helloworld) {
  // php_printf is PHP's version of printf, it's essentially "echo" from C
  SHF_DEBUG("C: In his BIG house at R'lyeh dead helloworld waits dreaming.\n");
}

PHP_FUNCTION(dummy1) {
  // do nothing!
}

PHP_FUNCTION(dummy2) { // one bool parameter
  // boolean type
  zend_bool english = 0;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "b", &english) == FAILURE) {
    return;
  }
}

PHP_FUNCTION(dummy3) { // two string parameters
    char * key;
    int    key_len;
    char * val;
    int    val_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val, &val_len) == FAILURE) {
      return;
    }
}

PHP_FUNCTION(dummy4) { // two string parameters and one return string to copy
  char * key;
  int    key_len;
  char * val;
  int    val_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val, &val_len) == FAILURE) {
    return;
  }

  RETURN_STRING("banana", 1 /* copy string */); // http://docstore.mik.ua/orelly/weblinux2/php/ch14_08.htm
}

PHP_FUNCTION(dummy5) { // two string parameters and one return copied string
  char * key;
  int    key_len;
  char * val;
  int    val_len;

  if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val, &val_len) == FAILURE) {
    return;
  }

  char *str = emalloc(7);
  strcpy(str, "banana");
  RETURN_STRINGL(str, 6, 0); // http://docstore.mik.ua/orelly/weblinux2/php/ch14_08.htm
}

zend_class_entry * sharedhashfile_ce_sharedhashfile;

static function_entry sharedhashfile_methods[] = {
    PHP_ME(SharedHashFile, __construct         , NULL, ZEND_ACC_PUBLIC|ZEND_ACC_CTOR  )
    PHP_ME(SharedHashFile, sacrifice           , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, createSharedHashFile, NULL, ZEND_ACC_PUBLIC|ZEND_ACC_STATIC)
    PHP_ME(SharedHashFile, getId               , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, attach              , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, putKeyVal           , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, putKeyValOver       , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, getKeyVal           , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, getKeyValOver       , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, hasKey              , NULL, ZEND_ACC_PUBLIC                )
    PHP_ME(SharedHashFile, delKeyVal           , NULL, ZEND_ACC_PUBLIC                )
    {NULL, NULL, NULL}
};

static void
shf_log_output_php_printf(char * log_line, uint32_t log_line_len)
{
    log_line[0] = '?' == log_line[0] ? 'C' : log_line[0]; /* mark as logged output by stdout */
    php_printf("%.*s", log_line_len, log_line);
    //fixme fflush(stdout);
} /* shf_log_output_php_printf() */

void sharedhashfile_init_sharedhashfile(TSRMLS_D) {
    zend_class_entry ce;

    shf_log_output_set(shf_log_output_php_printf);
    SHF_DEBUG("sharedhashfile_init_sharedhashfile(TSRMLS_D)() // called once upon PHP loading\n");

    INIT_CLASS_ENTRY(ce, "SharedHashFile", sharedhashfile_methods);

    ce.create_object = xxx_SharedHashFile___construct_object;
    sharedhashfile_ce_sharedhashfile = zend_register_internal_class(&ce TSRMLS_CC);

    shf_init();

    /* fields */
    zend_declare_property_bool(sharedhashfile_ce_sharedhashfile, "alive", strlen("alive"), 1, ZEND_ACC_PUBLIC TSRMLS_CC);
}

PHP_METHOD(SharedHashFile, sacrifice) {
  SHF_DEBUG("C: PHP_METHOD(SharedHashFile, sacrifice)()\n");
  // TODO
}

PHP_METHOD(SharedHashFile, createSharedHashFile) {
    SHF_DEBUG("C: PHP_METHOD(SharedHashFile, createSharedHashFile)()\n");
    object_init_ex(return_value, sharedhashfile_ce_sharedhashfile);
}

uint64_t uid = 0;

PHP_METHOD(SharedHashFile, __construct) {
    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    uid ++;
    this->uid = uid;

    SHF_DEBUG("%s() // ->uid=%lu\n", __FUNCTION__, this->uid);

    this->is_attached = 0;
}

PHP_METHOD(SharedHashFile, attach) {
    char * path;
    int    path_len;
    char * name;
    int    name_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &path, &path_len, &name, &name_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    this->shf = shf_attach(path, name, 1 /* todo: make is a parameter! */);

    SHF_DEBUG("%s(%.*s, %.*s) // ->uid=%lu ->shf=%p\n", __FUNCTION__, path_len, path, name_len, name, this->uid, this->shf);
}

PHP_METHOD(SharedHashFile, putKeyVal) {
    char * key;
    int    key_len;
    char * val;
    int    val_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val, &val_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s(%.*s, %.*s) // ->uid=%lu\n", __FUNCTION__, key_len, key, val_len, val, this->uid);

                   shf_make_hash(key, key_len);
    uint32_t uid = shf_put_key_val(this->shf, val, val_len);
}

PHP_METHOD(SharedHashFile, putKeyValOver) {
    char * key;
    int    key_len;
    char * val;
    int    val_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss", &key, &key_len, &val, &val_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s(%.*s, %.*s) // ->uid=%lu\n", __FUNCTION__, key_len, key, val_len, val, this->uid);

                 shf_make_hash(key, key_len);
    int exists = shf_put_key_val_over(this->shf, val, val_len);
    SHF_DEBUG("this->shf=%p\n", this->shf);
}

PHP_METHOD(SharedHashFile, getKeyVal) {
    char * key;
    int    key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s(%.*s) // ->uid=%lu\n", __FUNCTION__, key_len, key, this->uid);

                 shf_make_hash(key, key_len);
    int exists = shf_get_key_val_copy(this->shf);

    if (exists) {
        char * bytes = emalloc(shf_val_len);
        memcpy(bytes, shf_val, shf_val_len);
        RETURN_STRINGL(bytes, shf_val_len, 0); // http://docstore.mik.ua/orelly/weblinux2/php/ch14_08.htm
    }
    else {
        // todo: how to return null
    }
}

PHP_METHOD(SharedHashFile, getKeyValOver) {
    char * key;
    int    key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s(%.*s) // ->uid=%lu\n", __FUNCTION__, key_len, key, this->uid);

                 shf_make_hash(key, key_len);
    int exists = shf_get_key_val_copy(this->shf);

    if (exists) {
        SHF_ASSERT(shf_val_len >= 4, "ERROR: INTERNAL: Was key put with putKeyValOver()? Returned string of %u bytes is not big enough for over length", shf_val_len);
        uint32_t * val_len_over_addr = SHF_CAST(uint32_t *, shf_val);
        uint32_t   val_len_over = *val_len_over_addr;
        SHF_ASSERT(shf_val_len >= 4 + val_len_over, "ERROR: INTERNAL: Was key put with putKeyValOver()? Returned string of %u bytes is smaller than val_len_over=%u", shf_val_len, val_len_over);
        char * bytes = emalloc(val_len_over);
        memcpy(bytes, shf_val + 4, val_len_over);
        RETURN_STRINGL(bytes, val_len_over, 0); // http://docstore.mik.ua/orelly/weblinux2/php/ch14_08.htm
    }
    else {
        // todo: how to return null
    }
}

PHP_METHOD(SharedHashFile, hasKey) {
    char * key;
    int    key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s(%.*s) // ->uid=%lu\n", __FUNCTION__, key_len, key, this->uid);

                 shf_make_hash(key, key_len);
    int exists = shf_has_key(this->shf);

    RETURN_BOOL(exists);
}

PHP_METHOD(SharedHashFile, delKeyVal) {
    char * key;
    int    key_len;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &key, &key_len) == FAILURE) {
      return;
    }

    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s(%.*s) // ->uid=%lu\n", __FUNCTION__, key_len, key, this->uid);

                 shf_make_hash(key, key_len);
    int exists = shf_del_key_val(this->shf);
}

PHP_METHOD(SharedHashFile, getId) {
    obj_SharedHashFile * this = (obj_SharedHashFile *) zend_object_store_get_object(getThis() TSRMLS_CC);

    SHF_DEBUG("%s() // ->uid=%lu\n", __FUNCTION__, this->uid);

    RETURN_LONG(this->uid);
}

zend_object_value
xxx_SharedHashFile___construct_object(zend_class_entry *class_type TSRMLS_DC) {
  zend_object_value    retval;
  obj_SharedHashFile * intern;
  zval               * tmp;

  SHF_DEBUG("%s()\n", __FUNCTION__);

  // allocate the struct we're going to use
  intern = (obj_SharedHashFile *) emalloc(sizeof(obj_SharedHashFile));
  memset(intern, 0, sizeof(obj_SharedHashFile));

  // create a table for class properties
  zend_object_std_init(&intern->std, class_type TSRMLS_CC);
  zend_hash_copy(intern->std.properties,
     &class_type->default_properties,
     (copy_ctor_func_t) zval_add_ref,
     (void *) &tmp,
     sizeof(zval *));

  // create a destructor for this struct
  retval.handle = zend_objects_store_put(intern, (zend_objects_store_dtor_t) zend_objects_destroy_object, xxx_SharedHashFile___destruct_object, NULL TSRMLS_CC);
  retval.handlers = zend_get_std_object_handlers();

  return retval;
} // xxx_SharedHashFile___construct_object()

void
xxx_SharedHashFile___destruct_object(void * object TSRMLS_DC) {
    obj_SharedHashFile * this = (obj_SharedHashFile *) object;

    SHF_DEBUG("%s() // ->uid=%lu\n", __FUNCTION__, this->uid);

    if (this->prayer) {
        efree(this->prayer);
    }
    efree(this);
} // xxx_SharedHashFile___destruct_object()

