/* 
 */

#include <Python.h>
#include "minilzo.h"

/* Ensure we have updated versions */
#if !defined(PY_VERSION_HEX) || (PY_VERSION_HEX < 0x010502f0)
#  error "Need Python version 1.5.2 or greater"
#endif

#undef UNUSED
#define UNUSED(var)     ((void)&var)

static PyObject *LzoError;

#define BLOCK_SIZE        (256*1024l)


#define M_LZO1X_1 1
#define M_LZO1X_1_15 2
#define M_LZO1X_999 3

static /* const */ char decompress__doc__[] =
"decompress(string) -- Decompress the data in string, returning a string containing the decompressed data.\n"
;


static PyObject *
compress_block(PyObject *dummy, PyObject *args)
{
  PyObject *result;
  lzo_voidp wrkmem = NULL;
  const lzo_bytep in;
  lzo_uint in_len;

  lzo_bytep out;
  lzo_uint out_len;
  lzo_uint new_len;

  int len;
  int wrk_len;
  int level;
  int method;
  int err;

  UNUSED(dummy);

  if (!PyArg_ParseTuple(args, "s#II", &in, &in_len, &method, &level))
    return NULL;

  out_len = in_len + in_len / 64 + 16 + 3;

  result = PyString_FromStringAndSize(NULL, out_len);

  if (result == NULL)
    return PyErr_NoMemory();

  if (method == M_LZO1X_1)
      wrk_len = LZO1X_1_MEM_COMPRESS;
#ifdef USE_LIBLZO
  else if (method == M_LZO1X_1_15)
      wrk_len = LZO1X_1_15_MEM_COMPRESS;
  else if (method == M_LZO1X_999)
      wrk_len = LZO1X_999_MEM_COMPRESS;
#endif
  else
      wrk_len = 0;
  assert(wrk_len <= WRK_LEN);

  wrkmem = (lzo_voidp) PyMem_Malloc(wrk_len);

  out = (lzo_bytep) PyString_AsString(result);
  
  if (method == M_LZO1X_1){
    err = lzo1x_1_compress(in, in_len, out, &new_len, wrkmem);
  }
#ifdef USE_LIBLZO
  else if (method == M_LZO1X_1_15){
    err = lzo1x_1_15_compress(in, in_len,
                                    out, &new_len, wrkmem);
  }
  else if (method == M_LZO1X_999){
    err = lzo1x_999_compress_level(in, in_len,
                                         out, &new_len, wrkmem,
                                         NULL, 0, 0, level);
  }
#endif
  else{
    PyMem_Free(wrkmem);
    Py_DECREF(result);
    PyErr_SetString(LzoError, "internal error - method not supported");
    return NULL;
  }

  PyMem_Free(wrkmem);
  if (err != LZO_E_OK || new_len > out_len)
  {
    /* this should NEVER happen */
    Py_DECREF(result);
    PyErr_Format(LzoError, "Error %i while compressing data", err);
    return NULL;
  }


  if (new_len != out_len)
    _PyString_Resize(&result, new_len);

  return result;

}

static PyObject *
decompress_block(PyObject *dummy, PyObject *args)
{
  PyObject *result;
  lzo_uint len;
  int err;
  lzo_bytep out;

  lzo_bytep in;
  lzo_uint in_len;
  lzo_uint dst_len;
  UNUSED(dummy);
  
  //should dst_len be uint?
  if (!PyArg_ParseTuple(args, "s#I", &in, &in_len, &dst_len))
    return NULL;

  result = PyBytes_FromStringAndSize(NULL, dst_len);
  if (result == NULL) {
    return NULL;
  }

    
  out = (lzo_bytep) PyBytes_AS_STRING(result);

  err = lzo1x_decompress_safe(in, in_len, out, &len, NULL);

  //r = lzo1x_decompress_safe(fin, 690, out, &len, NULL);
  if (err != LZO_E_OK){
      Py_DECREF(result);
      result = NULL;
      PyErr_SetString(LzoError, "internal error - decompression failed");
      return NULL;
  }
  if (len != dst_len){
    return NULL;
  }

  return result;

}

static PyObject *
py_lzo_adler32(PyObject *dummy, PyObject *args)
{
  lzo_uint32 value;
  lzo_bytep in;
  lzo_uint32 len;

  lzo_uint32 new;

  if (!PyArg_ParseTuple(args, "Is#", &value, &in, &len))
    return NULL;

  if(len>0){
    new = lzo_adler32(value, (const lzo_bytep)in, len);
  }

  return Py_BuildValue("I", new);
}

#ifdef USE_LIBLZO
static PyObject *
py_lzo_crc32(PyObject *dummy, PyObject *args)
{


  lzo_uint32 value;
  lzo_bytep in;
  lzo_uint32 len;

  lzo_uint32 new;

  if (!PyArg_ParseTuple(args, "Is#", &value, &in, &len))
    return NULL;
  
  if(len>0){
    new = lzo_crc32(value, (const lzo_bytep)in, len);
  }

  return Py_BuildValue("I", new);
}
#endif

/***********************************************************************
// main
************************************************************************/

static /* const */ PyMethodDef methods[] =
{
    {"compress_block", (PyCFunction)compress_block, METH_VARARGS, decompress__doc__},
    {"decompress_block", (PyCFunction)decompress_block, METH_VARARGS, decompress__doc__},
    {"lzo_adler32", (PyCFunction)py_lzo_adler32, METH_VARARGS, decompress__doc__},
#ifdef USE_LIBLZO
    {"lzo_crc32", (PyCFunction)py_lzo_crc32, METH_VARARGS, decompress__doc__},
#endif
    {NULL, NULL, 0, NULL}
};


static /* const */ char module_documentation[]=
"The functions in this module allow compression and decompression "
"using the LZO library.\n\n"

;


#ifdef _MSC_VER
_declspec(dllexport)
#endif
void init_lzo(void)
{
    PyObject *m, *d, *v;
    if (lzo_init() != LZO_E_OK)
    {
        return;
    }

    m = Py_InitModule4("_lzo", methods, module_documentation,
                       NULL, PYTHON_API_VERSION);
    d = PyModule_GetDict(m);

    LzoError = PyErr_NewException("_lzo.error", NULL, NULL);
    PyDict_SetItemString(d, "error", LzoError);

    v = PyString_FromString("<iridiummx@gmail.com>");
    PyDict_SetItemString(d, "__author__", v);
    Py_DECREF(v);

    v = PyInt_FromLong(LZO_VERSION);
    PyDict_SetItemString(d, "LZO_VERSION", v);
    Py_DECREF(v);
    v = PyString_FromString(LZO_VERSION_STRING);
    PyDict_SetItemString(d, "LZO_VERSION_STRING", v);
    Py_DECREF(v);
    v = PyString_FromString(LZO_VERSION_DATE);
    PyDict_SetItemString(d, "LZO_VERSION_DATE", v);
    Py_DECREF(v);
}


/*
vi:ts=4:et
*/
