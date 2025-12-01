#include <ApplicationServices/ApplicationServices.h>
#include <CoreServices/CoreServices.h>
#include <HIServices/Pasteboard.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cjson.h"

static const CGKeyCode KEYCODE_D = 0x02;
static const CGKeyCode KEYCODE_C = 0x08;
static CFMachPortRef g_event_tap = NULL;

static void simulate_cmd_c(void) {
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
    if (!source) {
        fprintf(stderr, "Failed to create event source for cmd+c.\n");
        return;
    }

    CGEventRef key_down = CGEventCreateKeyboardEvent(source, KEYCODE_C, true);
    CGEventRef key_up = CGEventCreateKeyboardEvent(source, KEYCODE_C, false);
    if (key_down) {
        CGEventSetFlags(key_down, kCGEventFlagMaskCommand);
        CGEventPost(kCGHIDEventTap, key_down);
    }
    if (key_up) {
        CGEventSetFlags(key_up, kCGEventFlagMaskCommand);
        CGEventPost(kCGHIDEventTap, key_up);
    }

    if (key_down) CFRelease(key_down);
    if (key_up) CFRelease(key_up);
    CFRelease(source);
}

static CGPoint current_mouse_location(void) {
    CGEventRef event = CGEventCreate(NULL);
    CGPoint point = CGPointMake(0, 0);
    if (event) {
        point = CGEventGetLocation(event);
        CFRelease(event);
    }
    return point;
}

static void perform_triple_click(CGPoint p) {
    CGEventRef mouse_event = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDown, p, kCGMouseButtonLeft);
    if (!mouse_event) {
        fprintf(stderr, "Failed to create mouse event.\n");
        return;
    }

    CGEventPost(kCGHIDEventTap, mouse_event);
    CGEventSetType(mouse_event, kCGEventLeftMouseUp);
    CGEventPost(kCGHIDEventTap, mouse_event);

    usleep(200000);

    CGEventSetIntegerValueField(mouse_event, kCGMouseEventClickState, 3);

    CGEventSetType(mouse_event, kCGEventLeftMouseDown);
    CGEventPost(kCGHIDEventTap, mouse_event);

    CGEventSetType(mouse_event, kCGEventLeftMouseUp);
    CGEventPost(kCGHIDEventTap, mouse_event);

    CFRelease(mouse_event);
}

static char *copy_string_from_data(CFDataRef data) {
    if (!data) {
        return NULL;
    }
    CFIndex length = CFDataGetLength(data);
    const UInt8 *bytes = CFDataGetBytePtr(data);
    if (!bytes || length <= 0) {
        return NULL;
    }
    char *result = (char *)malloc((size_t)length + 1);
    if (!result) {
        return NULL;
    }
    memcpy(result, bytes, (size_t)length);
    result[length] = '\0';
    return result;
}

static char *read_clipboard_text(void) {
    PasteboardRef clipboard = NULL;
    OSStatus status = PasteboardCreate(kPasteboardClipboard, &clipboard);
    if (status != noErr || !clipboard) {
        fprintf(stderr, "Failed to access pasteboard: %d\n", (int)status);
        return NULL;
    }

    PasteboardSynchronize(clipboard);

    ItemCount count = 0;
    status = PasteboardGetItemCount(clipboard, &count);
    if (status != noErr || count == 0) {
        CFRelease(clipboard);
        return NULL;
    }

    PasteboardItemID item_id = 0;
    status = PasteboardGetItemIdentifier(clipboard, 1, &item_id);
    if (status != noErr) {
        CFRelease(clipboard);
        return NULL;
    }

    CFStringRef flavors[] = {
        kUTTypeUTF8PlainText,
        kUTTypePlainText
    };

    CFDataRef data = NULL;
    for (size_t i = 0; i < sizeof(flavors) / sizeof(flavors[0]); ++i) {
        status = PasteboardCopyItemFlavorData(clipboard, item_id, flavors[i], &data);
        if (status == noErr && data) {
            break;
        }
    }

    char *result = copy_string_from_data(data);

    if (data) CFRelease(data);
    if (clipboard) CFRelease(clipboard);
    return result;
}

static void write_json_to_tmp(const char *gem, const char *context) {
    cJSON *root = cJSON_CreateObject();
    if (!root) {
        fprintf(stderr, "Failed to create JSON object.\n");
        return;
    }

    const char *safe_gem = gem ? gem : "";
    const char *safe_context = context ? context : "";

    if (!cJSON_AddStringToObject(root, "gem", safe_gem) ||
        !cJSON_AddStringToObject(root, "context", safe_context)) {
        fprintf(stderr, "Failed to populate JSON.\n");
        cJSON_Delete(root);
        return;
    }

    char *json = cJSON_PrintUnformatted(root);
    if (!json) {
        fprintf(stderr, "Failed to serialize JSON.\n");
        cJSON_Delete(root);
        return;
    }

    FILE *file = fopen("/tmp/gem.json", "w");
    if (!file) {
        fprintf(stderr, "Unable to open /tmp/gem.json for writing.\n");
        free(json);
        cJSON_Delete(root);
        return;
    }

    fputs(json, file);
    fclose(file);

    free(json);
    cJSON_Delete(root);
}

static void perform_action_sequence(void) {
    simulate_cmd_c();
    char *gem_text = read_clipboard_text();

    CGPoint point = current_mouse_location();
    perform_triple_click(point);

    simulate_cmd_c();
    char *context_text = read_clipboard_text();

    write_json_to_tmp(gem_text ? gem_text : "", context_text ? context_text : "");

    free(gem_text);
    free(context_text);
}

static CGEventRef event_callback(CGEventTapProxy proxy, CGEventType type, CGEventRef event, void *refcon) {
    (void)proxy;
    (void)refcon;

    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
        if (g_event_tap) {
            CGEventTapEnable(g_event_tap, true);
        }
        return event;
    }

    if (type != kCGEventKeyDown) {
        return event;
    }

    CGEventFlags flags = CGEventGetFlags(event);
    CGKeyCode keycode = (CGKeyCode)CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if ((flags & kCGEventFlagMaskCommand) && keycode == KEYCODE_D) {
        perform_action_sequence();
    }

    return event;
}

int main(void) {
    CGEventMask mask = CGEventMaskBit(kCGEventKeyDown);
    g_event_tap = CGEventTapCreate(
        kCGSessionEventTap,
        kCGHeadInsertEventTap,
        kCGEventTapOptionDefault,
        mask,
        event_callback,
        NULL);

    if (!g_event_tap) {
        fprintf(stderr, "Failed to create event tap. Ensure accessibility permissions are granted.\n");
        return EXIT_FAILURE;
    }

    CFRunLoopSourceRef run_loop_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, g_event_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), run_loop_source, kCFRunLoopCommonModes);
    CGEventTapEnable(g_event_tap, true);

    printf("native-helper daemon running. Listening for Cmd+D...\n");
    CFRunLoopRun();

    CFRelease(run_loop_source);
    CFRelease(g_event_tap);
    return EXIT_SUCCESS;
}
