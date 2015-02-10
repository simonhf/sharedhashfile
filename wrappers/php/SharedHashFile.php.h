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

#define PHP_SHAREDHASHFILE_EXTNAME "sharedhashfile"
#define PHP_SHAREDHASHFILE_VERSION "0.01"

PHP_FUNCTION(helloworld);
PHP_FUNCTION(dummy1);
PHP_FUNCTION(dummy2);
PHP_FUNCTION(dummy3);
PHP_FUNCTION(dummy4);
PHP_FUNCTION(dummy5);

void sharedhashfile_init_sharedhashfile(TSRMLS_D);

typedef struct obj_SharedHashFile {
    // required
    zend_object std;

    // actual struct contents
    SHF      * shf;
    uint32_t   is_attached;

    uint64_t uid;
    char *prayer;
} obj_SharedHashFile;

zend_object_value xxx_SharedHashFile___construct_object(zend_class_entry * class_type TSRMLS_DC);
void              xxx_SharedHashFile___destruct_object (void             * object     TSRMLS_DC);

PHP_METHOD(SharedHashFile, sacrifice);
PHP_METHOD(SharedHashFile, createSharedHashFile);
PHP_METHOD(SharedHashFile, __construct);
PHP_METHOD(SharedHashFile, getId);
PHP_METHOD(SharedHashFile, attach);
PHP_METHOD(SharedHashFile, putKeyVal);
PHP_METHOD(SharedHashFile, putKeyValOver);
PHP_METHOD(SharedHashFile, getKeyVal);
PHP_METHOD(SharedHashFile, getKeyValOver);
PHP_METHOD(SharedHashFile, hasKey);
PHP_METHOD(SharedHashFile, delKeyVal);

