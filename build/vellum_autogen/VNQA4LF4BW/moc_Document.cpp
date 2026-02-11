/****************************************************************************
** Meta object code from reading C++ file 'Document.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/model/Document.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'Document.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_Document_t {
    const uint offsetsAndSize[10];
    char stringdata0[53];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_Document_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_Document_t qt_meta_stringdata_Document = {
    {
QT_MOC_LITERAL(0, 8), // "Document"
QT_MOC_LITERAL(9, 7), // "changed"
QT_MOC_LITERAL(17, 0), // ""
QT_MOC_LITERAL(18, 15), // "viewModeChanged"
QT_MOC_LITERAL(34, 18) // "Document::ViewMode"

    },
    "Document\0changed\0\0viewModeChanged\0"
    "Document::ViewMode"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_Document[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       2,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    0,   26,    2, 0x06,    1 /* Public */,
       3,    1,   27,    2, 0x06,    2 /* Public */,

 // signals: parameters
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 4,    2,

       0        // eod
};

void Document::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<Document *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->changed(); break;
        case 1: _t->viewModeChanged((*reinterpret_cast< std::add_pointer_t<Document::ViewMode>>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (Document::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Document::changed)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (Document::*)(Document::ViewMode );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&Document::viewModeChanged)) {
                *result = 1;
                return;
            }
        }
    }
}

const QMetaObject Document::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_Document.offsetsAndSize,
    qt_meta_data_Document,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_Document_t
, QtPrivate::TypeAndForceComplete<Document, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<Document::ViewMode, std::false_type>



>,
    nullptr
} };


const QMetaObject *Document::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Document::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_Document.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Document::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void Document::changed()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Document::viewModeChanged(Document::ViewMode _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
