//
//  main.c
//  test-kextmanager
//
//  Created by RoyalGraphX on 5/12/25.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/kext/KextManager.h>

// Set this to true to focus on non-Apple, non-NVIDIA, non-AMD kexts, and the __kernel__ entry.
// Set to false to show all currently loaded kexts exposed to KextManagerCopyLoadedKextInfo.
static const Boolean FOCUS_NON_APPLE = true; // Change this to false to see all kexts

// Helper function to convert CFStringRef to a C string (UTF-8)
// Remember to free the returned string if it's not NULL
const char * cfStringRefToCString(CFStringRef cfStr) {
    if (cfStr == NULL) {
        return NULL;
    }
    if (CFGetTypeID(cfStr) != CFStringGetTypeID()) {
        return NULL; // Not a CFString
    }
    CFIndex length = CFStringGetLength(cfStr);
    // Calculate the required buffer size for UTF-8 encoding + null terminator
    CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
    char *buffer = (char *)malloc(maxSize);
    if (buffer) {
        if (CFStringGetCString(cfStr, buffer, maxSize, kCFStringEncodingUTF8)) {
            return buffer;
        }
        free(buffer); // Failed to convert
    }
    return NULL;
}

// Helper function to print CFTypeRef values based on their type
void printCFTypeRef(CFStringRef keyCF, CFTypeRef value) {
    if (value == NULL) {
        // Don't print anything if the value is NULL
        return;
    }

    // Convert the key to a C string for printing
    const char *keyStr = cfStringRefToCString(keyCF);
    if (!keyStr) {
        fprintf(stderr, "Failed to convert key CFString to C string.\n");
        // We should still try to print the value if possible, but the key name is missing
        // For simplicity in this example, we'll return.
        return;
    }

    CFTypeID typeID = CFGetTypeID(value);

    printf("  %s: ", keyStr); // Print the key

    if (typeID == CFStringGetTypeID()) {
        const char *valueStr = cfStringRefToCString((CFStringRef)value);
        if (valueStr) {
            printf("%s\n", valueStr);
            free((void*)valueStr); // Free the converted string
        } else {
            printf("[String Conversion Failed]\n");
        }
    } else if (typeID == CFNumberGetTypeID()) {
        CFNumberRef numberCF = (CFNumberRef)value;
        CFNumberType numberType = CFNumberGetType(numberCF);

        // Try to get the number as various types for printing
        if (numberType == kCFNumberSInt32Type || numberType == kCFNumberIntType) {
             int intValue;
             if (CFNumberGetValue(numberCF, kCFNumberIntType, &intValue)) {
                 printf("%d\n", intValue);
             } else {
                 printf("[Number Conversion Failed (int)]\n");
             }
        } else if (numberType == kCFNumberSInt64Type || numberType == kCFNumberLongLongType) {
             long long llValue;
             if (CFNumberGetValue(numberCF, kCFNumberLongLongType, &llValue)) {
                 // Print addresses in hex, other numbers in decimal
                 if (CFStringCompare(keyCF, CFSTR("OSBundleLoadAddress"), 0) == kCFCompareEqualTo) {
                      printf("0x%llx\n", llValue);
                 } else {
                      printf("%lld\n", llValue);
                 }
             } else {
                 printf("[Number Conversion Failed (long long)]\n");
             }
        } else if (numberType == kCFNumberFloatType || numberType == kCFNumberDoubleType) {
             double dblValue;
             if (CFNumberGetValue(numberCF, kCFNumberDoubleType, &dblValue)) {
                 printf("%f\n", dblValue);
             } else {
                 printf("[Number Conversion Failed (double)]\n");
             }
        }
        // Could add more number types if needed
         else {
             printf("[Number of Unknown Type]\n");
         }
    } else if (typeID == CFBooleanGetTypeID()) {
        Boolean boolValue = CFBooleanGetValue((CFBooleanRef)value);
        printf("%s\n", boolValue ? "true" : "false");
    } else if (typeID == CFDataGetTypeID()) {
        CFDataRef dataValue = (CFDataRef)value;
        CFIndex dataLength = CFDataGetLength(dataValue);
        printf("[Data, length %ld]", dataLength);
        const UInt8 *bytes = CFDataGetBytePtr(dataValue);
        if (bytes && dataLength > 0) {
             printf(" (first byte: 0x%02x)", bytes[0]);
        }
        printf("\n");
    } else if (typeID == CFArrayGetTypeID()) {
        CFArrayRef arrayValue = (CFArrayRef)value;
        CFIndex arrayCount = CFArrayGetCount(arrayValue);
        printf("[Array, count %ld]", arrayCount);

        if (CFStringCompare(keyCF, CFSTR("OSBundleDependencies"), 0) == kCFCompareEqualTo) {
             printf(": [Load Tags]");
        } else if (CFStringCompare(keyCF, CFSTR("OSBundleClasses"), 0) == kCFCompareEqualTo) {
             printf(": [Class Dictionaries]");
        }
        printf("\n");
    } else {
        CFStringRef summary = CFCopyDescription(value);
        const char *summaryStr = NULL;
        if (summary) {
             summaryStr = cfStringRefToCString(summary);
        }
        printf("[Unknown Type: %lu] %s\n", (unsigned long)typeID, summaryStr ? summaryStr : "[Conversion Failed]");
        if (summaryStr) free((void*)summaryStr);
        if (summary) CFRelease(summary);
    }

    free((void*)keyStr); // Free the converted key string
}


int main(int argc, const char * argv[]) {
    printf("Attempting to get loaded kext information using KextManagerCopyLoadedKextInfo...\n");

    // Use KextManagerCopyLoadedKextInfo as declared in KextManager.h
    // Passing NULL for both arguments gets info for ALL loaded kexts and ALL info keys.
    CFDictionaryRef loadedKextsInfo = KextManagerCopyLoadedKextInfo(NULL, NULL);

    if (loadedKextsInfo == NULL) {
        fprintf(stderr, "Failed to get loaded kext information. Make sure you have permissions (e.g., run with sudo).\n");
        return 1;
    }

    if (CFGetTypeID(loadedKextsInfo) != CFDictionaryGetTypeID()) {
        fprintf(stderr, "KextManagerCopyLoadedKextInfo did not return a CFDictionaryRef.\n");
        CFRelease(loadedKextsInfo);
        return 1;
    }

    printf("Successfully retrieved kext information. Processing...\n");

    CFIndex kextCount = CFDictionaryGetCount(loadedKextsInfo);
    printf("Found %ld loaded kexts.\n", kextCount);

    // Get keys and values from the main dictionary
    CFIndex dictCount = CFDictionaryGetCount(loadedKextsInfo);
    CFTypeRef *keys = (CFTypeRef *)malloc(dictCount * sizeof(CFTypeRef));
    CFTypeRef *values = (CFTypeRef *)malloc(dictCount * sizeof(CFTypeRef));

    if (!keys || !values) {
        fprintf(stderr, "Failed to allocate memory for keys/values arrays.\n");
        if (keys) free(keys);
        if (values) free(values);
        CFRelease(loadedKextsInfo);
        return 1;
    }

    CFDictionaryGetKeysAndValues(loadedKextsInfo, (const void **)keys, (const void **)values);

    // Define an array of CFStringRef keys for the inner kext info dictionaries
    CFStringRef kextInfoKeys[] = {
        CFSTR("CFBundleIdentifier"),
        CFSTR("CFBundleVersion"),
        CFSTR("OSBundleCompatibleVersion"),
        CFSTR("OSBundleIsInterface"),
        CFSTR("OSKernelResource"),
        CFSTR("OSBundleCPUType"),
        CFSTR("OSBundleCPUSubtype"),
        CFSTR("OSBundlePath"),
        CFSTR("OSBundleExecutablePath"),
        CFSTR("OSBundleUUID"),
        CFSTR("OSBundleStarted"),
        CFSTR("OSBundlePrelinked"),
        CFSTR("OSBundleLoadTag"),
        CFSTR("OSBundleLoadAddress"),
        CFSTR("OSBundleLoadSize"),
        CFSTR("OSBundleWiredSize"),
        CFSTR("OSBundleDependencies"),
        CFSTR("OSBundleRetainCount"),
        CFSTR("OSBundleClasses")
    };
    CFIndex numKextInfoKeys = sizeof(kextInfoKeys) / sizeof(kextInfoKeys[0]);

    int displayedKextCount = 0;

    // Iterate through each kext entry
    for (CFIndex i = 0; i < dictCount; ++i) {
        // The key is the bundle ID (as a CFStringRef)
        if (CFGetTypeID(keys[i]) != CFStringGetTypeID()) {
            fprintf(stderr, "Skipping dictionary entry with non-string key type.\n");
            continue;
        }
        CFStringRef kextBundleIDCF = (CFStringRef)keys[i];

        // --- Filtering Logic ---
        if (FOCUS_NON_APPLE) {
            const char *kextBundleID = cfStringRefToCString(kextBundleIDCF);
            if (kextBundleID) {
                 // Check for the specific __kernel__ bundle ID
                if (strcmp(kextBundleID, "__kernel__") == 0) {
                    free((void*)kextBundleID); // Free the temporary string
                    continue; // Skip the __kernel__ entry
                }
                // Check if the bundle ID starts with known Apple/NVIDIA/AMD prefixes
                if (strncmp(kextBundleID, "com.apple.", strlen("com.apple.")) == 0 ||
                    strncmp(kextBundleID, "com.nvidia.", strlen("com.nvidia.")) == 0 ||
                    strncmp(kextBundleID, "com.amd.", strlen("com.amd.")) == 0) {
                    free((void*)kextBundleID); // Free the temporary string
                    continue; // Skip this kext if it matches an excluded prefix
                }
                free((void*)kextBundleID); // Free the temporary string if not skipped
            } else {
                 fprintf(stderr, "Warning: Could not convert bundle ID CFString for filtering.\n");
                 // Decide whether to skip or include if conversion fails.
                 // Let's include it by default if we can't filter it.
            }
        }
        // --- End Filtering Logic ---


        // The value is another dictionary with details
        if (CFGetTypeID(values[i]) != CFDictionaryGetTypeID()) {
            const char *tempKeyStr = cfStringRefToCString(kextBundleIDCF);
            fprintf(stderr, "Expected a dictionary for kext: %s, but got something else (Type ID: %lu).\n",
                    tempKeyStr ? tempKeyStr : "UNKNOWN_KEY_TYPE", (unsigned long)CFGetTypeID(values[i]));
             if (tempKeyStr) free((void*)tempKeyStr);
            continue;
        }
        CFDictionaryRef kextInfo = (CFDictionaryRef)values[i];

        // Print the kext information
        const char *kextBundleID = cfStringRefToCString(kextBundleIDCF);

        if (kextBundleID) {
            printf("--- Kext Bundle ID: %s ---\n", kextBundleID);
            free((void*)kextBundleID); // Free the bundle ID string

            // Now iterate through the known info keys and print their values
            for (CFIndex j = 0; j < numKextInfoKeys; ++j) {
                CFStringRef currentInfoKey = kextInfoKeys[j];
                CFTypeRef infoValue = CFDictionaryGetValue(kextInfo, currentInfoKey);

                // Use the helper function to print the key-value pair
                printCFTypeRef(currentInfoKey, infoValue);
            }
            printf("\n"); // Add a blank line after each kext's info
            displayedKextCount++; // Increment count only if displayed

        } else {
             fprintf(stderr, "Failed to convert kext bundle ID CFString to C string for printing.\n");
        }
    }

    free(keys); // Free the allocated keys array
    free(values); // Free the allocated values array
    CFRelease(loadedKextsInfo); // Release the main dictionary returned by KextManager

    printf("Finished listing %d kexts.\n", displayedKextCount);
    return 0;
}
