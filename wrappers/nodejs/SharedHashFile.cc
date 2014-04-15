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

#ifndef BUILDING_NODE_EXTENSION
#define BUILDING_NODE_EXTENSION
#endif
#include <node.h>

#include <SharedHashFile.hpp>

using namespace v8;

// Useful links WRT creating nodejs wrappers:
// See http://luismreis.github.io/node-bindings-guide/docs/returning.html
// See https://github.com/TooTallNate/node-gyp
// See https://github.com/TooTallNate/node-gyp/wiki/%22binding.gyp%22-files-out-in-the-wild

// todo: Consider handling native javascript values as sharedhashfile values; faster? See https://www.npmjs.org/package/hashtable implementation; "values can be any javascript type, including objects".
// todo: This code probably needs to be reviewed by somebody who is experienced creating nodejs wrappers for C/C++ code!

#define SHF_VALIDATE_ARG_COUNT_REQUIRED(ARGS_REQUIRED) \
    int32_t shf_args_required = ARGS_REQUIRED; \
    if (shf_args_required != args.Length()) { \
        ThrowException(Exception::TypeError(String::New("Wrong number of arguments"))); \
        return scope.Close(Undefined()); \
    }

#define SHF_VALIDATE_ARG_IS_STRING(ARG) \
    if (!args[ARG]->IsString()) { \
        ThrowException(Exception::TypeError(String::New("Wrong argument type; must be string"))); \
        return scope.Close(Undefined()); \
    } \
    String::Utf8Value arg##ARG(args[ARG]); \
    if (0 == arg##ARG.length()) { \
        ThrowException(Exception::TypeError(String::New("Wrong argument length; must be > 0"))); \
        return scope.Close(Undefined()); \
    }

#define SHF_VALIDATE_ARG_IS_INT32(ARG) \
    if (!args[ARG]->IsNumber()) { \
        ThrowException(Exception::TypeError(String::New("Wrong argument type; must be number; int32"))); \
        return scope.Close(Undefined()); \
    } \
    uint32_t arg##ARG = args[ARG]->ToInt32()->Value();

#define SHF_GET_SHAREDHASHFILE_OBJ() \
    sharedHashFile * obj = ObjectWrap::Unwrap<sharedHashFile>(args.This());

#define SHF_HANDLE_SCOPE() \
    HandleScope scope;

class sharedHashFile : public node::ObjectWrap {
public:
    static void Init(v8::Handle<v8::Object> target);

private:
    sharedHashFile();
    ~sharedHashFile();

    static v8::Handle<v8::Value> New                         (const v8::Arguments& args);
    static v8::Handle<v8::Value> AttachExisting              (const v8::Arguments& args);
    static v8::Handle<v8::Value> Attach                      (const v8::Arguments& args);
    static v8::Handle<v8::Value> MakeHash                    (const v8::Arguments& args);
    static v8::Handle<v8::Value> DelKeyVal                   (const v8::Arguments& args);
    static v8::Handle<v8::Value> DelUidVal                   (const v8::Arguments& args);
    static v8::Handle<v8::Value> PutKeyVal                   (const v8::Arguments& args);
    static v8::Handle<v8::Value> GetKeyVal                   (const v8::Arguments& args);
    static v8::Handle<v8::Value> GetUidVal                   (const v8::Arguments& args);
    static v8::Handle<v8::Value> DebugVerbosityLess          (const v8::Arguments& args);
    static v8::Handle<v8::Value> DebugVerbosityMore          (const v8::Arguments& args);
    static v8::Handle<v8::Value> DebugGetBytesMarkedAsDeleted(const v8::Arguments& args);

    SharedHashFile * shf;
};

sharedHashFile::sharedHashFile() {};
sharedHashFile::~sharedHashFile() {};

void
sharedHashFile::Init(Handle<Object> target) {
    SHF_DEBUG("%s()\n", __FUNCTION__);

    // Prepare constructor template
    Local<FunctionTemplate> tpl = FunctionTemplate::New(New);
    tpl->SetClassName(String::NewSymbol("sharedHashFile"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1); // todo: figure out what this is good for! see: http://stackoverflow.com/questions/16600735/what-is-an-internal-field-count-and-what-is-setinternalfieldcount-used-for

    // Prototype
    tpl->PrototypeTemplate()->Set(String::NewSymbol("attachExisting"              ), FunctionTemplate::New(AttachExisting              )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("attach"                      ), FunctionTemplate::New(Attach                      )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("makeHash"                    ), FunctionTemplate::New(MakeHash                    )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("delKeyVal"                   ), FunctionTemplate::New(DelKeyVal                   )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("delUidVal"                   ), FunctionTemplate::New(DelUidVal                   )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("putKeyVal"                   ), FunctionTemplate::New(PutKeyVal                   )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getKeyVal"                   ), FunctionTemplate::New(GetKeyVal                   )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("getUidVal"                   ), FunctionTemplate::New(GetUidVal                   )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("debugVerbosityLess"          ), FunctionTemplate::New(DebugVerbosityLess          )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("debugVerbosityMore"          ), FunctionTemplate::New(DebugVerbosityMore          )->GetFunction());
    tpl->PrototypeTemplate()->Set(String::NewSymbol("debugGetBytesMarkedAsDeleted"), FunctionTemplate::New(DebugGetBytesMarkedAsDeleted)->GetFunction());

    Persistent<Function> constructor = Persistent<Function>::New(tpl->GetFunction());
    target->Set(String::NewSymbol("sharedHashFile"), constructor);
}

Handle<Value>
sharedHashFile::New(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(0);

    sharedHashFile * obj = new sharedHashFile();
    obj->shf = new SharedHashFile;
    obj->Wrap(args.This());

    return args.This();
}

Handle<Value>
sharedHashFile::AttachExisting(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(2);
    SHF_VALIDATE_ARG_IS_STRING(0);
    SHF_VALIDATE_ARG_IS_STRING(1);
    SHF_GET_SHAREDHASHFILE_OBJ();

    uint32_t value = obj->shf->AttachExisting(*arg0, *arg1);

    return scope.Close(Number::New(value));
}

Handle<Value>
sharedHashFile::Attach(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(2);
    SHF_VALIDATE_ARG_IS_STRING(0);
    SHF_VALIDATE_ARG_IS_STRING(1);
    SHF_GET_SHAREDHASHFILE_OBJ();

    uint32_t value = obj->shf->Attach(*arg0, *arg1);

    return scope.Close(Number::New(value));
}

Handle<Value>
sharedHashFile::MakeHash(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(1);
    SHF_VALIDATE_ARG_IS_STRING(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    obj->shf->MakeHash(*arg0, arg0.length());

    return scope.Close(Undefined());
}

Handle<Value>
sharedHashFile::PutKeyVal(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(2);
    SHF_VALIDATE_ARG_IS_STRING(0);
    SHF_VALIDATE_ARG_IS_STRING(1);
    SHF_GET_SHAREDHASHFILE_OBJ();

    obj->shf->MakeHash(*arg0, arg0.length());
    uint32_t uid = obj->shf->PutKeyVal(*arg1, arg1.length());

    return scope.Close(Number::New(uid));
}

Handle<Value>
sharedHashFile::DelKeyVal(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(1);
    SHF_VALIDATE_ARG_IS_STRING(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    obj->shf->MakeHash(*arg0, arg0.length());
    uint32_t value = obj->shf->DelKeyVal();

    return scope.Close(Number::New(value));
}

Handle<Value>
sharedHashFile::DelUidVal(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(1);
    SHF_VALIDATE_ARG_IS_INT32(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    uint32_t uid = arg0;
    uint32_t value = obj->shf->DelUidVal(uid);

    return scope.Close(Number::New(value));
}

Handle<Value>
sharedHashFile::DebugGetBytesMarkedAsDeleted(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    uint64_t value = obj->shf->DebugGetBytesMarkedAsDeleted();

    return scope.Close(Number::New(value));
}

Handle<Value>
sharedHashFile::GetKeyVal(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(1);
    SHF_VALIDATE_ARG_IS_STRING(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    obj->shf->MakeHash(*arg0, arg0.length());
    if (0 == obj->shf->GetKeyValCopy()) { return scope.Close(Undefined()); }
    else                                { return scope.Close(String::New(shf_val, shf_val_len)); }
}

Handle<Value>
sharedHashFile::GetUidVal(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(1);
    SHF_VALIDATE_ARG_IS_INT32(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    int32_t uid = arg0;
    if (0 == obj->shf->GetUidValCopy(uid)) { return scope.Close(Undefined()); }
    else                                   { return scope.Close(String::New(shf_val, shf_val_len)); }
}

Handle<Value>
sharedHashFile::DebugVerbosityLess(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    obj->shf->DebugVerbosityLess();

    return scope.Close(Undefined());
}

Handle<Value>
sharedHashFile::DebugVerbosityMore(const Arguments& args) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    SHF_HANDLE_SCOPE();
    SHF_VALIDATE_ARG_COUNT_REQUIRED(0);
    SHF_GET_SHAREDHASHFILE_OBJ();

    obj->shf->DebugVerbosityLess();

    return scope.Close(Undefined());
}

void
InitAll(Handle<Object> exports) {
    SHF_DEBUG("%s()\n", __FUNCTION__);
    sharedHashFile::Init(exports);
}

NODE_MODULE(SharedHashFile, InitAll)
