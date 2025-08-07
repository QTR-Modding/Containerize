// Author: A. Ege Yilmaz
// Year: 2025

#pragma once
#include <windows.h>

namespace DAF_API {

    constexpr int MAJOR = 0;
    constexpr int MINOR = 1;

    #define DECLARE_API_FUNC_EX(                               \
        localName,   /* The name you call in your code */      \
        hostName,    /* The name actually exported by DLL */   \
        returnType,                                            \
        defaultValue,                                           \
        signature,   /* Parameter list in parentheses */        \
        callArgs     /* Just the parameter names in parentheses */ \
    )                                                          \
    using _##localName = returnType (*) signature;             \
    [[nodiscard]] inline returnType localName signature {      \
        static auto dllHandle = GetModuleHandle(L"DynamicAnimationFrameworkSE"); \
        if (!dllHandle) {                                   \
            return defaultValue;                               \
        }                                                      \
        static auto func =                                     \
            reinterpret_cast<_##localName>(GetProcAddress(dllHandle, hostName)); \
        if (func) {                                            \
            return func callArgs;                              \
        }                                                      \
        return defaultValue;                                   \
    }

	using AnimEventID = uint32_t;

    DECLARE_API_FUNC_EX(RequestEventID,"ProcessRequestEventID",AnimEventID,0,(const char* a_event,int a_major, int a_minor),(a_event, a_major, a_minor));

    inline AnimEventID RequestEventID(const char* a_event) {
        return RequestEventID(a_event, MAJOR, MINOR);
    }

    DECLARE_API_FUNC_EX(
        SendEvent,                          /* localName */
        "ProcessSendEvent",                     /* hostName */
        int,                                       /* returnType */
        0,                                      /* defaultValue */
        (AnimEventID a_event, uint32_t a_actor, uint32_t a_form), /* signature */
        (a_event, a_actor, a_form)         /* callArgs */
    );

};