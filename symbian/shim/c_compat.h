/* Force-included for C sources only (zlib). The SDK's OpenC headers use __SOFTFP on declarations such as
 * atof()/strtod(); it is only defined on the C++ include path, so plain C translation units fail with
 * "expected function body after function declarator". On soft-float targets it expands to nothing. */
#ifndef CAP32_SYMBIAN_C_COMPAT_H
#define CAP32_SYMBIAN_C_COMPAT_H
#ifndef __SOFTFP
#define __SOFTFP
#endif
#endif
