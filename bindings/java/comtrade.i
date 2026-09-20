%module(directors="1") ComtradeCoreNative
%{
#include "java_api.hpp"
#include "ai_java_api.hpp"
%}

// Map C++ exceptions and the facade's simple STL containers to Java types.
%include "std_except.i"
%include "std_string.i"
%include "std_vector.i"

namespace std {
    %template(DoubleVector) vector<double>;
    %template(IntVector) vector<int>;
}

%ignore ComtradeNativeRecord::nativeCfg;
%feature("director") ComtradeNativeRowCallback;

// Expose Java-safe facades, not chrono/vector<bool> implementation details.
%include "java_api.hpp"

// Optional AI module: ai_java_api.hpp is itself guarded by #ifdef COMTRADE_JAVA_AI_ENABLED, so this
// %include is a no-op unless CMake passed -DCOMTRADE_JAVA_AI_ENABLED to swig (see
// bindings/java/CMakeLists.txt), which it only does when COMTRADE_BUILD_AI is also ON.
#ifdef COMTRADE_JAVA_AI_ENABLED
// All-static facade: nothing for Java code to construct or destroy.
%nodefaultctor ComtradeNativeAI;
%include "ai_java_api.hpp"
#endif
