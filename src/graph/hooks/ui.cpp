#include "graph/hooks/ui.h"
#include "graph/hooks/hooks.h"
#include "graph/node/node.h"
#include "graph/datum/datum.h"

#include "ui/viewport/viewport_scene.h"

#include "control/point.h"
#include "control/wireframe.h"

using namespace boost::python;

template <typename T, typename O>
QVector<T> ScriptUIHooks::_extractList(O obj)
{
    QVector<T> out;
    for (int i=0; i < len(obj); ++i)
    {
        extract<T> e(obj[i]);
        if (!e.check())
            throw hooks::HookException(
                    "Failed to extract data from object.");
        out << e();
    }
    return out;
}

template <typename T>
QVector<T> ScriptUIHooks::extractList(object obj)
{
    auto tuple_ = extract<tuple>(obj);
    if (tuple_.check())
        return _extractList<T>(tuple_());

    auto list_ = extract<list>(obj);
    if (list_.check())
        return _extractList<T>(list_());

    throw hooks::HookException("Input must be a list or a tuple");
}

QVector<QVector3D> ScriptUIHooks::extractVectors(object obj)
{
    QVector<QVector3D> out;

    // Try to extract a bunch of tuples from the top-level list.
    QVector<tuple> tuples;
    bool got_tuple = true;
    try {
        tuples = extractList<tuple>(obj);
    } catch (hooks::HookException e) {
        got_tuple = false;
    }

    if (got_tuple)
    {
        for (auto t : tuples)
        {
            auto v = extractList<float>(extract<object>(t)());
            if (v.length() != 3)
                throw hooks::HookException("Position data must have three terms.");
            out << QVector3D(v[0], v[1], v[2]);
        }
        return out;
    }

    // Try to extract a bunch of lists from the top-level list.
    QVector<list> lists;
    bool got_list = true;
    try {
        lists = extractList<list>(obj);
    } catch (hooks::HookException e) {
        got_list = false;
    }

    if (got_list)
    {
        for (auto l : lists)
        {
            auto v = extractList<float>(extract<object>(l)());
            if (v.length() != 3)
                throw hooks::HookException("Position data must have three terms.");
            out << QVector3D(v[0], v[1], v[2]);
        }
        return out;
    }

    throw hooks::HookException(
            "Position data must be a list of 3-element lists");
}

long ScriptUIHooks::getInstruction()
{
    // Get the current bytecode instruction
    // (used to uniquely identify calls to this function)
    auto inspect_module = PyImport_ImportModule("inspect");
    auto frame = PyObject_CallMethod(inspect_module, "currentframe", NULL);
    auto f_lineno = PyObject_GetAttrString(frame, "f_lineno");
    long lineno = PyLong_AsLong(f_lineno);
    Q_ASSERT(!PyErr_Occurred());

    // Clean up these objects immediately
    for (auto o : {inspect_module, frame, f_lineno})
        Py_DECREF(o);

    return lineno;
}

QString ScriptUIHooks::getDatum(PyObject* obj)
{
    for (auto d : node->findChildren<Datum*>(
                QString(), Qt::FindDirectChildrenOnly))
        if (d->getValue() == obj)
            return d->objectName();
    return QString();
}

object ScriptUIHooks::point(tuple args, dict kwargs)
{
    ScriptUIHooks& self = extract<ScriptUIHooks&>(args[0])();

    // Find the instruction at which this callback happened
    // (used as a unique identifier for the Control).
    long lineno = getInstruction();

    if (len(args) != 4)
        throw hooks::HookException("Expected x, y, z as arguments");

    // Extract x, y, z as floats from first three arguments.
    extract<float> x_(args[1]);
    extract<float> y_(args[2]);
    extract<float> z_(args[3]);
    if (!x_.check())
        throw hooks::HookException("x value must be a number");
    if (!y_.check())
        throw hooks::HookException("y value must be a number");
    if (!z_.check())
        throw hooks::HookException("z value must be a number");
    float x = x_();
    float y = y_();
    float z = z_();

    // If this callback happened because we're dragging the generated
    // Control, don't delete it; otherwise, clear it to make room for
    // an updated Control.
    ControlPoint* p = dynamic_cast<ControlPoint*>(
            self.scene->getControl(self.node, lineno));
    if (p && !p->isDragging())
    {
        p->deleteLater();
        p = NULL;
    }

    if (!p)
    {
        if (kwargs.has_key("drag"))
        {
            auto d = extract<object>(kwargs["drag"])().ptr();
            Py_INCREF(d);
            p = new ControlPoint(self.node, d);
        }
        else
        {
            if (kwargs.has_key("relative"))
                throw hooks::HookException(
                        "Can't provide 'relative' argument "
                        "without drag function");

            // Try to automatically generate a drag function by looking to see
            // if the x, y, z arguments match datum values; if so, make a drag
            // function that drags these datums.
            auto px = self.getDatum(extract<object>(args[1])().ptr());
            auto py = self.getDatum(extract<object>(args[2])().ptr());
            auto pz = self.getDatum(extract<object>(args[3])().ptr());
            QString drag =
                "def drag(this, x, y, z):\n"
                "    pass\n";
            if (!px.isNull())
                drag += QString("    this.%1 += x\n").arg(px);
            if (!py.isNull())
                drag += QString("    this.%1 += y\n").arg(py);
            if (!pz.isNull())
                drag += QString("    this.%1 += z\n").arg(pz);

            auto globals = PyDict_New();
            PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());
            auto locals = Py_BuildValue("{}");
            auto out = PyRun_String(
                    drag.toStdString().c_str(),
                    Py_file_input, globals, locals);
            Q_ASSERT(!PyErr_Occurred());

            auto drag_func = PyDict_GetItemString(locals, "drag");
            Q_ASSERT(drag_func);
            Py_INCREF(drag_func);

            // Clean up references
            for (auto obj : {globals, locals, out})
                Py_DECREF(obj);

            p = new ControlPoint(self.node, drag_func);
        }

        self.scene->registerControl(self.node, lineno, p);
    }

    const float r = getFloat(p->getR(), kwargs, "r");
    const QColor color = getColor(p->getColor(), kwargs);
    const bool relative = getBool(p->getRelative(), kwargs, "relative");

    p->update(x, y, z, r, color, relative);
    p->touch();

    // Return None
    return object();
}

object ScriptUIHooks::wireframe(tuple args, dict kwargs)
{
    ScriptUIHooks& self = extract<ScriptUIHooks&>(args[0])();

    // Find the instruction at which this callback happened
    // (used as a unique identifier for the Control).
    long lineno = getInstruction();

    if (len(args) != 2)
        throw hooks::HookException("Expected list of 3-tuples as argument");

    auto v = extractVectors(extract<object>(args[1])());
    if (v.isEmpty())
        throw hooks::HookException("Wireframe must have at least one point");

    ControlWireframe* w = dynamic_cast<ControlWireframe*>(
            self.scene->getControl(self.node, lineno));
    if (!w)
    {
        w = new ControlWireframe(self.node);
        self.scene->registerControl(self.node, lineno, w);
    }

    const float t = getFloat(w->getT(), kwargs, "t");
    const QColor color = getColor(w->getColor(), kwargs);
    const bool close = getBool(w->getClose(), kwargs, "close");
    w->update(v, t, color, close);
    w->touch();

    return object();
}

float ScriptUIHooks::getFloat(float v, dict kwargs, std::string key)
{
    if (kwargs.has_key(key))
    {
        extract<float> v_(kwargs[key]);
        if (!v_.check())
            throw hooks::HookException(key + " value must be a number");
        v = v_();
    }
    return v;
}

bool ScriptUIHooks::getBool(bool b, dict kwargs, std::string key)
{
    if (kwargs.has_key(key))
    {
        extract<bool> b_(kwargs[key]);
        if (!b_.check())
            throw hooks::HookException(key + " value must be a boolean");
        b = b_();
    }
    return b;
}

QColor ScriptUIHooks::getColor(QColor color, dict kwargs)
{
    if (kwargs.has_key("color"))
    {
        auto rgb = extractList<int>(kwargs["color"]);
        if (rgb.length() != 3)
            throw hooks::HookException("color tuple must have three values");
        color = QColor(rgb[0], rgb[1], rgb[2]);
    }
    return color;
}