#pragma once

#if defined(_WIN32)

    #if defined(DENT_BUILDING_DLL)
        #define DENT_API __declspec(dllexport)
    #else
        #define DENT_API __declspec(dllimport)
    #endif

#else

    #if defined(DENT_BUILDING_DLL)
        #define DENT_API __attribute__((visibility("default")))
    #else
        #define DENT_API
    #endif

#endif


#ifdef __cplusplus
extern "C" {
#endif


/*
 * Solve a DENT .dent model file and return the result as JSON.
 *
 * The returned string is allocated by DENT and must be released
 * with dent_free_string().
 *
 * Returns nullptr only if the API itself cannot allocate the
 * result string.
 */
DENT_API const char* dent_solve_file_json(
    const char* model_path
);


/*
 * Free a string returned by DENT.
 */
DENT_API void dent_free_string(
    const char* value
);


#ifdef __cplusplus
}
#endif