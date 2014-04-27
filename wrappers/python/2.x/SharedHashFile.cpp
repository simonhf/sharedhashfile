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
#include <Python.h>
#include <SharedHashFile.hpp>
#include <string>       // std::string
#include <sstream>      // std::stringstream, std::stringbuf


class SharedHashFileSingleton
{
public:
	static SharedHashFile& getInstance()
	{
		static SharedHashFile shf;
		return shf;
	}

private:
	SharedHashFileSingleton() {};                   		// Constructor
	SharedHashFileSingleton(SharedHashFileSingleton const&);    // Don't Implement
	void operator=(SharedHashFileSingleton const&); 		// Don't implement
};

#define sharedFileHash SharedHashFileSingleton::getInstance()

static PyObject *SharedHashFileError;


static PyObject *
SharedHashFile_debugVerbosityLess(
		PyObject *self, PyObject *args
		/* none */)
{
	sharedFileHash.DebugVerbosityLess();
	return Py_BuildValue("i", 0);
}

static PyObject *
SharedHashFile_attachExisting(
		PyObject *self, PyObject *args
		/*const char* ShfFolder, const char* ShfName*/)
{
	char* shfFolder;
	char* shfName;

	if (!PyArg_ParseTuple(args, "ss:attachExisting", &shfFolder, &shfName))
	{
		return NULL;
	}

	if (!sharedFileHash.AttachExisting(
				(const char*)shfFolder, (const char*)shfName))
	{
		std::stringstream errorstream;
		errorstream << "Attaching to Existing SharedHashFile: " << shfFolder
					<< "/" << shfName << " Failed, please check if it exists.";
		PyErr_SetString(SharedHashFileError, errorstream.str().c_str());
		return NULL;
	}

	return Py_BuildValue("i", 1);
}

static PyObject *
SharedHashFile_attach(
		PyObject *self, PyObject *args
		/*const char* ShfFolder, const char* ShfName, uint32_t delete_upon_process_exit*/)
{
	char* shfFolder;
	char* shfName;
	uint32_t delete_upon_process_exit;

	if (!PyArg_ParseTuple(args, "ssI:attach", &shfFolder, &shfName, &delete_upon_process_exit))
	{
		return NULL;
	}

	if (!sharedFileHash.Attach((const char*)shfFolder, (const char*)shfName, delete_upon_process_exit))
	{
		std::stringstream errorstream;
		errorstream << "Attaching to SharedHashFile: " << shfFolder
					<< "/" << shfName << " Failed, please check permissions.";
		PyErr_SetString(SharedHashFileError, errorstream.str().c_str());
		return NULL;
	}

	return Py_BuildValue("i", 1);
}

static PyObject *
SharedHashFile_qGetName(
		PyObject *self, PyObject *args
		/*char * queueName*/)
{
	char* queueName;

	if (!PyArg_ParseTuple(args, "s:qGetName", &queueName))
	{
		return NULL;
	}

	int qid = sharedFileHash.QGetName(queueName, strlen(queueName)-1);
	return Py_BuildValue("i", qid);
}

static PyObject *
SharedHashFile_qNewName(
		PyObject *self, PyObject *args
		/*char * queueName*/)
{
	char* queueName;

	if (!PyArg_ParseTuple(args, "s:qNewName", &queueName))
	{
		return NULL;
	}

	int qid = sharedFileHash.QNewName(queueName, strlen(queueName)-1);
	return Py_BuildValue("i", qid);
}

static PyObject *
SharedHashFile_makeHash(
		PyObject *self, PyObject *args
		/*char * hashKeyName*/)
{
	char* hashKeyName;

	if (!PyArg_ParseTuple(args, "s:makeHash", &hashKeyName))
	{
		return NULL;
	}

	sharedFileHash.MakeHash(hashKeyName, strlen(hashKeyName)-1);
	return Py_BuildValue("i", 0);
}

static PyObject *
SharedHashFile_qNew(
		PyObject *self, PyObject *args
		/*uint32_t shf_qs, uint32_t shf_q_items, uint32_t shf_q_item_size, uint32_t qids_nolock_max*/)
{
	uint32_t shf_qs, shf_q_items, shf_q_item_size, qids_nolock_max;

	if (!PyArg_ParseTuple(args, "IIII:qNew", &shf_qs, &shf_q_items, &shf_q_item_size, &qids_nolock_max))
	{
		return NULL;
	}

	void* pointer = sharedFileHash.QNew(shf_qs, shf_q_items, shf_q_item_size, qids_nolock_max);
	return Py_BuildValue("I", pointer);
}

static PyObject *
SharedHashFile_qPushHeadPullTail(
		PyObject *self, PyObject *args
		/*uint32_t push_qid, uint32_t push_qiid, uint32_t pull_qid*/)
{
	uint32_t push_qid, push_qiid, pull_qid;

	if (!PyArg_ParseTuple(args, "III:qPushHeadPullTail", &push_qid, &push_qiid, &pull_qid))
	{
		return NULL;
	}

	uint32_t index = sharedFileHash.QPushHeadPullTail(push_qid, push_qiid, pull_qid);

	return Py_BuildValue("I", index);
}

static PyObject *
SharedHashFile_raceStart(
		PyObject *self, PyObject *args
		/*const char * name, uint32_t horses*/)
{
	char* name;
	uint32_t horses;

	if (!PyArg_ParseTuple(args, "sI:raceStart", &name, &horses))
	{
		return NULL;
	}

	sharedFileHash.RaceStart(name, strlen(name)-1, horses);
	return Py_BuildValue("i", 0);
}


static PyObject *
SharedHashFile_raceInit(
		PyObject *self, PyObject *args
		/*const char * name*/)
{
	char* name;

	if (!PyArg_ParseTuple(args, "s:raceInit", &name))
	{
		return NULL;
	}
	sharedFileHash.RaceInit(name, strlen(name)-1);
	return Py_BuildValue("i", 0);
}


static PyObject *
SharedHashFile_delete(
		PyObject *self, PyObject *args
		/*none*/)
{
	char * sizeInfo = sharedFileHash.Del();
	return Py_BuildValue("s", sizeInfo);
}

static PyObject *
SharedHashFile_isAttached(
		PyObject *self, PyObject *args
		/*none*/)
{
	bool attached = sharedFileHash.IsAttached();
	return Py_BuildValue("b", attached);
}

//static v8::Handle<v8::Value> New               (const v8::Arguments& args);

//static v8::Handle<v8::Value> Uid               (const v8::Arguments& args);
//static v8::Handle<v8::Value> MakeHash          (const v8::Arguments& args);
//static v8::Handle<v8::Value> DelKeyVal         (const v8::Arguments& args);
//static v8::Handle<v8::Value> DelUidVal         (const v8::Arguments& args);
//static v8::Handle<v8::Value> PutKeyVal         (const v8::Arguments& args);
//static v8::Handle<v8::Value> GetKeyVal         (const v8::Arguments& args);
//static v8::Handle<v8::Value> GetUidVal         (const v8::Arguments& args);
//static v8::Handle<v8::Value> DebugGetGarbage   (const v8::Arguments& args);
//static v8::Handle<v8::Value> DebugVerbosityMore(const v8::Arguments& args);
//static v8::Handle<v8::Value> SetDataNeedFactor (const v8::Arguments& args);
//static v8::Handle<v8::Value> QNew              (const v8::Arguments& args);
//static v8::Handle<v8::Value> QGet              (const v8::Arguments& args);
//static v8::Handle<v8::Value> QDel              (const v8::Arguments& args);
//static v8::Handle<v8::Value> QPushHead         (const v8::Arguments& args);
//static v8::Handle<v8::Value> QPullTail         (const v8::Arguments& args);
//static v8::Handle<v8::Value> QTakeItem         (const v8::Arguments& args);

static PyMethodDef SharedHashFileMethods[] = {
    {"debugVerbosityLess",  SharedHashFile_debugVerbosityLess, METH_VARARGS,
     "Set minimal Debug level."},
    {"attachExisting",  SharedHashFile_attachExisting, METH_VARARGS,
     "Attach to existing hash file."},
	{"attach",  SharedHashFile_attach, METH_VARARGS,
	 "Attach to new hash file."},
	{"makeHash",  SharedHashFile_makeHash, METH_VARARGS,
	  "Make new hash to be added to queues."},
	{"qNew",  SharedHashFile_qNew, METH_VARARGS,
	  "Make a new queue."},
	{"qGetName",  SharedHashFile_qGetName, METH_VARARGS,
	  "Unique ID for hash file queue name."},
	{"qNewName",  SharedHashFile_qNewName, METH_VARARGS,
	  "Set new hash file queue name."},
	{"qPushHeadPullTail",  SharedHashFile_qPushHeadPullTail, METH_VARARGS,
	  "Attach to head and pull tail of hash file."},
	{"raceStart",  SharedHashFile_raceStart, METH_VARARGS,
	  "Sync processes."},
	{"raceInit",  SharedHashFile_raceInit, METH_VARARGS,
	  "Initialize process race."},
	{"delete",  SharedHashFile_delete, METH_VARARGS,
	  "Delete the created shared hash file."},
	{"isAttached",  SharedHashFile_isAttached, METH_VARARGS,
	  "Check to see if shared hash file attached."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

PyMODINIT_FUNC
initSharedHashFile(void)
{
    PyObject *m;

    m = Py_InitModule("SharedHashFile", SharedHashFileMethods);
    if (m == NULL)
        return;

    SharedHashFileError = PyErr_NewException(const_cast<char *>("SharedHashFile.error"), NULL, NULL);
    Py_INCREF(SharedHashFileError);
    PyModule_AddObject(m, "error", SharedHashFileError);
}
