#ifndef __INTERSECTION_CLS__
#define __INTERSECTION_CLS__

#include "dg_core.hpp"
#include "utils/python_embedding.hpp"

using namespace std;

namespace dg
{
    struct IntersectionResult
    {
        int cls;                  // 0: non-intersection, 1: intersection
        double confidence;  // 0 ~ 1
    };

    /**
    * @brief C++ Wrapper of Python module - IntersectionClassifier
    */
    class IntersectionClassifier: public PythonModuleWrapper
    {
    public:
        /**
        * Initialize the module
        * @return true if successful (false if failed)
        */
        bool initialize(const char* module_name = "intersection_cls", const char* module_path = "./../src/intersection_cls", const char* class_name = "IntersectionClassifier", const char* func_name_init = "initialize", const char* func_name_apply = "apply")
        {
            PyGILState_STATE state;
            bool ret;

            if (isThreadingEnabled()) state = PyGILState_Ensure();

            ret = _initialize(module_name, module_path, class_name, func_name_init, func_name_apply);

            if (isThreadingEnabled()) PyGILState_Release(state);

            return ret;
        }

        /**
        * Reset variables and clear the memory
        */
        void clear()
        {
            PyGILState_STATE state;

            if (isThreadingEnabled()) state = PyGILState_Ensure();

            _clear();

            if (isThreadingEnabled()) PyGILState_Release(state);
        }

        /**
        * Run once the module for a given input (support thread run)
        * @return true if successful (false if failed)
        */
        bool apply(cv::Mat image, dg::Timestamp t)
        {
            PyGILState_STATE state;
            bool ret;

            if (isThreadingEnabled()) state = PyGILState_Ensure();

            /* Call Python/C API functions here */
            ret = _apply(image, t);

            if (isThreadingEnabled()) PyGILState_Release(state);

            return ret;
        }

        /**
        * Run once the module for a given input
        * @return true if successful (false if failed)
        */
        bool _apply(cv::Mat image, dg::Timestamp t)
        {
            // Set function arguments
            int arg_idx = 0;
            PyObject* pArgs = PyTuple_New(2);

            // Image
            import_array();
            npy_intp dimensions[3] = { image.rows, image.cols, image.channels() };
            PyObject* pValue = PyArray_SimpleNewFromData(image.dims + 1, (npy_intp*)&dimensions, NPY_UINT8, image.data);
            if (!pValue) {
                fprintf(stderr, "IntersectionClassifier::apply() - Cannot convert argument1\n");
                return false;
            }
            PyTuple_SetItem(pArgs, arg_idx++, pValue);

            // Timestamp
            pValue = PyFloat_FromDouble(t);
            PyTuple_SetItem(pArgs, arg_idx++, pValue);

            // Call the method
            PyObject* pRet = PyObject_CallObject(m_pFuncApply, pArgs);
            if (pRet != NULL) {
                Py_ssize_t n_ret = PyTuple_Size(pRet);
                if (n_ret != 2)
                {
                    fprintf(stderr, "IntersectionClassifier::apply() - Wrong number of returns\n");
                    return false;
                }

                // intersection class & confidence
                pValue = PyTuple_GetItem(pRet, 0);
                m_intersect.cls = PyLong_AsLong(pValue);
                pValue = PyTuple_GetItem(pRet, 1);
                m_intersect.confidence = PyFloat_AsDouble(pValue);
            }
            else {
                PyErr_Print();
                fprintf(stderr, "IntersectionClassifier::apply() - Call failed\n");
                return false;
            }

            // Update Timestamp
            m_timestamp = t;

            // Clean up
            if(pRet) Py_DECREF(pRet);
            if(pArgs) Py_DECREF(pArgs);            

            return true;
        }

        void get(IntersectionResult& intersect)
        {
            intersect = m_intersect;
        }

        void get(IntersectionResult& intersect, Timestamp& ts)
        {
            intersect = m_intersect;
            ts = m_timestamp;
        }

    protected:
        IntersectionResult m_intersect;
        Timestamp m_timestamp = -1;
    };

} // End of 'dg'

#endif // End of '__INTERSECTION_CLS__'