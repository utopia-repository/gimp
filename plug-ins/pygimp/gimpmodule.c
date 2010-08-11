/* -*- Mode: C; c-basic-offset: 4 -*-
    Gimp-Python - allows the writing of Gimp plugins in Python.
    Copyright (C) 1997-2002  James Henstridge <james@daa.com.au>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "pygimp.h"

#define _INSIDE_PYGIMP_
#include "pygimp-api.h"

#include <sysmodule.h>

#if PY_VERSION_HEX >= 0x2030000
#define ARG_UINT_FORMAT "I"
#else
#define ARG_UINT_FORMAT "i"
#endif

/* maximum bits per pixel ... */
#define MAX_BPP 4

PyObject *pygimp_error;

#ifndef PG_DEBUG
# define PG_DEBUG 0
#endif


/* End of code for pdbFunc objects */
/* -------------------------------------------------------- */

GimpPlugInInfo PLUG_IN_INFO = {
    NULL, /* init_proc */
    NULL, /* quit_proc */
    NULL, /* query_proc */
    NULL  /* run_proc */
};

static PyObject *callbacks[] = {
    NULL, NULL, NULL, NULL
};

typedef struct _ProgressData ProgressData;

struct _ProgressData
{
  PyObject *start, *end, *text, *value;
  PyObject *user_data;
};


static void
pygimp_init_proc(void)
{
    PyObject *r;

    r = PyObject_CallFunction(callbacks[0], "()");

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static void
pygimp_quit_proc(void)
{
    PyObject *r;

    r = PyObject_CallFunction(callbacks[1], "()");

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static void
pygimp_query_proc(void)
{
    PyObject *r;

    r = PyObject_CallFunction(callbacks[2], "()");

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static void
pygimp_run_proc(const char *name, int nparams, const GimpParam *params,
		int *nreturn_vals, GimpParam **return_vals)
{
    PyObject *args, *ret;
    GimpParamDef *pd, *rv;
    GimpPDBProcType t;
    char *b, *h, *a, *c, *d;
    int np, nrv;

    gimp_procedural_db_proc_info(name, &b, &h, &a, &c, &d, &t, &np, &nrv,
				 &pd, &rv);
    g_free(b); g_free(h); g_free(a); g_free(c); g_free(d); g_free(pd);

#if PG_DEBUG > 0
    fprintf(stderr, "Params for %s:", name);
    print_GParam(nparams, params);
#endif

    args = pygimp_param_to_tuple(nparams, params);

    if (args == NULL) {
	PyErr_Clear();

	*nreturn_vals = 1;
	*return_vals = g_new(GimpParam, 1);
	(*return_vals)[0].type = GIMP_PDB_STATUS;
	(*return_vals)[0].data.d_status = GIMP_PDB_CALLING_ERROR;

	return;
    }

    ret = PyObject_CallFunction(callbacks[3], "(sO)", name, args);
    Py_DECREF(args);

    if (ret == NULL) {
	PyErr_Print();
	PyErr_Clear();

	*nreturn_vals = 1;
	*return_vals = g_new(GimpParam, 1);
	(*return_vals)[0].type = GIMP_PDB_STATUS;
	(*return_vals)[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

	return;
    }

    *return_vals = pygimp_param_from_tuple(ret, rv, nrv);
    g_free(rv);

    if (*return_vals == NULL) {
	PyErr_Clear();

	*nreturn_vals = 1;
	*return_vals = g_new(GimpParam, 1);
	(*return_vals)[0].type = GIMP_PDB_STATUS;
	(*return_vals)[0].data.d_status = GIMP_PDB_EXECUTION_ERROR;

	return;
    }

    Py_DECREF(ret);

    *nreturn_vals = nrv + 1;
    (*return_vals)[0].type = GIMP_PDB_STATUS;
    (*return_vals)[0].data.d_status = GIMP_PDB_SUCCESS;
}

static PyObject *
pygimp_main(PyObject *self, PyObject *args)
{
    PyObject *av;
    int argc, i;
    char **argv;
    PyObject *ip, *qp, *query, *rp;
	
    if (!PyArg_ParseTuple(args, "OOOO:main", &ip, &qp, &query, &rp))
	return NULL;

#define Arg_Check(v) (PyCallable_Check(v) || (v) == Py_None)

    if (!Arg_Check(ip) || !Arg_Check(qp) || !Arg_Check(query) ||
	!Arg_Check(rp)) {
	PyErr_SetString(pygimp_error, "arguments must be callable");
	return NULL;
    }

#undef Arg_Check

    if (query == Py_None) {
	PyErr_SetString(pygimp_error, "a query procedure must be provided");
	return NULL;
    }

    if (ip != Py_None) {
	callbacks[0] = ip;
	PLUG_IN_INFO.init_proc = pygimp_init_proc;
    }

    if (qp != Py_None) {
	callbacks[1] = qp;
	PLUG_IN_INFO.quit_proc = pygimp_quit_proc;
    }

    if (query != Py_None) {
	callbacks[2] = query;
	PLUG_IN_INFO.query_proc = pygimp_query_proc;
    }

    if (rp != Py_None) {
	callbacks[3] = rp;
	PLUG_IN_INFO.run_proc = pygimp_run_proc;
    }

    av = PySys_GetObject("argv");

    argc = PyList_Size(av);
    argv = g_new(char *, argc);

    for (i = 0; i < argc; i++)
	argv[i] = g_strdup(PyString_AsString(PyList_GetItem(av, i)));

    gimp_main(&PLUG_IN_INFO, argc, argv);

    if (argv != NULL) {
	for (i = 0; i < argc; i++)
	    if (argv[i] != NULL)
		g_free(argv[i]);

	g_free(argv);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_quit(PyObject *self)
{
    gimp_quit();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_message(PyObject *self, PyObject *args)
{
    char *msg;

    if (!PyArg_ParseTuple(args, "s:message", &msg))
        return NULL;

    gimp_message(msg);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_set_data(PyObject *self, PyObject *args)
{
    char *id, *data;
    int bytes, nreturn_vals;
    GimpParam *return_vals;

    if (!PyArg_ParseTuple(args, "ss#:set_data", &id, &data, &bytes))
	return NULL;

    return_vals = gimp_run_procedure("gimp-procedural-db-set-data",
				     &nreturn_vals,
				     GIMP_PDB_STRING, id,
				     GIMP_PDB_INT32, bytes,
				     GIMP_PDB_INT8ARRAY, data,
				     GIMP_PDB_END);

    if (return_vals[0].data.d_status != GIMP_PDB_SUCCESS) {
	PyErr_SetString(pygimp_error, "error occurred while storing");
	return NULL;
    }

    gimp_destroy_params(return_vals, nreturn_vals);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_get_data(PyObject *self, PyObject *args)
{
    char *id;
    int nreturn_vals;
    GimpParam *return_vals;
    PyObject *s;

    if (!PyArg_ParseTuple(args, "s:get_data", &id))
	return NULL;

    return_vals = gimp_run_procedure("gimp-procedural-db-get-data",
				     &nreturn_vals,
				     GIMP_PDB_STRING, id,
				     GIMP_PDB_END);

    if (return_vals[0].data.d_status != GIMP_PDB_SUCCESS) {
	PyErr_SetString(pygimp_error, "no data for id");
	return NULL;
    }

    s = PyString_FromStringAndSize((char *)return_vals[2].data.d_int8array,
				   return_vals[1].data.d_int32);
    gimp_destroy_params(return_vals, nreturn_vals);

    return s;
}

static PyObject *
pygimp_progress_init(PyObject *self, PyObject *args)
{
    char *msg = NULL;

    if (!PyArg_ParseTuple(args, "|s:progress_init", &msg))
	return NULL;

    gimp_progress_init(msg);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_progress_update(PyObject *self, PyObject *args)
{
    double p;

    if (!PyArg_ParseTuple(args, "d:progress_update", &p))
	return NULL;

    gimp_progress_update(p);

    Py_INCREF(Py_None);
    return Py_None;
}

static void
pygimp_progress_start(const gchar *message, gboolean cancelable, gpointer data)
{
    ProgressData *pdata = data;
    PyObject *r;

    if (pdata->user_data) {
	r = PyObject_CallFunction(pdata->start, "siO", message, cancelable,
				  pdata->user_data);
	Py_DECREF(pdata->user_data);
    } else
	r = PyObject_CallFunction(pdata->start, "si", message, cancelable);

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static void
pygimp_progress_end(gpointer data)
{
    ProgressData *pdata = data;
    PyObject *r;

    if (pdata->user_data) {
	r = PyObject_CallFunction(pdata->end, "O", pdata->user_data);
	Py_DECREF(pdata->user_data);
    } else
	r = PyObject_CallFunction(pdata->end, NULL);

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static void
pygimp_progress_text(const gchar *message, gpointer data)
{
    ProgressData *pdata = data;
    PyObject *r;

    if (pdata->user_data) {
	r = PyObject_CallFunction(pdata->text, "sO", message, pdata->user_data);
	Py_DECREF(pdata->user_data);
    } else
	r = PyObject_CallFunction(pdata->text, "s", message);

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static void
pygimp_progress_value(gdouble percentage, gpointer data)
{
    ProgressData *pdata = data;
    PyObject *r;

    if (pdata->user_data) {
	r = PyObject_CallFunction(pdata->value, "dO", percentage,
				  pdata->user_data);
	Py_DECREF(pdata->user_data);
    } else
	r = PyObject_CallFunction(pdata->value, "d", percentage);

    if (!r) {
	PyErr_Print();
	PyErr_Clear();
	return;
    }

    Py_DECREF(r);
}

static PyObject *
pygimp_progress_install(PyObject *self, PyObject *args, PyObject *kwargs)
{
    GimpProgressVtable vtable = { 0, };
    const gchar *ret;
    ProgressData *pdata;
    static char *kwlist[] = { "start", "end", "text", "value", "data", NULL };

    pdata = g_new0(ProgressData, 1);

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "OOOO|O:progress_install",
				     kwlist,
				     &pdata->start, &pdata->end,
				     &pdata->text, &pdata->value,
				     &pdata->user_data))
	goto cleanup;

#define PROCESS_FUNC(n) G_STMT_START {					\
    if (!PyCallable_Check(pdata->n)) {					\
	PyErr_SetString(pygimp_error, #n "argument must be callable");	\
	goto cleanup;							\
    }									\
    Py_INCREF(pdata->n);						\
} G_STMT_END

    PROCESS_FUNC(start);
    PROCESS_FUNC(end);
    PROCESS_FUNC(text);
    PROCESS_FUNC(value);

    Py_XINCREF(pdata->user_data);

#undef PROCESS_FUNC

    vtable.start     = pygimp_progress_start;
    vtable.end       = pygimp_progress_end;
    vtable.set_text  = pygimp_progress_text;
    vtable.set_value = pygimp_progress_value;

    ret = gimp_progress_install_vtable(&vtable, pdata);

    if (!ret) {
	PyErr_SetString(pygimp_error,
			"error occurred while installing progress functions");

	Py_DECREF(pdata->start);
	Py_DECREF(pdata->end);
	Py_DECREF(pdata->text);
	Py_DECREF(pdata->value);

	goto cleanup;
    }

    return PyString_FromString(ret);

cleanup:
    g_free(pdata);
    return NULL;
}

static PyObject *
pygimp_progress_uninstall(PyObject *self, PyObject *args)
{
    ProgressData *pdata;
    gchar *callback;

    if (!PyArg_ParseTuple(args, "s:progress_uninstall", &callback))
	return NULL;

    pdata = gimp_progress_uninstall(callback);

    if (!pdata) {
	PyErr_SetString(pygimp_error,
			"error occurred while uninstalling progress functions");
	return NULL;
    }

    Py_DECREF(pdata->start);
    Py_DECREF(pdata->end);
    Py_DECREF(pdata->text);
    Py_DECREF(pdata->value);

    Py_XDECREF(pdata->user_data);

    g_free(pdata);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_image_list(PyObject *self)
{
    gint32 *imgs;
    int nimgs, i;
    PyObject *ret;

    imgs = gimp_image_list(&nimgs);
    ret = PyList_New(nimgs);

    for (i = 0; i < nimgs; i++)
	PyList_SetItem(ret, i, (PyObject *)pygimp_image_new(imgs[i]));

    g_free(imgs);

    return ret;
}

static PyObject *
pygimp_install_procedure(PyObject *self, PyObject *args)
{
    char *name, *blurb, *help, *author, *copyright, *date, *menu_path,
	 *image_types, *n, *d;
    GimpParamDef *params, *return_vals;
    int type, nparams, nreturn_vals, i;
    PyObject *pars, *rets;

    if (!PyArg_ParseTuple(args, "sssssszziOO:install_procedure",
			  &name, &blurb, &help,
			  &author, &copyright, &date, &menu_path, &image_types,
			  &type, &pars, &rets))
	return NULL;

    if (!PySequence_Check(pars) || !PySequence_Check(rets)) {
	PyErr_SetString(PyExc_TypeError, "last two args must be sequences");
	return NULL;
    }

    nparams = PySequence_Length(pars);
    nreturn_vals = PySequence_Length(rets);
    params = g_new(GimpParamDef, nparams);

    for (i = 0; i < nparams; i++) {
	if (!PyArg_ParseTuple(PySequence_GetItem(pars, i), "iss",
			      &(params[i].type), &n, &d)) {
	    g_free(params);
	    return NULL;
	}

	params[i].name = g_strdup(n);
	params[i].description = g_strdup(d);
    }

    return_vals = g_new(GimpParamDef, nreturn_vals);

    for (i = 0; i < nreturn_vals; i++) {
	if (!PyArg_ParseTuple(PySequence_GetItem(rets, i), "iss",
			      &(return_vals[i].type), &n, &d)) {
	    g_free(params); g_free(return_vals);
	    return NULL;
	}

	return_vals[i].name = g_strdup(n);
	return_vals[i].description = g_strdup(d);
    }

    gimp_install_procedure(name, blurb, help, author, copyright, date,
			   menu_path, image_types, type, nparams, nreturn_vals,
			   params, return_vals);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_install_temp_proc(PyObject *self, PyObject *args)
{
    char *name, *blurb, *help, *author, *copyright, *date, *menu_path,
	*image_types, *n, *d;
    GimpParamDef *params, *return_vals;
    int type, nparams, nreturn_vals, i;
    PyObject *pars, *rets;

    if (!PyArg_ParseTuple(args, "sssssszziOO:install_temp_proc",
			  &name, &blurb, &help,
			  &author, &copyright, &date, &menu_path, &image_types,
			  &type, &pars, &rets))
	return NULL;

    if (!PySequence_Check(pars) || !PySequence_Check(rets)) {
	PyErr_SetString(PyExc_TypeError, "last two args must be sequences");
	return NULL;
    }

    nparams = PySequence_Length(pars);
    nreturn_vals = PySequence_Length(rets);
    params = g_new(GimpParamDef, nparams);

    for (i = 0; i < nparams; i++) {
	if (!PyArg_ParseTuple(PySequence_GetItem(pars, i), "iss",
			      &(params[i].type), &n, &d)) {
	    g_free(params);
	    return NULL;
	}

	params[i].name = g_strdup(n);
	params[i].description = g_strdup(d);
    }

    return_vals = g_new(GimpParamDef, nreturn_vals);

    for (i = 0; i < nreturn_vals; i++) {
	if (!PyArg_ParseTuple(PySequence_GetItem(rets, i), "iss",
			      &(return_vals[i].type), &n, &d)) {
	    g_free(params); g_free(return_vals);
	    return NULL;
	}

	return_vals[i].name = g_strdup(n);
	return_vals[i].description = g_strdup(d);
    }

    gimp_install_temp_proc(name, blurb, help, author, copyright, date,
			   menu_path, image_types, type,
			   nparams, nreturn_vals, params, return_vals,
			   pygimp_run_proc);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_uninstall_temp_proc(PyObject *self, PyObject *args)
{
    char *name;

    if (!PyArg_ParseTuple(args, "s:uninstall_temp_proc", &name))
	return NULL;

    gimp_uninstall_temp_proc(name);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_register_magic_load_handler(PyObject *self, PyObject *args)
{
    char *name, *extensions, *prefixes, *magics;

    if (!PyArg_ParseTuple(args, "ssss:register_magic_load_handler",
			  &name, &extensions, &prefixes, &magics))
	return NULL;

    gimp_register_magic_load_handler(name, extensions, prefixes, magics);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_register_load_handler(PyObject *self, PyObject *args)
{
    char *name, *extensions, *prefixes;

    if (!PyArg_ParseTuple(args, "sss:register_load_handler",
			  &name, &extensions, &prefixes))
	return NULL;

    gimp_register_load_handler(name, extensions, prefixes);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_register_save_handler(PyObject *self, PyObject *args)
{
    char *name, *extensions, *prefixes;

    if (!PyArg_ParseTuple(args, "sss:register_save_handler",
			  &name, &extensions, &prefixes))
	return NULL;

    gimp_register_save_handler(name, extensions, prefixes);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_gamma(PyObject *self)
{
    return PyFloat_FromDouble(gimp_gamma());
}

static PyObject *
pygimp_install_cmap(PyObject *self)
{
    return PyBool_FromLong(gimp_install_cmap());
}

static PyObject *
pygimp_min_colors(PyObject *self)
{
    return PyInt_FromLong(gimp_min_colors());
}

static PyObject *
pygimp_gtkrc(PyObject *self)
{
    return PyString_FromString(gimp_gtkrc());
}

static PyObject *
pygimp_personal_rc_file(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char *basename, *filename;
    PyObject *ret;

    static char *kwlist[] = { "basename", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
				     "s:personal_rc_file", kwlist,
				     &basename))
	return NULL;

    filename = gimp_personal_rc_file(basename);
    ret = PyString_FromString(filename);
    g_free(filename);

    return ret;
}

static PyObject *
pygimp_get_background(PyObject *self)
{
    GimpRGB colour;
    guchar r, g, b;

    gimp_context_get_background(&colour);
    gimp_rgb_get_uchar(&colour, &r, &g, &b);

    return Py_BuildValue("(iii)", (int)r, (int)g, (int)b);
}

static PyObject *
pygimp_get_foreground(PyObject *self)
{
    GimpRGB colour;
    guchar r, g, b;

    gimp_context_get_foreground(&colour);
    gimp_rgb_get_uchar(&colour, &r, &g, &b);

    return Py_BuildValue("(iii)", (int)r, (int)g, (int)b);
}

static PyObject *
pygimp_set_background(PyObject *self, PyObject *args)
{
    GimpRGB colour;
    int r, g, b;

    if (!PyArg_ParseTuple(args, "(iii):set_background", &r, &g, &b)) {
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "iii:set_background", &r, &g, &b))
	    return NULL;
    }

    r = CLAMP(r, 0, 255);
    g = CLAMP(g, 0, 255);
    b = CLAMP(b, 0, 255);

    gimp_rgb_set_uchar(&colour, r, g, b);
    gimp_context_set_background(&colour);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_set_foreground(PyObject *self, PyObject *args)
{
    GimpRGB colour;
    int r, g, b;

    if (!PyArg_ParseTuple(args, "(iii):set_foreground", &r, &g, &b)) {
	PyErr_Clear();
	if (!PyArg_ParseTuple(args, "iii:set_foreground", &r, &g, &b))
	    return NULL;
    }

    r = CLAMP(r, 0, 255);
    g = CLAMP(g, 0, 255);
    b = CLAMP(b, 0, 255);

    gimp_rgb_set_uchar(&colour, r, g, b);
    gimp_context_set_foreground(&colour);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_gradients_get_list(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char **list, *filter = NULL;
    int num, i;
    PyObject *ret;

    static char *kwlist[] = { "filter", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
				     "|s:gradients_get_list", kwlist,
				     &filter))
	return NULL;

    list = gimp_gradients_get_list(filter, &num);

    ret = PyList_New(num);

    for (i = 0; i < num; i++) {
	PyList_SetItem(ret, i, PyString_FromString(list[i]));
	g_free(list[i]);
    }

    g_free(list);

    return ret;
}

static PyObject *
pygimp_context_get_gradient(PyObject *self)
{
    char *name;
    PyObject *ret;

    name = gimp_context_get_gradient();
    ret = PyString_FromString(name);
    g_free(name);

    return ret;
}

static PyObject *
pygimp_gradients_get_gradient(PyObject *self)
{
    if (PyErr_Warn(PyExc_DeprecationWarning, "use gimp.context_get_gradient") < 0)
        return NULL;

    return pygimp_context_get_gradient(self);
}

static PyObject *
pygimp_context_set_gradient(PyObject *self, PyObject *args)
{
    char *actv;

    if (!PyArg_ParseTuple(args, "s:gradients_set_gradient", &actv))
	return NULL;

    gimp_context_set_gradient(actv);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_gradients_set_gradient(PyObject *self, PyObject *args)
{
    if (PyErr_Warn(PyExc_DeprecationWarning, "use gimp.context_set_gradient") < 0)
        return NULL;

    return pygimp_context_set_gradient(self, args);
}

static PyObject *
pygimp_gradient_get_uniform_samples(PyObject *self, PyObject *args)
{
    int num, reverse = FALSE;
    char *name;
    int nsamp;
    double *samp;
    int i, j;
    PyObject *ret;

    if (!PyArg_ParseTuple(args, "si|i:gradient_get_uniform_samples",
			  &name, &num, &reverse))
	return NULL;

    if (!gimp_gradient_get_uniform_samples(name, num, reverse, &nsamp, &samp)) {
	PyErr_SetString(pygimp_error, "gradient_get_uniform_samples failed");
	return NULL;
    }

    ret = PyList_New(num);
    for (i = 0, j = 0; i < num; i++, j += 4)
	PyList_SetItem(ret, i, Py_BuildValue("(dddd)", samp[j],
					     samp[j+1], samp[j+2], samp[j+3]));

    g_free(samp);

    return ret;
}

static PyObject *
pygimp_gradient_get_custom_samples(PyObject *self, PyObject *args)
{
    int num, reverse = FALSE;
    char *name;
    int nsamp;
    double *pos, *samp;
    int i, j;
    PyObject *ret, *item;
    gboolean success;

    if (!PyArg_ParseTuple(args, "sO|i:gradient_get_custom_samples",
			  &name, &ret, &reverse))
	return NULL;

    if (!PySequence_Check(ret)) {
	PyErr_SetString(PyExc_TypeError,
			"second arg must be a sequence");
	return NULL;
    }

    num = PySequence_Length(ret);
    pos = g_new(gdouble, num);

    for (i = 0; i < num; i++) {
	item = PySequence_GetItem(ret, i);

	if (!PyFloat_Check(item)) {
	    PyErr_SetString(PyExc_TypeError,
			    "second arg must be a sequence of floats");
	    g_free(pos);
	    return NULL;
	}

	pos[i] = PyFloat_AsDouble(item);
    }

    success = gimp_gradient_get_custom_samples(name, num, pos, reverse,
					       &nsamp, &samp);
    g_free(pos);

    if (!success) {
	PyErr_SetString(pygimp_error, "gradient_get_custom_samples failed");
	return NULL;
    }

    ret = PyList_New(num);
    for (i = 0, j = 0; i < num; i++, j += 4)
	PyList_SetItem(ret, i, Py_BuildValue("(dddd)", samp[j],
					     samp[j+1], samp[j+2], samp[j+3]));

    g_free(samp);

    return ret;
}

static PyObject *
pygimp_gradients_sample_uniform(PyObject *self, PyObject *args)
{
    char *name;
    PyObject *arg_list, *str, *new_args, *ret;

    if (PyErr_Warn(PyExc_DeprecationWarning,
		   "use gimp.gradient_get_uniform_samples") < 0)
        return NULL;

    arg_list = PySequence_List(args);

    name = gimp_context_get_gradient();

    str = PyString_FromString(name);
    g_free(name);

    PyList_Insert(arg_list, 0, str);
    Py_XDECREF(str);

    new_args = PyList_AsTuple(arg_list);
    Py_XDECREF(arg_list);

    ret = pygimp_gradient_get_uniform_samples(self, new_args);
    Py_XDECREF(new_args);

    return ret;
}

static PyObject *
pygimp_gradients_sample_custom(PyObject *self, PyObject *args)
{
    char *name;
    PyObject *arg_list, *str, *new_args, *ret;

    if (PyErr_Warn(PyExc_DeprecationWarning,
		   "use gimp.gradient_get_custom_samples") < 0)
        return NULL;

    arg_list = PySequence_List(args);

    name = gimp_context_get_gradient();

    str = PyString_FromString(name);
    g_free(name);

    PyList_Insert(arg_list, 0, str);
    Py_XDECREF(str);

    new_args = PyList_AsTuple(arg_list);
    Py_XDECREF(arg_list);

    ret = pygimp_gradient_get_custom_samples(self, new_args);

    return ret;
}

static PyObject *
pygimp_delete(PyObject *self, PyObject *args)
{
    PyGimpImage *img;

    if (!PyArg_ParseTuple(args, "O:delete", &img))
	return NULL;

    if (pygimp_image_check(img))
        gimp_image_delete(img->ID);
    else if (pygimp_drawable_check(img))
        gimp_drawable_delete(img->ID);
    else if (pygimp_display_check(img))
        gimp_display_delete(img->ID);

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
pygimp_displays_flush(PyObject *self)
{
    gimp_displays_flush();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_displays_reconnect(PyObject *self, PyObject *args)
{
    PyGimpImage *old_img, *new_img;

    if (!PyArg_ParseTuple(args, "O!O!:displays_reconnect",
			  &PyGimpImage_Type, &old_img,
			  &PyGimpImage_Type, &new_img))
	return NULL;

    if (!gimp_displays_reconnect (old_img->ID, new_img->ID)) {
	PyErr_Format(pygimp_error,
		     "could not reconnect the displays of image (ID %d) "
		     "to image (ID %d)",
		     old_img->ID, new_img->ID);
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_tile_cache_size(PyObject *self, PyObject *args)
{
    unsigned long k;

    if (!PyArg_ParseTuple(args, "l:tile_cache_size", &k))
	return NULL;

    gimp_tile_cache_size(k);

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
pygimp_tile_cache_ntiles(PyObject *self, PyObject *args)
{
    unsigned long n;

    if (!PyArg_ParseTuple(args, "l:tile_cache_ntiles", &n))
	return NULL;

    gimp_tile_cache_ntiles(n);

    Py_INCREF(Py_None);
    return Py_None;
}


static PyObject *
pygimp_tile_width(PyObject *self)
{
    return PyInt_FromLong(gimp_tile_width());
}


static PyObject *
pygimp_tile_height(PyObject *self)
{
    return PyInt_FromLong(gimp_tile_height());
}

static PyObject *
pygimp_extension_ack(PyObject *self)
{
    gimp_extension_ack();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_extension_enable(PyObject *self)
{
    gimp_extension_enable();

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_extension_process(PyObject *self, PyObject *args)
{
    guint timeout;

    if (!PyArg_ParseTuple(args, ARG_UINT_FORMAT ":extension_process", &timeout))
	return NULL;

    gimp_extension_process(timeout);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_parasite_find(PyObject *self, PyObject *args)
{
    char *name;

    if (!PyArg_ParseTuple(args, "s:parasite_find", &name))
	return NULL;

    return pygimp_parasite_new(gimp_parasite_find(name));
}

static PyObject *
pygimp_parasite_attach(PyObject *self, PyObject *args)
{
    PyGimpParasite *parasite;

    if (!PyArg_ParseTuple(args, "O!:parasite_attach",
			  &PyGimpParasite_Type, &parasite))
	return NULL;

    if (!gimp_parasite_attach(parasite->para)) {
	PyErr_Format(pygimp_error, "could not attach parasite '%s'",
		     gimp_parasite_name(parasite->para));
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_attach_new_parasite(PyObject *self, PyObject *args)
{
    char *name, *data;
    int flags, size;

    if (!PyArg_ParseTuple(args, "sis#:attach_new_parasite", &name, &flags,
			  &data, &size))
	return NULL;

    if (!gimp_attach_new_parasite(name, flags, size, data)) {
	PyErr_Format(pygimp_error, "could not attach new parasite '%s'", name);
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_parasite_detach(PyObject *self, PyObject *args)
{
    char *name;

    if (!PyArg_ParseTuple(args, "s:parasite_detach", &name))
	return NULL;

    if (!gimp_parasite_detach(name)) {
	PyErr_Format(pygimp_error, "could not detach parasite '%s'", name);
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_parasite_list(PyObject *self)
{
    gint num_parasites;
    gchar **parasites;

    if (gimp_parasite_list(&num_parasites, &parasites)) {
	PyObject *ret;
	gint i;

	ret = PyTuple_New(num_parasites);

	for (i = 0; i < num_parasites; i++) {
	    PyTuple_SetItem(ret, i, PyString_FromString(parasites[i]));
	    g_free(parasites[i]);
	}

	g_free(parasites);
	return ret;
    }

    PyErr_SetString(pygimp_error, "could not list parasites");
    return NULL;
}

static PyObject *
pygimp_show_tool_tips(PyObject *self)
{
    return PyBool_FromLong(gimp_show_tool_tips());
}

static PyObject *
pygimp_show_help_button(PyObject *self)
{
    return PyBool_FromLong(gimp_show_help_button());
}

static PyObject *
pygimp_check_size(PyObject *self)
{
    return PyInt_FromLong(gimp_check_size());
}

static PyObject *
pygimp_check_type(PyObject *self)
{
    return PyInt_FromLong(gimp_check_type());
}

static PyObject *
pygimp_default_display(PyObject *self)
{
    return pygimp_display_new(gimp_default_display());
}

static PyObject *
pygimp_wm_class(PyObject *self)
{
    return PyString_FromString(gimp_wm_class());
}

static PyObject *
pygimp_display_name(PyObject *self)
{
    return PyString_FromString(gimp_display_name());
}

static PyObject *
pygimp_monitor_number(PyObject *self)
{
    return PyInt_FromLong(gimp_monitor_number());
}

static PyObject *
pygimp_get_progname(PyObject *self)
{
    return PyString_FromString(gimp_get_progname());
}

static PyObject *
pygimp_fonts_refresh(PyObject *self)
{
    if (!gimp_fonts_refresh()) {
	PyErr_SetString(pygimp_error, "could not refresh fonts");
	return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
pygimp_checks_get_shades(PyObject *self, PyObject *args, PyObject *kwargs)
{
    int type;
    guchar light, dark;
    static char *kwlist[] = { "type", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
				     "i:checks_get_shades", kwlist,
				     &type))
        return NULL;

    if (type < GIMP_CHECK_TYPE_LIGHT_CHECKS ||
	type > GIMP_CHECK_TYPE_BLACK_ONLY) {
        PyErr_SetString(PyExc_ValueError, "Invalid check type");
	return NULL;
    }

    gimp_checks_get_shades(type, &light, &dark);

    return Py_BuildValue("(ii)", light, dark);
}

static PyObject *
pygimp_fonts_get_list(PyObject *self, PyObject *args, PyObject *kwargs)
{
    char **list, *filter = NULL;
    int num, i;
    PyObject *ret;

    static char *kwlist[] = { "filter", NULL };

    if (!PyArg_ParseTupleAndKeywords(args, kwargs,
				     "|s:fonts_get_list", kwlist,
				     &filter))
        return NULL;

    list = gimp_fonts_get_list(filter, &num);

    if (num == 0) {
	PyErr_SetString(pygimp_error, "could not get font list");
	return NULL;
    }

    ret = PyList_New(num);

    for (i = 0; i < num; i++) {
	PyList_SetItem(ret, i, PyString_FromString(list[i]));
	g_free(list[i]);
    }

    g_free(list);

    return ret;
}

static PyObject *
id2image(PyObject *self, PyObject *args)
{
    int id;

    if (!PyArg_ParseTuple(args, "i:_id2image", &id))
	return NULL;

    if (id >= 0)
	return pygimp_image_new(id);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
id2drawable(PyObject *self, PyObject *args)
{
    int id;

    if (!PyArg_ParseTuple(args, "i:_id2drawable", &id))
	return NULL;

    if (id >= 0)
	return pygimp_drawable_new(NULL, id);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *
id2display(PyObject *self, PyObject *args)
{
    int id;

    if (!PyArg_ParseTuple(args, "i:_id2display", &id))
	return NULL;

    if (id >= 0)
	return pygimp_display_new(id);

    Py_INCREF(Py_None);
    return Py_None;
}

/* List of methods defined in the module */

static struct PyMethodDef gimp_methods[] = {
    {"main",	(PyCFunction)pygimp_main,	METH_VARARGS},
    {"quit",	(PyCFunction)pygimp_quit,	METH_NOARGS},
    {"message",	(PyCFunction)pygimp_message,	METH_VARARGS},
    {"set_data",	(PyCFunction)pygimp_set_data,	METH_VARARGS},
    {"get_data",	(PyCFunction)pygimp_get_data,	METH_VARARGS},
    {"progress_init",	(PyCFunction)pygimp_progress_init,	METH_VARARGS},
    {"progress_update",	(PyCFunction)pygimp_progress_update,	METH_VARARGS},
    {"progress_install",	(PyCFunction)pygimp_progress_install,	METH_VARARGS | METH_KEYWORDS},
    {"progress_uninstall",	(PyCFunction)pygimp_progress_uninstall,	METH_VARARGS},
    {"image_list",	(PyCFunction)pygimp_image_list,	METH_NOARGS},
    {"install_procedure",	(PyCFunction)pygimp_install_procedure,	METH_VARARGS},
    {"install_temp_proc",	(PyCFunction)pygimp_install_temp_proc,	METH_VARARGS},
    {"uninstall_temp_proc",	(PyCFunction)pygimp_uninstall_temp_proc,	METH_VARARGS},
    {"register_magic_load_handler",	(PyCFunction)pygimp_register_magic_load_handler,	METH_VARARGS},
    {"register_load_handler",	(PyCFunction)pygimp_register_load_handler,	METH_VARARGS},
    {"register_save_handler",	(PyCFunction)pygimp_register_save_handler,	METH_VARARGS},
    {"gamma",	(PyCFunction)pygimp_gamma,	METH_NOARGS},
    {"install_cmap",	(PyCFunction)pygimp_install_cmap,	METH_NOARGS},
    {"min_colors",	(PyCFunction)pygimp_min_colors,	METH_NOARGS},
    {"gtkrc",	(PyCFunction)pygimp_gtkrc,	METH_NOARGS},
    {"personal_rc_file",	(PyCFunction)pygimp_personal_rc_file, METH_VARARGS | METH_KEYWORDS},
    {"get_background",	(PyCFunction)pygimp_get_background,	METH_NOARGS},
    {"get_foreground",	(PyCFunction)pygimp_get_foreground,	METH_NOARGS},
    {"set_background",	(PyCFunction)pygimp_set_background,	METH_VARARGS},
    {"set_foreground",	(PyCFunction)pygimp_set_foreground,	METH_VARARGS},
    {"gradients_get_list",	(PyCFunction)pygimp_gradients_get_list,	METH_VARARGS | METH_KEYWORDS},
    {"context_get_gradient",	(PyCFunction)pygimp_context_get_gradient,	METH_NOARGS},
    {"context_set_gradient",	(PyCFunction)pygimp_context_set_gradient,	METH_VARARGS},
    {"gradients_get_gradient",	(PyCFunction)pygimp_gradients_get_gradient,	METH_NOARGS},
    {"gradients_set_gradient",	(PyCFunction)pygimp_gradients_set_gradient,	METH_VARARGS},
    {"gradient_get_uniform_samples",	(PyCFunction)pygimp_gradient_get_uniform_samples,	METH_VARARGS},
    {"gradient_get_custom_samples",	(PyCFunction)pygimp_gradient_get_custom_samples,	METH_VARARGS},
    {"gradients_sample_uniform",	(PyCFunction)pygimp_gradients_sample_uniform,	METH_VARARGS},
    {"gradients_sample_custom",	(PyCFunction)pygimp_gradients_sample_custom,	METH_VARARGS},
    {"delete", (PyCFunction)pygimp_delete, METH_VARARGS},
    {"displays_flush", (PyCFunction)pygimp_displays_flush, METH_NOARGS},
    {"displays_reconnect", (PyCFunction)pygimp_displays_reconnect, METH_VARARGS},
    {"tile_cache_size", (PyCFunction)pygimp_tile_cache_size, METH_VARARGS},
    {"tile_cache_ntiles", (PyCFunction)pygimp_tile_cache_ntiles, METH_VARARGS},
    {"tile_width", (PyCFunction)pygimp_tile_width, METH_NOARGS},
    {"tile_height", (PyCFunction)pygimp_tile_height, METH_NOARGS},
    {"extension_ack", (PyCFunction)pygimp_extension_ack, METH_NOARGS},
    {"extension_enable", (PyCFunction)pygimp_extension_enable, METH_NOARGS},
    {"extension_process", (PyCFunction)pygimp_extension_process, METH_VARARGS},
    {"parasite_find",      (PyCFunction)pygimp_parasite_find,      METH_VARARGS},
    {"parasite_attach",    (PyCFunction)pygimp_parasite_attach,    METH_VARARGS},
    {"attach_new_parasite",(PyCFunction)pygimp_attach_new_parasite,METH_VARARGS},
    {"parasite_detach",    (PyCFunction)pygimp_parasite_detach,    METH_VARARGS},
    {"parasite_list",    (PyCFunction)pygimp_parasite_list,    METH_NOARGS},
    {"show_tool_tips",  (PyCFunction)pygimp_show_tool_tips,  METH_NOARGS},
    {"show_help_button",  (PyCFunction)pygimp_show_help_button,  METH_NOARGS},
    {"check_size",  (PyCFunction)pygimp_check_size,  METH_NOARGS},
    {"check_type",  (PyCFunction)pygimp_check_type,  METH_NOARGS},
    {"default_display",  (PyCFunction)pygimp_default_display,  METH_NOARGS},
    {"wm_class", (PyCFunction)pygimp_wm_class,	METH_NOARGS},
    {"display_name", (PyCFunction)pygimp_display_name,	METH_NOARGS},
    {"monitor_number", (PyCFunction)pygimp_monitor_number,	METH_NOARGS},
    {"get_progname", (PyCFunction)pygimp_get_progname,	METH_NOARGS},
    {"fonts_refresh", (PyCFunction)pygimp_fonts_refresh,	METH_NOARGS},
    {"fonts_get_list", (PyCFunction)pygimp_fonts_get_list,	METH_VARARGS | METH_KEYWORDS},
    {"checks_get_shades", (PyCFunction)pygimp_checks_get_shades, METH_VARARGS | METH_KEYWORDS},
    {"_id2image", (PyCFunction)id2image, METH_VARARGS},
    {"_id2drawable", (PyCFunction)id2drawable, METH_VARARGS},
    {"_id2display", (PyCFunction)id2display, METH_VARARGS},
    {NULL,	 (PyCFunction)NULL, 0, NULL}		/* sentinel */
};


static struct _PyGimp_Functions pygimp_api_functions = {
    pygimp_image_new,
    pygimp_display_new,
    pygimp_layer_new,
    pygimp_channel_new,

    &PyGimpPDBFunction_Type,
    pygimp_pdb_function_new
};


/* Initialization function for the module (*must* be called initgimp) */

static char gimp_module_documentation[] =
"This module provides interfaces to allow you to write gimp plugins"
;

DL_EXPORT(void)
initgimp(void)
{
    PyObject *m, *d;
    PyObject *i;

    PyGimpPDB_Type.ob_type = &PyType_Type;
    PyGimpPDB_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpPDB_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpPDB_Type) < 0)
	return;

    PyGimpPDBFunction_Type.ob_type = &PyType_Type;
    PyGimpPDBFunction_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpPDBFunction_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpPDBFunction_Type) < 0)
	return;

    PyGimpImage_Type.ob_type = &PyType_Type;
    PyGimpImage_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpImage_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpImage_Type) < 0)
	return;

    PyGimpDisplay_Type.ob_type = &PyType_Type;
    PyGimpDisplay_Type.ob_type = &PyType_Type;
    PyGimpDisplay_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpDisplay_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpDisplay_Type) < 0)
	return;

    PyGimpLayer_Type.ob_type = &PyType_Type;
    PyGimpLayer_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpLayer_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpLayer_Type) < 0)
	return;

    PyGimpChannel_Type.ob_type = &PyType_Type;
    PyGimpChannel_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpChannel_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpChannel_Type) < 0)
	return;

    PyGimpTile_Type.ob_type = &PyType_Type;
    PyGimpTile_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpTile_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpTile_Type) < 0)
	return;

    PyGimpPixelRgn_Type.ob_type = &PyType_Type;
    PyGimpPixelRgn_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpPixelRgn_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpPixelRgn_Type) < 0)
	return;

    PyGimpParasite_Type.ob_type = &PyType_Type;
    PyGimpParasite_Type.tp_alloc = PyType_GenericAlloc;
    PyGimpParasite_Type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&PyGimpParasite_Type) < 0)
	return;

    /* set the default python encoding to utf-8 */
    PyUnicode_SetDefaultEncoding("utf-8");

    /* Create the module and add the functions */
    m = Py_InitModule4("gimp", gimp_methods,
		       gimp_module_documentation,
		       NULL, PYTHON_API_VERSION);

    /* Add some symbolic constants to the module */
    d = PyModule_GetDict(m);
    pygimp_error = PyErr_NewException("gimp.error", PyExc_RuntimeError, NULL);
    PyDict_SetItemString(d, "error", pygimp_error);

    PyDict_SetItemString(d, "pdb", pygimp_pdb_new());

    /* export the types used in gimpmodule */
    PyDict_SetItemString(d, "Image", (PyObject *)&PyGimpImage_Type);
    PyDict_SetItemString(d, "Drawable", (PyObject *)&PyGimpDrawable_Type);
    PyDict_SetItemString(d, "Layer", (PyObject *)&PyGimpLayer_Type);
    PyDict_SetItemString(d, "Channel", (PyObject *)&PyGimpChannel_Type);
    PyDict_SetItemString(d, "Display", (PyObject *)&PyGimpDisplay_Type);
    PyDict_SetItemString(d, "Tile", (PyObject *)&PyGimpTile_Type);
    PyDict_SetItemString(d, "PixelRgn", (PyObject *)&PyGimpPixelRgn_Type);
    PyDict_SetItemString(d, "Parasite", (PyObject *)&PyGimpParasite_Type);

    /* for other modules */
    PyDict_SetItemString(d, "_PyGimp_API",
			 i=PyCObject_FromVoidPtr(&pygimp_api_functions, NULL));
    Py_DECREF(i);

    PyDict_SetItemString(d, "version",
			 i=Py_BuildValue("(iii)",
					 gimp_major_version,
					 gimp_minor_version,
					 gimp_micro_version));
    Py_DECREF(i);

    /* Some environment constants */
    PyDict_SetItemString(d, "directory",
			 PyString_FromString(gimp_directory()));
    PyDict_SetItemString(d, "data_directory",
			 PyString_FromString(gimp_data_directory()));
    PyDict_SetItemString(d, "locale_directory",
			 PyString_FromString(gimp_locale_directory()));
    PyDict_SetItemString(d, "sysconf_directory",
			 PyString_FromString(gimp_sysconf_directory()));
    PyDict_SetItemString(d, "plug_in_directory",
			 PyString_FromString(gimp_plug_in_directory()));

    /* Check for errors */
    if (PyErr_Occurred())
	Py_FatalError("can't initialize module gimp");
}
