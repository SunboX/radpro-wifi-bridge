// SPDX-FileCopyrightText: 2026 André Fiedler
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "Arduino.h"

class WebServer
{
public:
    void send(int status, const char *contentType, const String &body)
    {
        status_ = status;
        contentType_ = contentType;
        body_ = body;
    }

    int status_ = 0;
    String contentType_;
    String body_;
};

struct WiFiManager
{
    WebServer *server = nullptr;
};
