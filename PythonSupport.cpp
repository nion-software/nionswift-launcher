/*
 Copyright (c) 2012-2015 Nion Company.
*/

#include <stdint.h>

#include <QtCore/QDebug>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QProcessEnvironment>
#include <QtCore/QSettings>
#include <QtCore/QStandardPaths>
#include <QtCore/QStringList>
#include <QtCore/QUrl>
#include <QtCore/QVariant>

#include <QtWidgets/QApplication>

#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
#if !defined(Q_OS_WIN)
#include <dlfcn.h>
#define LOOKUP_SYMBOL dlsym
#else
#define LOOKUP_SYMBOL GetProcAddress
#endif
#endif

#include "PythonSupport.h"
#include "PythonStubs.h"
#include "PythonSelectDialog.h"

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#define PY_ARRAY_UNIQUE_SYMBOL d2af2aa3297e

#pragma push_macro("_DEBUG")
#undef _DEBUG

#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
// numpy uses these functions, so to allow linking to occur
// override them here and redefine them with the dynamic
// python versions.
#pragma push_macro("PyCapsule_CheckExact")
#pragma push_macro("PyCapsule_GetPointer")
#pragma push_macro("PyErr_Format")
#pragma push_macro("PyErr_Print")
#pragma push_macro("PyErr_SetString")
#pragma push_macro("PyExc_AttributeError")
#pragma push_macro("PyExc_ImportError")
#pragma push_macro("PyExc_RuntimeError")
#pragma push_macro("PyImport_ImportModule")
#pragma push_macro("PyObject_GetAttrString")
#undef PyCapsule_CheckExact
#define PyCapsule_CheckExact DPyCapsule_CheckExact
#define PyCapsule_GetPointer DPyCapsule_GetPointer
#define PyErr_Format DPyErr_Format
#define PyErr_Print DPyErr_Print
#define PyErr_SetString DPyErr_SetString
#define PyExc_AttributeError DPyExc_GetAttributeError()
#define PyExc_ImportError DPyExc_GetImportError()
#define PyExc_RuntimeError DPyExc_GetRuntimeError()
#define PyImport_ImportModule DPyImport_ImportModule
#define PyObject_GetAttrString DPyObject_GetAttrString
#endif

#include "numpy/arrayobject.h"
#include "structmember.h"

#define SIMPLE_WRAP 0

static void* init_numpy() {
    import_array();
    return NULL;
}

#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
#pragma pop_macro("PyCapsule_CheckExact")
#pragma pop_macro("PyCapsule_GetPointer")
#pragma pop_macro("PyErr_Format")
#pragma pop_macro("PyErr_Print")
#pragma pop_macro("PyErr_SetString")
#pragma pop_macro("PyExc_AttributeError")
#pragma pop_macro("PyExc_ImportError")
#pragma pop_macro("PyExc_RuntimeError")
#endif

#pragma pop_macro("_DEBUG")

#if defined(Q_OS_WIN)
#include <Windows.h>
#include <WinBase.h>
#endif

static PythonSupport *thePythonSupport = NULL;
const char* PythonSupport::qobject_capsule_name = "b93c9a511d32.qobject";

void PythonSupport::checkTarget(const QString &python_path)
{
    // this code does not work since Python is not made to
    // be un-loadable. Once NumPy is initialized, it cannot
    // be initialized again.
#if defined(Q_OS_WIN)
    const char *script =
        "found = '>'\n"
        "try:\n"
        "  # import scipy\n"
        "  found += ' scipy'\n"
        "except BaseException as e:\n"
        "  found += ' no-scipy ' + str(e)\n";
    SetEnvironmentVariable("PYTHONPATH", "C:\\Miniconda3");
    SetEnvironmentVariable("PYTHONHOME", "C:\\Miniconda3");
    HMODULE m = LoadLibrary("C:\\Miniconda3\\Python34.dll");
    typedef void(*Py_InitializeFn)();
    typedef void(*Py_FinalizeFn)();
    typedef void(*Py_DecRefFn)(PyObject *);
    typedef PyObject* (*PyRun_StringFn)(const char *, int start, PyObject *globals, PyObject *locals);
    typedef char* (*PyBytes_AsStringFn)(PyObject *);
    typedef PyObject* (*PyDict_NewFn)();
    typedef PyObject* (*PyDict_GetItemStringFn)(PyObject *p, const char *key);
    typedef PyObject* (*PyUnicode_AsUTF8StringFn)(PyObject *unicode);
    typedef PyObject* (*PyImport_AddModuleFn)(const char *module_name);
    typedef void(*PyImport_CleanupFn)();
    typedef PyObject* (*PyModule_GetDictFn)(PyObject *module);
    Py_InitializeFn py_initialize_fn = (Py_InitializeFn)GetProcAddress(m, "Py_Initialize");
    Py_FinalizeFn py_finalize_fn = (Py_FinalizeFn)GetProcAddress(m, "Py_Finalize");
    Py_DecRefFn py_dec_ref_fn = (Py_DecRefFn)GetProcAddress(m, "Py_DecRef");
    PyRun_StringFn pyrun_string_fn = (PyRun_StringFn)GetProcAddress(m, "PyRun_String");
    PyBytes_AsStringFn pybytes_as_string_fn = (PyBytes_AsStringFn)GetProcAddress(m, "PyBytes_AsString");
    PyUnicode_AsUTF8StringFn pyunicode_as_utf8_string_fn = (PyUnicode_AsUTF8StringFn)GetProcAddress(m, "PyUnicode_AsUTF8String");
    PyDict_NewFn pydict_new_fn = (PyDict_NewFn)GetProcAddress(m, "PyDict_New");
    PyDict_GetItemStringFn pydict_get_item_string_fn = (PyDict_GetItemStringFn)GetProcAddress(m, "PyDict_GetItemString");
    PyImport_AddModuleFn pyimport_add_module_fn = (PyImport_AddModuleFn)GetProcAddress(m, "PyImport_AddModule");
    PyImport_CleanupFn pyimport_cleanup_fn = (PyImport_CleanupFn)GetProcAddress(m, "PyImport_Cleanup");
    PyModule_GetDictFn pymodule_get_dict_fn = (PyModule_GetDictFn)GetProcAddress(m, "PyModule_GetDict");
    Q_ASSERT(py_initialize_fn != NULL);
    Q_ASSERT(pyrun_string_fn != NULL);
    Q_ASSERT(pybytes_as_string_fn != NULL);
    Q_ASSERT(pydict_new_fn != NULL);
    Q_ASSERT(pydict_get_item_string_fn != NULL);
    py_initialize_fn();
    PyObject *py_main = pyimport_add_module_fn("__main__");
    PyObject *py_dict = pymodule_get_dict_fn(py_main);
    //PyObject *py_dict = pydict_new_fn();
    PyObject* result = pyrun_string_fn(script, Py_file_input, py_dict, py_dict);
    qDebug() << "Result: " << QString::fromUtf8(pybytes_as_string_fn(pyunicode_as_utf8_string_fn(pydict_get_item_string_fn(py_dict, "found"))));
    py_dec_ref_fn(result);
    //py_finalize_fn();

    //FreeLibrary(m);
#endif
}

QString PythonSupport::ensurePython(const QString &python_home)
{
#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
    QTextStream cout(stdout);

    if (!python_home.isEmpty() && QFile(python_home).exists())
    {
        cout << "Using Python from command line: " << python_home << endl;
        return python_home;
    }

    // try reading the config file
    QDir dir(QStandardPaths::writableLocation(QStandardPaths::DataLocation));
    QString data_location = dir.absolutePath();
    QString config_file_path = QDir(data_location).absoluteFilePath("PythonConfig.ini");
    QFile config_file(config_file_path);
    if (config_file.open(QIODevice::ReadOnly))
    {
        QTextStream in(&config_file);
        QString line = in.readLine();
        cout << "Using Python from " << QDir::toNativeSeparators(config_file_path) << ": " << QDir::toNativeSeparators(line) << endl;
        if (QFile(line).exists())
            return line;
    }

    // try asking the user
    PythonSelectDialog pythonSelectDialog;
    if (!pythonSelectDialog.exec())
        return QString();
    cout << "Using Python from selection: " << pythonSelectDialog.getPythonHome() << endl;
    return pythonSelectDialog.getPythonHome();
#else
#if defined(Q_OS_MAC)
    return qApp->applicationDirPath()+"/../Resources/miniconda";
#endif
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return qApp->applicationDirPath() + "/miniconda";
#endif
#endif
}

void PythonSupport::initInstance(const QString &python_home)
{
    thePythonSupport = new PythonSupport(python_home);
}

void PythonSupport::deinitInstance()
{
    delete thePythonSupport;
    thePythonSupport = nullptr;
}

PythonSupport *PythonSupport::instance()
{
    return thePythonSupport;
}

PythonSupport::PythonSupport(const QString &python_home)
    : module_exception(nullptr)
{
#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
#if defined(Q_OS_MAC)
    QString file_path;
    QString venv_conf_file_name = QDir(python_home).absoluteFilePath("pyvenv.cfg");
    if (QFile(venv_conf_file_name).exists())
    {
        // probably Python w/ virtual environment
        QSettings settings(venv_conf_file_name, QSettings::IniFormat);
        QString home_bin_path = settings.value("home").toString();
        if (!home_bin_path.isEmpty())
        {
            QDir home_dir(home_bin_path);
            home_dir.cdUp();
            QString file_path_37 = home_dir.absoluteFilePath("lib/libpython3.7m.dylib");
            QString file_path_36 = home_dir.absoluteFilePath("lib/libpython3.6m.dylib");
            QString file_path_35 = home_dir.absoluteFilePath("lib/libpython3.5m.dylib");
            file_path = QFile(file_path_37).exists() ? file_path_37 : QFile(file_path_36).exists() ? file_path_36 : file_path_35;
        }
    }
    else
    {
        // probably conda or standard Python
        QString file_path_37 = QDir(python_home).absoluteFilePath("lib/libpython3.7m.dylib");
        QString file_path_36 = QDir(python_home).absoluteFilePath("lib/libpython3.6m.dylib");
        QString file_path_35 = QDir(python_home).absoluteFilePath("lib/libpython3.5m.dylib");
        file_path = QFile(file_path_37).exists() ? file_path_37 : QFile(file_path_36).exists() ? file_path_36 : file_path_35;
    }
    void *dl = dlopen(file_path.toUtf8().constData(), RTLD_LAZY);
#elif defined(Q_OS_LINUX)
    QString file_path;

    QString file_path_37s = "/usr/lib/x86_64-linux-gnu/libpython3.7m.so";
    QString file_path_36s = "/usr/lib/x86_64-linux-gnu/libpython3.6m.so";
    QString file_path_35s = "/usr/lib/x86_64-linux-gnu/libpython3.5m.so";
    QString file_path_37 = QDir(python_home).absoluteFilePath("lib/libpython3.7m.so");
    QString file_path_36 = QDir(python_home).absoluteFilePath("lib/libpython3.6m.so");
    QString file_path_35 = QDir(python_home).absoluteFilePath("lib/libpython3.5m.so");

    QStringList file_paths;
    file_paths.append(file_path_37);
    file_paths.append(file_path_36);
    file_paths.append(file_path_35);
    file_paths.append(file_path_37s);
    file_paths.append(file_path_36s);
    file_paths.append(file_path_35s);

    Q_FOREACH(file_path, file_paths)
    {
        if (QFile(file_path).exists())
            break;
    }

    void *dl = dlopen(file_path.toUtf8().constData(), RTLD_LAZY | RTLD_GLOBAL);
#else
    QString python_home_new = python_home;
    QString file_path;
    QString venv_conf_file_name = QDir(python_home).absoluteFilePath("pyvenv.cfg");
    if (QFile(venv_conf_file_name).exists())
    {
        // probably Python w/ virtual environment.
        // this code makes me hate both Windows and Qt equally. it is necessary to handle backslashes in paths.
        QFile file(venv_conf_file_name);
        if (file.open(QFile::ReadOnly))
        {
            QByteArray bytes = file.readAll();
            QString str = QString::fromUtf8(bytes);
            Q_FOREACH(const QString &line, str.split(QRegExp("[\r\n]"), QString::SkipEmptyParts))
            {
                QRegExp re("^home\\s?=\\s?(.+)$");
                if (re.indexIn(line) >= 0)
                {
                    QString home_bin_path = re.cap(1).trimmed();
                    if (!home_bin_path.isEmpty())
                    {
                        QDir home_dir(QDir::fromNativeSeparators(home_bin_path));
                        python_home_new = home_dir.absolutePath();
                        QString file_path_37 = QDir(python_home).absoluteFilePath("Scripts/Python37.dll");
                        QString file_path_36 = QDir(python_home).absoluteFilePath("Scripts/Python36.dll");
                        QString file_path_35 = QDir(python_home).absoluteFilePath("Scripts/Python35.dll");
                        file_path = QFile(file_path_37).exists() ? file_path_37 : QFile(file_path_36).exists() ? file_path_36 : file_path_35;
                    }
                }
            }
        }
    }
    else
    {
        QString file_path_37 = QDir(python_home).absoluteFilePath("Python37.dll");
        QString file_path_36 = QDir(python_home).absoluteFilePath("Python36.dll");
        QString file_path_35 = QDir(python_home).absoluteFilePath("Python35.dll");
        file_path = QFile(file_path_37).exists() ? file_path_37 : QFile(file_path_36).exists() ? file_path_36 : file_path_35;
    }

    // Python may have side-by-side DLLs that it uses. This seems to be an issue with how
    // Anaconda handles installation of the VS redist -- they include it in the directory
    // rather than installing it system wide. That approach works great when running the
    // python.exe, but not so great when loading the python35.dll. To avoid _us_ having to
    // install the VS redist, we add the python home to the DLL search path. Through trial
    // and error, the qputenv or SetDllDirectory approach works. The AddDllDirectory does not
    // work. I leave this code here so the next time someone encounters it, they can try these
    // different solutions.

    // DOES NOT WORK
    //SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_USER_DIRS);
    //AddDllDirectory((PCWSTR)QDir::toNativeSeparators(python_home).utf16());  // ensure that DLLs local to Python can be found

    // DOES NOT WORK
    //QProcessEnvironment::systemEnvironment().insert("PATH", QProcessEnvironment::systemEnvironment().value("PATH") + ";" + python_home);

    // WORKS. Library/bin added for Anaconda/Miniconda 3.7 compatibility.
    qputenv("PATH", (qgetenv("PATH") + ";" + python_home + ";" + QDir(python_home).absoluteFilePath("Library/bin")).toUtf8());

    // WORKS ALMOST. DOESN'T ALLOW CAMERA PLUG-INS TO LOAD.
    //SetDllDirectory(QDir::toNativeSeparators(python_home).toUtf8());  // ensure that DLLs local to Python can be found

    void *dl = LoadLibrary(QDir::toNativeSeparators(file_path).toUtf8());
#endif
    extern void initialize_pylib(void *);
    initialize_pylib(dl);
#endif

#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
#if !defined(Q_OS_WIN)
    dynamic_PyArg_ParseTuple = (PyArg_ParseTupleFn)dlsym(dl, "PyArg_ParseTuple");
    dynamic_Py_BuildValue = (Py_BuildValueFn)dlsym(dl, "Py_BuildValue");
#else
    dynamic_PyArg_ParseTuple = (PyArg_ParseTupleFn)GetProcAddress(HMODULE(dl), "PyArg_ParseTuple");
    dynamic_Py_BuildValue = (Py_BuildValueFn)GetProcAddress(HMODULE(dl), "Py_BuildValue");
#endif
#else
    dynamic_PyArg_ParseTuple = PyArg_ParseTuple;
    dynamic_Py_BuildValue = Py_BuildValue;
#endif
}

PythonSupport::PythonSupport(PythonSupport const &)
{
}

PythonSupport& PythonSupport::operator=(PythonSupport const &)
{
    return *this;
}

PythonSupport::~PythonSupport()
{
    Py_XDECREF(module_exception);

#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
    extern void *pylib;
    void *dl = pylib;
#endif

    extern void deinitialize_pylib();
    deinitialize_pylib();

#if defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON
#if defined(Q_OS_MAC)
    dlclose(dl);
#elif defined(Q_OS_LINUX)
    dlclose(dl);
#else
    FreeLibrary((HMODULE)dl);
#endif
#endif
}

class PyObjectPtr
{
public:
    PyObjectPtr() : py_object(NULL) { }
    PyObjectPtr(const PyObjectPtr &py_object_ptr)
    {
        Python_ThreadBlock thread_block;
        py_object = py_object_ptr.pyObject();
        Py_INCREF(py_object);
    }
    ~PyObjectPtr()
    {
        Python_ThreadBlock thread_block;
        if (this->py_object)
        {
            Py_DECREF(this->py_object);
        }
    }
    PyObjectPtr &operator=(const PyObjectPtr &) = delete;
    PyObject *pyObject() const { return this->py_object; }
    void setPyObject(PyObject *py_object)
    {
        Python_ThreadBlock thread_block;
        if (this->py_object)
        {
            Py_DECREF(this->py_object);
        }
        Py_INCREF(py_object);
        this->py_object = py_object;
    }

    static int metaId();

private:
    PyObject *py_object;
};

Q_DECLARE_METATYPE(PyObjectPtr)

// static
int PyObjectPtr::metaId()
{
    static bool once = false;
    static int meta_id = 0;
    if (!once)
    {
        meta_id = qRegisterMetaType<PyObjectPtr>("PyObjectPtr");
        once = true;
    }
    return meta_id;
}

struct Python_ThreadBlockState
{
    PyGILState_STATE gstate;
};

void Python_ThreadBlock::release()
{
    if (m_state)
    {
        CALL_PY(PyGILState_Release)(m_state->gstate);
        delete m_state;
        m_state = NULL;
    }
}

void Python_ThreadBlock::grab()
{
    m_state = new Python_ThreadBlockState();
    m_state->gstate = CALL_PY(PyGILState_Ensure)();
}

struct Python_ThreadAllowState
{
    PyThreadState *gstate;
};

void Python_ThreadAllow::release()
{
    if (m_state)
    {
        CALL_PY(PyEval_RestoreThread)(m_state->gstate);
        delete m_state;
        m_state = NULL;
    }
}

void Python_ThreadAllow::grab()
{
    m_state = new Python_ThreadAllowState();
    m_state->gstate = CALL_PY(PyEval_SaveThread)();
}

static wchar_t python_home_static[512];
static wchar_t python_program_name_static[512];
#if defined(Q_OS_WIN)
static wchar_t python_path_static[512];
#endif

void PythonSupport::initialize(const QString &python_home)
{
#if !(defined(DYNAMIC_PYTHON) && DYNAMIC_PYTHON)
#if defined(Q_OS_MAC) && !defined(DEBUG)
    if (!python_home.isEmpty())
        setenv("PYTHONHOME", python_home.toUtf8().constData(), 1);
#endif
#else
    if (!python_home.isEmpty())
    {
        memset(&python_home_static[0], 0, sizeof(python_home_static));
        python_home.toWCharArray(python_home_static);
        CALL_PY(Py_SetPythonHome)(python_home_static);  // requires a permanent buffer
    }
#endif

#if defined(Q_OS_MAC)
    QString python_home_new = python_home;
    QString python_program_name = QDir(python_home).absoluteFilePath("bin/python3");

    // check if we're running inside a venv, determined by whether pyvenv.cfg exists.
    // if so, read the config file and find the home key, indicating the python installation
    // directory (with /bin attached). then call SetPythonHome and SetProgramName with the
    // installation directory and virtual environment python path respectively. these are
    // required for the virtual environment to load correctly.
    QString venv_conf_file_name = QDir(python_home).absoluteFilePath("pyvenv.cfg");
    if (QFile(venv_conf_file_name).exists())
    {
        QSettings settings(venv_conf_file_name, QSettings::IniFormat);
        QString home_bin_path = settings.value("home").toString();
        if (!home_bin_path.isEmpty())
        {
            QDir home_dir(home_bin_path);
            home_dir.cdUp();
            python_home_new = home_dir.absolutePath();
        }
    }

    memset(&python_home_static[0], 0, sizeof(python_home_static));
    python_home_new.toWCharArray(python_home_static);
    CALL_PY(Py_SetPythonHome)(python_home_static);  // requires a permanent buffer

    memset(&python_program_name_static[0], 0, sizeof(python_program_name_static));
    python_program_name.toWCharArray(python_program_name_static);
    CALL_PY(Py_SetProgramName)(python_program_name_static);  // requires a permanent buffer
#elif defined(Q_OS_WIN)
    QString python_home_new = python_home;
    QString python_program_name = QDir(python_home).absoluteFilePath("Python.exe");

    // check if we're running inside a venv, determined by whether pyvenv.cfg exists.
    // if so, read the config file and find the home key, indicating the python installation
    // directory (with /bin attached). then call SetPythonHome and SetProgramName with the
    // installation directory and virtual environment python path respectively. these are
    // required for the virtual environment to load correctly.
    QString venv_conf_file_name = QDir(python_home).absoluteFilePath("pyvenv.cfg");
    if (QFile(venv_conf_file_name).exists())
    {
        // probably Python w/ virtual environment.
        // this code makes me hate both Windows and Qt equally. it is necessary to handle backslashes in paths.
        QFile file(venv_conf_file_name);
        if (file.open(QFile::ReadOnly))
        {
            QByteArray bytes = file.readAll();
            QString str = QString::fromUtf8(bytes);
            Q_FOREACH(const QString &line, str.split(QRegExp("[\r\n]"), QString::SkipEmptyParts))
            {
                QRegExp re("^home\\s?=\\s?(.+)$");
                if (re.indexIn(line) >= 0)
                {
                    QString home_bin_path = re.cap(1).trimmed();
                    if (!home_bin_path.isEmpty())
                    {
                        QDir home_dir(QDir::fromNativeSeparators(home_bin_path));
                        python_home_new = home_dir.absolutePath();
                        python_program_name = QDir(python_home).absoluteFilePath("Scripts/Python.exe");

                        // required to configure the path; see https://bugs.python.org/issue34725
                        QStringList python_paths;
                        python_paths.append(QDir(python_home).absoluteFilePath("Scripts/python37.zip"));
                        python_paths.append(QDir(python_home).absoluteFilePath("Scripts/python36.zip"));
                        python_paths.append(QDir(python_home).absoluteFilePath("Scripts/python35.zip"));
                        python_paths.append(QDir(python_home_new).absoluteFilePath("DLLs"));
                        python_paths.append(QDir(python_home_new).absoluteFilePath("lib"));
                        python_paths.append(QDir(python_home_new).absolutePath());
                        python_paths.append(QDir(python_home).absolutePath());
                        python_paths.append(QDir(python_home).absoluteFilePath("lib/site-packages"));
                        memset(&python_path_static[0], 0, sizeof(python_path_static));
                        python_paths.join(";").toWCharArray(python_path_static);
                        CALL_PY(Py_SetPath)(python_path_static);  // requires a permanent buffer
                    }
                }
            }
        }
    }

    memset(&python_home_static[0], 0, sizeof(python_home_static));
    python_home_new.toWCharArray(python_home_static);
    CALL_PY(Py_SetPythonHome)(python_home_static);  // requires a permanent buffer

    memset(&python_program_name_static[0], 0, sizeof(python_program_name_static));
    python_program_name.toWCharArray(python_program_name_static);
    CALL_PY(Py_SetProgramName)(python_program_name_static);  // requires a permanent buffer
#elif defined(Q_OS_LINUX)
    QString python_home_new = python_home;
    QString python_program_name = QDir(python_home).absoluteFilePath("bin/python3");

    // check if we're running inside a venv, determined by whether pyvenv.cfg exists.
    // if so, read the config file and find the home key, indicating the python installation
    // directory (with /bin attached). then call SetPythonHome and SetProgramName with the
    // installation directory and virtual environment python path respectively. these are
    // required for the virtual environment to load correctly.
    QString venv_conf_file_name = QDir(python_home).absoluteFilePath("pyvenv.cfg");
    if (QFile(venv_conf_file_name).exists())
    {
        QSettings settings(venv_conf_file_name, QSettings::IniFormat);
        QString home_bin_path = settings.value("home").toString();
        if (!home_bin_path.isEmpty())
        {
            QDir home_dir(home_bin_path);
            home_dir.cdUp();
            python_home_new = home_dir.absolutePath();
        }
    }

    memset(&python_home_static[0], 0, sizeof(python_home_static));
    python_home_new.toWCharArray(python_home_static);
    CALL_PY(Py_SetPythonHome)(python_home_static);  // requires a permanent buffer

    memset(&python_program_name_static[0], 0, sizeof(python_program_name_static));
    python_program_name.toWCharArray(python_program_name_static);
    CALL_PY(Py_SetProgramName)(python_program_name_static);  // requires a permanent buffer
#endif

    CALL_PY(Py_Initialize)();

    // initialize threads and save the main thread state
    CALL_PY(PyEval_InitThreads)();
    PyGILState_STATE main_restore_state = PyGILState_UNLOCKED;
    CALL_PY(PyGILState_Release)(main_restore_state);

    PyObjectPtr::metaId();

    Python_ThreadBlock thread_block;

    init_numpy();
}

void PythonSupport::deinitialize()
{
    CALL_PY(PyGILState_Ensure)();

    CALL_PY(Py_Finalize)();
}

QObject *PythonSupport::UnwrapQObject(PyObject *py_obj)
{
#if SIMPLE_WRAP
    return static_cast<QObject *>((void *)PyInt_AsLong(py_obj));
#else
	QObject *ptr = NULL;
	if (CALL_PY(PyCapsule_CheckExact)(py_obj))
		ptr = static_cast<QObject *>(CALL_PY(PyCapsule_GetPointer)(py_obj, PythonSupport::qobject_capsule_name));
    return ptr;
#endif
}

PyObject *WrapQObject(QObject *ptr)
{
#if SIMPLE_WRAP
    return PyInt_FromLong((long)ptr);
#else
    PyObject *ret = CALL_PY(PyCapsule_New)(ptr, PythonSupport::qobject_capsule_name, NULL);
    return ret;
#endif
}

// New reference
PyObject* QStringToPyObject(const QString& str)
{
    if (str.isNull()) {
        return PyString_FromString("");
    } else {
        return CALL_PY(PyUnicode_DecodeUTF16)((const char*)str.utf16(), str.length()*2, NULL, NULL);
    }
}

void FreePyObject(PyObject *py_object)
{
    Py_XDECREF(py_object);
}

PyObject *QVariantToPyObject(const QVariant &value);

// New reference
PyObject *QVariantMapToPyObject(const QVariantMap &variant_map)
{
    PyObject *py_map = CALL_PY(PyDict_New)();
    Q_FOREACH(const QString &key, variant_map.keys())
    {
        PyObject *py_key = QStringToPyObject(key);
        PyObject *py_value = QVariantToPyObject(variant_map.value(key));
        CALL_PY(PyDict_SetItem)(py_map, py_key, py_value);
        Py_DECREF(py_key);
        Py_DECREF(py_value);
    }
    return py_map;
}

// New reference
PyObject *QVariantListToPyObject(const QVariantList &variant_list)
{
    PyObject *py_list = CALL_PY(PyTuple_New)(variant_list.size());
    int i = 0;
    Q_FOREACH(const QVariant &variant, variant_list)
    {
        CALL_PY(PyTuple_SetItem)(py_list, i, QVariantToPyObject(variant)); // steals reference
        i++;
    }
    return py_list;
}

// New reference
PyObject *QVariantToPyObject(const QVariant &value)
{
    const void *data = (void *)value.constData();
    int type = value.userType();

    switch (type)
    {
        case QMetaType::Void:
            Py_INCREF(CALL_PY(Py_NoneGet)());
            return CALL_PY(Py_NoneGet)();

        case QMetaType::Char:
            return PyInt_FromLong(*((char*)data));

        case QMetaType::UChar:
            return PyInt_FromLong(*((unsigned char*)data));

        case QMetaType::Short:
            return PyInt_FromLong(*((short*)data));

        case QMetaType::UShort:
            return PyInt_FromLong(*((unsigned short*)data));

        case QMetaType::Long:
            return PyInt_FromLong(*((long*)data));

        case QMetaType::ULong:
            // does not fit into simple int of python
            return CALL_PY(PyLong_FromUnsignedLong)(*((unsigned long*)data));

        case QMetaType::Bool: {
            PyObject *py_obj = value.toBool() ? CALL_PY(Py_TrueGet)() : CALL_PY(Py_FalseGet)();
            Py_INCREF(py_obj);
            return py_obj;
        }

        case QMetaType::Int:
            return PyInt_FromLong(*((int*)data));

        case QMetaType::UInt:
            // does not fit into simple int of python
            return CALL_PY(PyLong_FromUnsignedLong)(*((unsigned int*)data));

        case QMetaType::QChar:
            return PyInt_FromLong(*((short*)data));

        case QMetaType::Float:
            return CALL_PY(PyFloat_FromDouble)(*((float*)data));

        case QMetaType::Double:
            return CALL_PY(PyFloat_FromDouble)(*((double*)data));

        case QMetaType::LongLong:
            return CALL_PY(PyLong_FromLongLong)(*((qint64*)data));

        case QMetaType::ULongLong:
            return CALL_PY(PyLong_FromUnsignedLongLong)(*((quint64*)data));

        case QMetaType::QVariantMap:
            return QVariantMapToPyObject(*((QVariantMap*)data));

        case QMetaType::QVariantList:
            return QVariantListToPyObject(*((QVariantList*)data));

        case QMetaType::QString:
            return QStringToPyObject(*((QString*)data));

        case QMetaType::QStringList:
        {
            QVariantList variant_list;
            Q_FOREACH(const QString &str, *((QStringList*)data))
                variant_list.append(str);
            return QVariantListToPyObject(variant_list);
        }

        case QMetaType::QObjectStar:
        {
            return WrapQObject(*((QObject**)data));
        }

        default:
        {
            if (type == PyObjectPtr::metaId())
            {
                PyObject *py_object = ((PyObjectPtr *)data)->pyObject();
                Py_INCREF(py_object);
                return py_object;
            }
            else if (type == qMetaTypeId< QList<QUrl> >())
            {
                QVariantList variant_list;
                Q_FOREACH(const QUrl &url, *((QList<QUrl> *)data))
                    variant_list.append(url.toString());
                return QVariantListToPyObject(variant_list);
            }
        }
    }

    Py_INCREF(CALL_PY(Py_NoneGet)());
    return CALL_PY(Py_NoneGet)();
}

QString PyObjectToQString(PyObject* val)
{
    if (PyUnicode_Check(val))
    {
        return QString::fromUtf8(CALL_PY(PyUnicode_AsUTF8)(val));
    }
    return QString();
}

QVariant PyObjectToQVariant(PyObject *py_object)
{
    if (PyString_Check(py_object) || PyUnicode_Check(py_object))
    {
        return PyObjectToQString(py_object);
    }
    else if (PyInt_Check(py_object))
    {
        return (int)PyInt_AsLong(py_object);
    }
    else if (PyLong_Check(py_object))
    {
        return CALL_PY(PyLong_AsLongLong)(py_object);
    }
    else if (CALL_PY(PyFloat_Check)(py_object))
    {
        return CALL_PY(PyFloat_AsDouble)(py_object);
    }
    else if (CALL_PY(PyBool_Check)(py_object))
    {
        return CALL_PY(PyObject_IsTrue)(py_object);
    }
	else if (CALL_PY(PyCapsule_IsValid)(py_object, PythonSupport::qobject_capsule_name))
    {
        return qVariantFromValue(PythonSupport::instance()->UnwrapQObject(py_object));
    }
    else if (PyDict_Check(py_object))
    {
        QMap<QString,QVariant> map;
        PyObject *items = CALL_PY(PyMapping_Items)(py_object);
        if (items)
        {
            int count = (int)CALL_PY(PyList_Size)(items);
            for (int i=0; i<count; i++)
            {
                PyObject *tuple = CALL_PY(PyList_GetItem)(items,i); //borrowed
                PyObject *key = CALL_PY(PyTuple_GetItem)(tuple, 0); //borrowed
                PyObject *value = CALL_PY(PyTuple_GetItem)(tuple, 1); //borrowed
                map.insert(PyObjectToQString(key), PyObjectToQVariant(value));
            }
            Py_DECREF(items);
            return map;
        }
    }
    else if (PyList_Check(py_object) || PyTuple_Check(py_object))
    {
        if (CALL_PY(PySequence_Check)(py_object))
        {
            QVariantList list;
            int count = (int)CALL_PY(PySequence_Size)(py_object);
            PyObject *fast_list = CALL_PY(PySequence_Fast)(py_object, "error");
            PyObject **fast_items = PySequence_Fast_ITEMS(fast_list);
            for (int i=0; i<count; i++)
            {
                list.append(PyObjectToQVariant(fast_items[i]));
            }
            Py_DECREF(fast_list);
            return list;
        }
    }
    else if (py_object == CALL_PY(Py_NoneGet)())
    {
        return QVariant();
    }
    else
    {
        PyObjectPtr py_object_ptr;
        py_object_ptr.setPyObject(py_object);
        return qVariantFromValue(py_object_ptr);
    }
    return QVariant();
}

void PythonSupport::addResourcePath(const QString &resources_path)
{
    PyObject *sys_module = CALL_PY(PyImport_ImportModule)("sys");
    PyObject *py_path = CALL_PY(PyObject_GetAttrString)(sys_module, "path");
    PyObject *py_filename = QStringToPyObject(resources_path);
    CALL_PY(PyList_Insert)(py_path, 1, py_filename);
    Py_DECREF(py_filename);
    Py_DECREF(py_path);
    Py_DECREF(sys_module);
}

void PythonSupport::addPyObjectToModuleFromQVariant(PyObject* module, const QString &identifier, const QVariant& object)
{
	CALL_PY(PyModule_AddObject)(module, identifier.toLatin1().data(), QVariantToPyObject(object));   // steals reference
}

void PythonSupport::addPyObjectToModule(PyObject* module, const QString &identifier, PyObject *object)
{
    CALL_PY(PyModule_AddObject)(module, identifier.toLatin1().data(), object);   // steals reference
}

// Example:
// Python:
//    class Dispatch:
//    def __init__(self, zip):
//    self.zip = zip
//    def doSomething(self, zz, yy):
//    print self.zip
//    print zz
//    print yy
//    return [self.zip, zz]
// Qml:
//    var result_list = app.invokePyMethod(app.lookupPyObjectByName("Dispatch(6)"), "doSomething", ["hockey stick", 40])

QVariant PythonSupport::invokePyMethod(const QVariant &object, const QString &method, const QVariantList &args)
{
    QVariant result;

    Python_ThreadBlock thread_block;

    PyObject *py_object = QVariantToPyObject(object);
    if (py_object)
    {
        PyObject *callable = CALL_PY(PyObject_GetAttrString)(py_object, method.toLatin1().data());

        if (CALL_PY(PyCallable_Check)(callable))
        {
            CALL_PY(PyErr_Clear)();

            bool err = false;

            PyObject *py_args = args.size() > 0 ? CALL_PY(PyTuple_New)(args.size()) : NULL;

            int index = 0;
            Q_FOREACH(const QVariant &arg, args)
            {
                PyObject *obj = QVariantToPyObject(arg);
                if (obj)
                {
                    // steals reference
                    CALL_PY(PyTuple_SetItem)(py_args, index, obj);
                }
                else
                {
                    err = true;
                    break;
                }
                ++index;
            }

            if (!err)
            {
                CALL_PY(PyErr_Clear)();
                PyObject *py_result = CALL_PY(PyObject_CallObject)(callable, py_args);

                if (py_result)
                {
                    result = PyObjectToQVariant(py_result);
                    Py_DECREF(py_result);
                }
                else
                {
                    CALL_PY(PyErr_Print)();
                    CALL_PY(PyErr_Clear)();
                }
            }

            Py_XDECREF(py_args);
            Py_DECREF(callable);
        }
        Py_DECREF(py_object);
    }

    return result;
}


QVariant PythonSupport::getAttribute(const QVariant &object, const QString &attribute)
{
    QVariant result;

    Python_ThreadBlock thread_block;

    PyObject *py_object = QVariantToPyObject(object);
    if (py_object)
    {
        PyObject *py_attribute = PyString_FromString(attribute.toLatin1().data());
        if (py_attribute)
        {
            CALL_PY(PyErr_Clear)();
            PyObject *py_result = CALL_PY(PyObject_GetAttr)(py_object, py_attribute);
            if (py_result)
            {
                result = PyObjectToQVariant(py_result);
                Py_DECREF(py_result);
            }
            else
            {
                CALL_PY(PyErr_Print)();
                CALL_PY(PyErr_Clear)();
            }
            Py_DECREF(py_attribute);
        }
        Py_DECREF(py_object);
    }

    return result;
}


bool PythonSupport::setAttribute(const QVariant &object, const QString &attribute, const QVariant &value)
{
    int result = 0;

    Python_ThreadBlock thread_block;

    PyObject *py_object = QVariantToPyObject(object);
    if (py_object)
    {
        PyObject *py_attribute = PyString_FromString(attribute.toLatin1().data());
        if (py_attribute)
        {
            PyObject *py_value = QVariantToPyObject(value);
            if (py_value)
            {
                CALL_PY(PyErr_Clear)();
                result = CALL_PY(PyObject_SetAttr)(py_object, py_attribute, py_value);
                Py_DECREF(py_value);
            }
            Py_DECREF(py_attribute);
        }
        Py_DECREF(py_object);
    }

    return result != -1;
}

QImage PythonSupport::imageFromRGBA(PyObject *ndarray_py)
{
    PyArrayObject *array = (PyArrayObject *)PyArray_ContiguousFromObject(ndarray_py, NPY_UINT32, 2, 2);
    if (array != NULL)
    {
        long width = PyArray_DIMS(array)[1];
        long height = PyArray_DIMS(array)[0];
        QImage image((int)width, (int)height, QImage::Format_ARGB32);
        for (int row=0; row<height; ++row)
            memcpy(image.scanLine(row), ((uint32_t *)PyArray_DATA(array)) + row*width, width*sizeof(uint32_t));
        Py_DECREF(array);
        return image;
    }
    return QImage();
}

QImage PythonSupport::imageFromArray(PyObject *ndarray_py, float display_limit_low, float display_limit_high, PyObject *lookup_table_ndarray)
{
    PyArrayObject *array = (PyArrayObject *)PyArray_ContiguousFromObject(ndarray_py, NPY_FLOAT32, 2, 2);
    if (array != NULL)
    {
        long width = PyArray_DIMS(array)[1];
        long height = PyArray_DIMS(array)[0];
        float m = display_limit_high != display_limit_low ? 255.0 / (display_limit_high - display_limit_low) : 1;
        QVector<QRgb> colorTable;
        if (lookup_table_ndarray != NULL)
        {
            PyArrayObject *lookup_table_array = (PyArrayObject *)PyArray_ContiguousFromObject(lookup_table_ndarray, NPY_UINT32, 1, 1);
            if (lookup_table_array != NULL)
            {
                uint32_t *lookup_table = ((uint32_t *)PyArray_DATA(lookup_table_array));
                for (int i=0; i<256; ++i)
                    colorTable.push_back(lookup_table[i]);
                Py_DECREF(lookup_table_array);
            }
        }
        if (colorTable.length() == 0)
            for (int i=0; i<256; ++i)
                colorTable.push_back(0xFF << 24 | i << 16 | i << 8 | i);
        if (false)
        {
            const QRgb *colorTableP = colorTable.constData();
            QImage image((int)width, (int)height, QImage::Format_ARGB32_Premultiplied);
            for (int row=0; row<height; ++row)
            {
                float *src = ((float *)PyArray_DATA(array)) + row*width;
                uint32_t *dst = (uint32_t *)image.scanLine(row);
                for (int col=0; col<width; ++col)
                {
                    float v = *src++;
                    if (v < display_limit_low)
                        *dst++ = colorTableP[0];
                    else if (v > display_limit_high)
                        *dst++ = colorTableP[255];
                    else
                    {
                        unsigned char vv = (unsigned char)((v - display_limit_low) * m);
                        *dst++ = colorTableP[vv];
                    }
                }
            }
            Py_DECREF(array);
            return image;
        }
        else
        {
            QImage image((int)width, (int)height, QImage::Format_Indexed8);
            for (int row=0; row<height; ++row)
            {
                float *src = ((float *)PyArray_DATA(array)) + row*width;
                uint8_t *dst = (uint8_t *)image.scanLine(row);
                for (int col=0; col<width; ++col)
                {
                    float v = *src++;
                    if (v < display_limit_low)
                        *dst++ = 0x00;
                    else if (v > display_limit_high)
                        *dst++ = 0xFF;
                    else
                        *dst++ = (unsigned char)((v - display_limit_low) * m);
                }
            }
            image.setColorTable(colorTable);
            Py_DECREF(array);
            return image;
        }
    }
    return QImage();
}

PyObject *PythonSupport::arrayFromImage(const QImage &image)
{
    Py_ssize_t dims[2];
    dims[0] = image.height();
    dims[1] = image.width();

    PyObject *obj = PyArray_SimpleNew(2, dims, NPY_UINT32);

    PyArrayObject *array = (PyArrayObject *)PyArray_ContiguousFromObject(obj, NPY_UINT32, 2, 2);

    if (array != NULL)
    {
        long height = PyArray_DIMS(array)[0];
        long width = PyArray_DIMS(array)[1];
        for (int row=0; row<height; ++row)
            memcpy(((uint32_t *)PyArray_DATA(array)) + row*width, image.scanLine(row), width*sizeof(uint32_t));

        Py_DECREF(array);

        return obj;
    }
    else
    {
        Py_DECREF(obj);

        return NULL;
    }
}

void PythonSupport::bufferRelease(Py_buffer *buffer)
{
    CALL_PY(PyBuffer_Release)(buffer);
}

void PythonSupport::setErrorString(const QString &error_string)
{
    CALL_PY(PyErr_SetString)(module_exception, error_string.toUtf8().constData());
}

PyObject *PythonSupport::getPyListFromStrings(const QStringList &strings)
{
    PyObject *py_list = CALL_PY(PyList_New)(0);

    Q_FOREACH(const QString &str, strings)
    {
        PyObject *py_str = PyString_FromString(str.toUtf8().data());
        CALL_PY(PyList_Append)(py_list, py_str); //reference to str stolen
    }

    return py_list;
}

PythonSupport::PyArg_ParseTupleFn PythonSupport::parse()
{
    return dynamic_PyArg_ParseTuple;
}

PythonSupport::Py_BuildValueFn PythonSupport::build()
{
    return dynamic_Py_BuildValue;
}

PyObject *PythonSupport::getNoneReturnValue()
{
    Py_INCREF(CALL_PY(Py_NoneGet)());
    return CALL_PY(Py_NoneGet)();
}

bool PythonSupport::isNone(PyObject *obj)
{
    return obj == CALL_PY(Py_NoneGet)();
}

PyObject *PythonSupport::createAndAddModule(PyModuleDef *moduledef)
{
    PyObject *m = CALL_PY(PyModule_Create2)(moduledef, PYTHON_ABI_VERSION); //borrowed reference
    CALL_PY(PyState_AddModule)(m, moduledef);
    return m;
}

void PythonSupport::prepareModuleException(const char *name)
{
    module_exception = CALL_PY(PyErr_NewException)(name, 0, 0);
    Py_INCREF(module_exception);
}

void PythonSupport::initializeModule(const char *name, CreateAndAddModuleFn fn)
{
    CALL_PY(PyImport_AppendInittab)(name, fn);
}

void PythonSupport::printAndClearErrors()
{
    if (CALL_PY(PyErr_Occurred)()) { CALL_PY(PyErr_Print)(); CALL_PY(PyErr_Clear)(); }
}

PyObject *PythonSupport::import(const char *name)
{
    return CALL_PY(PyImport_ImportModule)(name);
}
