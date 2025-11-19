#include "FileSystem/BridgeFileSystem.h"

#include <LittleFS.h>
#include <FS.h>
#include "esp_err.h"
extern "C"
{
#include "esp_littlefs.h"
}

namespace
{
    static void dumpDirectory(Print &log, const String &path, File &dir)
    {
        File entry = dir.openNextFile();
        while (entry)
        {
            String entryPath = path + entry.name();
            log.print("  ");
            log.print(entryPath);
            if (entry.isDirectory())
            {
                log.println("/ (dir)");
                File sub = LittleFS.open(entryPath);
                if (sub)
                {
                    dumpDirectory(log, entryPath + "/", sub);
                    sub.close();
                }
            }
            else
            {
                log.print(" size=");
                log.println(entry.size());
            }
            entry = dir.openNextFile();
        }
    }

    static void dumpRoot(Print &log)
    {
        File root = LittleFS.open("/");
        if (!root || !root.isDirectory())
        {
            log.println("[LittleFS] <root unavailable>");
            return;
        }

        dumpDirectory(log, String("/"), root);
        root.close();
    }
}

namespace BridgeFileSystem
{
    bool mount(Print &log, const char *stage, bool formatOnFail)
    {
        log.print("[LittleFS] mount request (");
        log.print(stage ? stage : "<unknown>");
        log.println(")");
        log.print("[LittleFS] already mounted? ");
        log.println(esp_littlefs_mounted(kLabel) ? "yes" : "no");

        bool mounted = LittleFS.begin(formatOnFail, kBasePath, kMaxFiles, kLabel);
        log.print("[LittleFS] begin returned ");
        log.println(mounted ? "true" : "false");
        log.print("[LittleFS] mounted after begin? ");
        log.println(esp_littlefs_mounted(kLabel) ? "yes" : "no");

        logStats(log, stage);
        if (mounted)
            dumpTree(log, stage);
        return mounted;
    }

    void logStats(Print &log, const char *stage)
    {
        size_t total = 0;
        size_t used = 0;
        esp_err_t err = esp_littlefs_info(kLabel, &total, &used);
        log.print("[LittleFS] info (");
        log.print(stage ? stage : "<unknown>");
        log.print("): err=");
        log.println(esp_err_to_name(err));
        if (err == ESP_OK)
        {
            log.print("[LittleFS] total=");
            log.print(total);
            log.print(" bytes used=");
            log.println(used);
        }
    }

    void dumpTree(Print &log, const char *reason)
    {
        log.print("[LittleFS] Directory listing (");
        log.print(reason ? reason : "<unknown>");
        log.println("):");
        dumpRoot(log);
    }

    void dumpTree(Print &log, const __FlashStringHelper *reason)
    {
        log.print("[LittleFS] Directory listing (");
        log.print(reason ? reason : F("<unknown>"));
        log.println("):");
        dumpRoot(log);
    }
}
