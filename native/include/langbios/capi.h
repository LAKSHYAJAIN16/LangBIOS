#ifndef LANGBIOS_CAPI_H
#define LANGBIOS_CAPI_H
/* Plain C ABI so Python's ctypes (or anything else) can call into the
 * real-BIOS engine without linking C++ directly. One entry point: give it
 * a natural-language command, get back a JSON result. */

#ifdef _WIN32
#define LANGBIOS_API __declspec(dllexport)
#else
#define LANGBIOS_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* utf8Text: a rule-parseable command, e.g. "enable secure boot".
 * outBuf/outBufLen: caller-owned buffer to receive a UTF-8 JSON object:
 *   {"ok": bool, "message": "...", "value": "..."}
 * Returns the number of bytes written (excluding the trailing NUL), or a
 * negative number if outBuf was too small (the magnitude is the required
 * size including the NUL, so the caller can retry with a bigger buffer). */
LANGBIOS_API int langbios_execute(const char* utf8Text, char* outBuf, int outBufLen);

#ifdef __cplusplus
}
#endif

#endif
