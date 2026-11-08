#include "process_windows_internal.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define LAUNCH_UNITS 32767U
#define ARGUMENT_UNITS 16384U
#define LAUNCH_ENTRIES 64U

typedef struct WideText {
    wchar_t* data;
    size_t length;
} WideText;

typedef struct EnvironmentEntry {
    wchar_t* data;
    size_t name_length;
    size_t length;
} EnvironmentEntry;

AetherStatus aether_windows_process_error(DWORD error)
{
    switch (error) {
    case ERROR_SUCCESS:
        return AETHER_STATUS_OK;
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_INVALID_DRIVE:
    case ERROR_NOT_FOUND:
        return AETHER_STATUS_NOT_FOUND;
    case ERROR_ACCESS_DENIED:
    case ERROR_PRIVILEGE_NOT_HELD:
    case ERROR_ELEVATION_REQUIRED:
        return AETHER_STATUS_PERMISSION_DENIED;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return AETHER_STATUS_OUT_OF_MEMORY;
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
    case ERROR_FILENAME_EXCED_RANGE:
    case ERROR_NO_UNICODE_TRANSLATION:
    case ERROR_BAD_EXE_FORMAT:
    case ERROR_EXE_MACHINE_TYPE_MISMATCH:
        return AETHER_STATUS_INVALID_ARGUMENT;
    case ERROR_OPERATION_ABORTED:
        return AETHER_STATUS_CANCELLED;
    case ERROR_TIMEOUT:
    case WAIT_TIMEOUT:
        return AETHER_STATUS_TIMEOUT;
    case ERROR_NOT_SUPPORTED:
    case ERROR_CALL_NOT_IMPLEMENTED:
        return AETHER_STATUS_NOT_SUPPORTED;
    default:
        return AETHER_STATUS_IO_ERROR;
    }
}

/* Explicit byte counts reject embedded NUL and malformed UTF-8 before allocation. */
static AetherStatus wide_length(AetherStringView input, size_t limit, size_t* length)
{
    int units;
    *length = 0U;
    if (input.length == 0U) {
        return AETHER_STATUS_OK;
    }
    if (input.data == NULL || input.length > INT_MAX || input.length > limit * 4U ||
        memchr(input.data, '\0', input.length) != NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data,
                               (int)input.length, NULL, 0);
    if (units == 0) {
        return aether_windows_process_error(GetLastError());
    }
    if ((size_t)units > limit) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    *length = (size_t)units;
    return AETHER_STATUS_OK;
}

static AetherStatus wide_copy(AetherStringView input, wchar_t* output, size_t length)
{
    if (length != 0U &&
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data,
                            (int)input.length, output, (int)length) == 0) {
        return aether_windows_process_error(GetLastError());
    }
    output[length] = L'\0';
    return AETHER_STATUS_OK;
}

static AetherStatus wide_allocate(AetherStringView input, size_t limit, WideText* text)
{
    AetherStatus status = wide_length(input, limit, &text->length);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    text->data = malloc((text->length + 1U) * sizeof(*text->data));
    if (text->data == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    return wide_copy(input, text->data, text->length);
}

static bool separator(wchar_t value)
{
    return value == L'\\' || value == L'/';
}

static bool drive_absolute(const wchar_t* path, size_t length)
{
    return length >= 3U &&
        ((path[0] >= L'A' && path[0] <= L'Z') ||
         (path[0] >= L'a' && path[0] <= L'z')) &&
        path[1] == L':' && separator(path[2]);
}

static bool unc_absolute(const wchar_t* path, size_t length)
{
    size_t server = 0U;
    size_t share;
    while (server < length && !separator(path[server])) {
        ++server;
    }
    if (server == 0U || server == length) {
        return false;
    }
    share = server + 1U;
    return share < length && !separator(path[share]);
}

/* Accept fully qualified DOS/UNC paths, including their extended-length forms.
 * Device namespaces and drive-relative/root-relative paths are not launch paths. */
static bool absolute_path(const WideText* text)
{
    const wchar_t* path = text->data;
    size_t length = text->length;
    size_t start = 0U;
    bool absolute;
    if (length >= 4U && path[0] == L'\\' && path[1] == L'\\' &&
        path[2] == L'?' && path[3] == L'\\') {
        start = 4U;
        absolute = drive_absolute(path + start, length - start);
        if (!absolute && length >= 8U &&
            CompareStringOrdinal(path + start, 4, L"UNC\\", 4, TRUE) == CSTR_EQUAL) {
            absolute = unc_absolute(path + 8U, length - 8U);
        }
    } else {
        absolute = drive_absolute(path, length);
        if (!absolute && length >= 2U && separator(path[0]) && separator(path[1])) {
            absolute = unc_absolute(path + 2U, length - 2U);
        }
    }
    if (!absolute) {
        return false;
    }
    for (size_t i = start; i < length; ++i) {
        if (path[i] < L' ' || path[i] == L'"' || path[i] == L'<' ||
            path[i] == L'>' || path[i] == L'|' || path[i] == L'?' || path[i] == L'*') {
            return false;
        }
    }
    /* The Win32 device prefix must never be mistaken for an ordinary UNC host. */
    return !(length >= 4U && separator(path[0]) && separator(path[1]) &&
             path[2] == L'.' && separator(path[3]));
}

/* Always quote arguments. Backslashes are doubled only before quotes or the
 * closing quote, exactly as required by the Windows CRT command-line parser. */
static size_t quoted_argument(const WideText* text, wchar_t* output)
{
    size_t count = 0U;
    size_t slashes = 0U;
#define EMIT(value) do { if (output != NULL) { output[count] = (value); } ++count; } while (0)
    EMIT(L'"');
    for (size_t i = 0U; i < text->length; ++i) {
        wchar_t value = text->data[i];
        if (value == L'\\') {
            ++slashes;
            continue;
        }
        size_t copies = value == L'"' ? slashes * 2U + 1U : slashes;
        for (size_t j = 0U; j < copies; ++j) {
            EMIT(L'\\');
        }
        slashes = 0U;
        EMIT(value);
    }
    for (size_t i = 0U; i < slashes * 2U; ++i) {
        EMIT(L'\\');
    }
    EMIT(L'"');
#undef EMIT
    return count;
}

static AetherStatus build_command(
    AetherPlatformProcessInput input, const WideText* executable, wchar_t** output
)
{
    size_t lengths[LAUNCH_ENTRIES] = {0};
    size_t units = executable->length + 3U;
    size_t longest = 0U;
    wchar_t* scratch;
    AetherStatus status;
    if (input.argument_count > LAUNCH_ENTRIES) return AETHER_STATUS_INVALID_ARGUMENT;
    for (size_t i = 0U; i < input.argument_count; ++i) {
        AetherStringView argument = input.arguments[i];
        size_t slashes = 0U;
        size_t escapes = 0U;
        status = wide_length(argument, ARGUMENT_UNITS, &lengths[i]);
        if (status != AETHER_STATUS_OK) return status;
        if (lengths[i] > longest) longest = lengths[i];
        /* Only ASCII quote/backslash affect CRT escaping; UTF-8 continuation
         * bytes cannot equal either, so count without a second conversion. */
        for (size_t byte = 0U; byte < argument.length; ++byte) {
            if (argument.data[byte] == '\\') ++slashes;
            else {
                if (argument.data[byte] == '"') escapes += slashes + 1U;
                slashes = 0U;
            }
        }
        units += 3U + lengths[i] + escapes + slashes;
        if (units > LAUNCH_UNITS) return AETHER_STATUS_INVALID_ARGUMENT;
    }
    if (units > LAUNCH_UNITS) return AETHER_STATUS_INVALID_ARGUMENT;
    *output = malloc(units * sizeof(**output));
    if (*output == NULL) return AETHER_STATUS_OUT_OF_MEMORY;
    scratch = malloc((longest + 1U) * sizeof(*scratch));
    if (scratch == NULL) {
        free(*output);
        *output = NULL;
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    (*output)[0] = L'"';
    memcpy(*output + 1U, executable->data, executable->length * sizeof(**output));
    size_t cursor = executable->length + 1U;
    (*output)[cursor++] = L'"';
    for (size_t i = 0U; i < input.argument_count; ++i) {
        status = wide_copy(input.arguments[i], scratch, lengths[i]);
        if (status != AETHER_STATUS_OK) {
            free(scratch);
            free(*output);
            *output = NULL;
            return status;
        }
        (*output)[cursor++] = L' ';
        cursor += quoted_argument(&(WideText){scratch, lengths[i]}, *output + cursor);
    }
    (*output)[cursor] = L'\0';
    free(scratch);
    return AETHER_STATUS_OK;
}

static AetherStatus make_environment_entry(
    AetherProcessEnvironment input, EnvironmentEntry* entry
)
{
    size_t value_length;
    AetherStatus status = wide_length(input.name, LAUNCH_UNITS, &entry->name_length);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    if (entry->name_length == 0U || memchr(input.name.data, '=', input.name.length) != NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = wide_length(input.value, LAUNCH_UNITS, &value_length);
    if (status != AETHER_STATUS_OK) {
        return status;
    }
    entry->length = entry->name_length + value_length + 2U;
    if (entry->length >= LAUNCH_UNITS) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    entry->data = malloc(entry->length * sizeof(*entry->data));
    if (entry->data == NULL) {
        return AETHER_STATUS_OUT_OF_MEMORY;
    }
    status = wide_copy(input.name, entry->data, entry->name_length);
    if (status == AETHER_STATUS_OK) {
        entry->data[entry->name_length] = L'=';
        status = wide_copy(input.value, entry->data + entry->name_length + 1U, value_length);
    }
    return status;
}

static AetherStatus build_environment(AetherPlatformProcessInput input, wchar_t** output)
{
    EnvironmentEntry entries[LAUNCH_ENTRIES] = {0};
    size_t order[LAUNCH_ENTRIES] = {0};
    size_t units = input.environment_count == 0U ? 2U : 1U;
    AetherStatus status = AETHER_STATUS_OK;
    for (size_t i = 0U; i < input.environment_count; ++i) {
        status = make_environment_entry(input.environment[i], &entries[i]);
        if (status != AETHER_STATUS_OK) {
            goto done;
        }
        units += entries[i].length;
        if (units > LAUNCH_UNITS) {
            status = AETHER_STATUS_INVALID_ARGUMENT;
            goto done;
        }
        size_t position = i;
        for (size_t j = 0U; j < i; ++j) {
            const EnvironmentEntry* other = &entries[order[j]];
            int comparison = CompareStringOrdinal(entries[i].data, (int)entries[i].name_length,
                other->data, (int)other->name_length, TRUE);
            if (comparison == 0) {
                status = aether_windows_process_error(GetLastError());
                goto done;
            }
            if (comparison == CSTR_EQUAL) {
                status = AETHER_STATUS_INVALID_ARGUMENT;
                goto done;
            }
            if (comparison == CSTR_LESS_THAN && position == i) {
                position = j;
            }
        }
        for (size_t j = i; j > position; --j) {
            order[j] = order[j - 1U];
        }
        order[position] = i;
    }
    *output = malloc(units * sizeof(**output));
    if (*output == NULL) {
        status = AETHER_STATUS_OUT_OF_MEMORY;
        goto done;
    }
    size_t cursor = 0U;
    for (size_t i = 0U; i < input.environment_count; ++i) {
        const EnvironmentEntry* entry = &entries[order[i]];
        memcpy(*output + cursor, entry->data, entry->length * sizeof(**output));
        cursor += entry->length;
    }
    (*output)[cursor] = L'\0';
    if (input.environment_count == 0U) {
        (*output)[1] = L'\0';
    }
done:
    for (size_t i = 0U; i < input.environment_count; ++i) {
        free(entries[i].data);
    }
    return status;
}

void aether_windows_launch_dispose(AetherWindowsLaunch* launch)
{
    if (launch != NULL) {
        free(launch->executable);
        free(launch->command_line);
        free(launch->cwd);
        free(launch->environment);
        memset(launch, 0, sizeof(*launch));
    }
}

AetherStatus aether_windows_launch_build(
    AetherPlatformProcessInput input, AetherWindowsLaunch* out_launch
)
{
    WideText executable = {0};
    WideText cwd = {0};
    AetherStatus status;
    if (out_launch == NULL) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    memset(out_launch, 0, sizeof(*out_launch));
    if (input.argument_count > LAUNCH_ENTRIES || input.environment_count > LAUNCH_ENTRIES ||
        (input.argument_count != 0U && input.arguments == NULL) ||
        (input.environment_count != 0U && input.environment == NULL) ||
        input.executable.length == 0U || input.working_directory.length == 0U) {
        return AETHER_STATUS_INVALID_ARGUMENT;
    }
    status = wide_allocate(input.executable, LAUNCH_UNITS, &executable);
    out_launch->executable = executable.data;
    if (status != AETHER_STATUS_OK) {
        goto failed;
    }
    if (!absolute_path(&executable) || executable.length < 4U ||
        CompareStringOrdinal(executable.data + executable.length - 4U,
                             4, L".exe", 4, TRUE) != CSTR_EQUAL) {
        status = AETHER_STATUS_INVALID_ARGUMENT;
        goto failed;
    }
    status = wide_allocate(input.working_directory, LAUNCH_UNITS, &cwd);
    out_launch->cwd = cwd.data;
    if (status != AETHER_STATUS_OK) {
        goto failed;
    }
    if (!absolute_path(&cwd)) {
        status = AETHER_STATUS_INVALID_ARGUMENT;
        goto failed;
    }
    status = build_command(input, &executable, &out_launch->command_line);
    if (status == AETHER_STATUS_OK) {
        status = build_environment(input, &out_launch->environment);
    }
    if (status == AETHER_STATUS_OK) {
        return status;
    }
failed:
    aether_windows_launch_dispose(out_launch);
    return status;
}
