%module(package="comtrade") ComtradeCoreNative
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

// Expose only the stable Java facade, not every internal C++ data structure.
%include "java_api.hpp"
