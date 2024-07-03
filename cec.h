//#define DEBUG 1

#ifdef DEBUG
# define debug(...) printf("CEC DEBUG: " __VA_ARGS__)
#else
# define debug(...)
#endif

#define EVENT_LOG           0x0001
#define EVENT_KEYPRESS      0x0002
#define EVENT_COMMAND       0x0004
#define EVENT_CONFIG_CHANGE 0x0008
#define EVENT_ALERT         0x0010
#define EVENT_MENU_CHANGED  0x0020
#define EVENT_ACTIVATED     0x0040
#define EVENT_VALID         0x007F
#define EVENT_ALL           0x007F

#define RETURN_BOOL(arg) do { \
  bool result; \
  Py_BEGIN_ALLOW_THREADS \
  result = (arg); \
  Py_END_ALLOW_THREADS \
  PyObject * ret = (result)?Py_True:Py_False; \
  Py_INCREF(ret); \
  return ret; \
} while(0)

// cec_adapter_descriptor and DetectAdapters were introduced in 2.1.0
#if CEC_LIB_VERSION_MAJOR >= 3 || (CEC_LIB_VERSION_MAJOR >= 2 && CEC_LIB_VERSION_MINOR >= 1)
#define CEC_ADAPTER_TYPE cec_adapter_descriptor
#define CEC_FIND_ADAPTERS DetectAdapters
#define HAVE_CEC_ADAPTER_DESCRIPTOR 1
#else
#define CEC_ADAPTER_TYPE cec_adapter
#define CEC_FIND_ADAPTERS FindAdapters
#define HAVE_CEC_ADAPTER_DESCRIPTOR 0
#endif

int parse_physical_addr(const char * addr);
