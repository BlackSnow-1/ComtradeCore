%module(directors="1") ComtradeCoreNative
%{
#include "java_api.hpp"
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
