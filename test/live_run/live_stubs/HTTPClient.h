#ifndef MLB_LIVE_STUB_HTTPCLIENT_H
#define MLB_LIVE_STUB_HTTPCLIENT_H

// Stand-in for ESP32's <HTTPClient.h>, for test/live_run only (see
// Arduino.h in this directory). Unlike test/arduino_stubs/HTTPClient.h
// (which always returns -1 -- deliberately never makes a real request),
// this one makes a REAL HTTPS request by shelling out to `curl`. That's
// a deliberate choice over linking libcurl or hand-rolling TLS: `curl`
// is already on your machine, this is test-only tooling that never ships,
// and it keeps this file to a few dozen lines instead of pulling in a
// networking dependency. macOS/Linux only (uses popen/mkstemp) -- fine,
// since this harness is a desktop debugging aid, not something that
// needs to be portable to Windows or run on the ESP32.
//
// The response body is written to a temp file rather than captured
// straight off curl's stdout so a `-w` status-code marker can never be
// mistaken for JSON content (or vice versa) -- curl writes the body to
// -o <tmpfile> and ONLY the status code to stdout.

#include "Arduino.h"
#include "WiFi.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h> // WIFEXITED(), WEXITSTATUS()
#include <unistd.h>   // close(), mkstemp()

#define HTTP_CODE_OK 200

// A Stream-backed in-memory buffer holding the response body, so
// deserializeJson(doc, http.getStream(), filter) reads it exactly the
// way it reads a real WiFiClient stream.
class HttpBufferStream : public Stream
{
  public:
    void setData(std::string data)
    {
        _data = std::move(data);
        _pos = 0;
    }

    int available() override { return (int)(_data.size() - _pos); }

    int read() override { return _pos < _data.size() ? (unsigned char)_data[_pos++] : -1; }

    size_t readBytes(char *buffer, size_t length) override
    {
        size_t remaining = _data.size() - _pos;
        size_t n = length < remaining ? length : remaining;
        memcpy(buffer, _data.data() + _pos, n);
        _pos += n;
        return n;
    }

    const std::string &data() const { return _data; }

  private:
    std::string _data;
    size_t _pos = 0;
};

class HTTPClient
{
  public:
    void setTimeout(uint32_t ms) { _timeoutMs = ms; }

    bool begin(const String &url)
    {
        _url = url.c_str();
        return !_url.empty();
    }

    // Real network call. Returns the HTTP status code, or -1 if curl
    // itself couldn't be run/completed (timeout, DNS failure, no
    // network, curl missing, ...).
    int GET()
    {
        char tmpl[] = "/tmp/mlb_live_run_XXXXXX";
        int fd = mkstemp(tmpl);
        if (fd == -1)
        {
            Serial.printf("[HTTPClient] mkstemp failed\n");
            return -1;
        }
        close(fd);

        unsigned long timeoutSec = (_timeoutMs / 1000) + 1;
        std::ostringstream cmd;
        cmd << "curl -sS -m " << timeoutSec << " -o '" << tmpl << "' -w '%{http_code}' '" << _url << "' 2>&1";

        FILE *pipe = popen(cmd.str().c_str(), "r");
        if (!pipe)
        {
            Serial.printf("[HTTPClient] popen failed for curl\n");
            remove(tmpl);
            return -1;
        }
        char statusBuf[64] = {0};
        size_t n = fread(statusBuf, 1, sizeof(statusBuf) - 1, pipe);
        statusBuf[n] = '\0';
        int waitStatus = pclose(pipe);
        // pclose() returns a raw wait(2) status, not curl's exit code
        // directly -- WEXITSTATUS() unpacks it. (Without this, a failure
        // printed a meaningless number like "exit 14336" instead of
        // curl's real exit code, e.g. 56 for "couldn't connect".)
        int curlExitCode = WIFEXITED(waitStatus) ? WEXITSTATUS(waitStatus) : -1;

        std::ifstream bodyFile(tmpl, std::ios::binary);
        std::ostringstream bodyStream;
        bodyStream << bodyFile.rdbuf();
        _stream.setData(bodyStream.str());
        remove(tmpl);

        if (curlExitCode != 0)
        {
            // curl itself failed (network down, DNS, timeout, ...) --
            // statusBuf is curl's own error text in this case, not a
            // status code, which is exactly what you want to see.
            Serial.printf("[HTTPClient] curl failed (exit %d): %s\n", curlExitCode, statusBuf);
            return -1;
        }

        return atoi(statusBuf);
    }

    Stream &getStream() { return _stream; }

    String getString() { return String(_stream.data()); }

    void end() {}

  private:
    std::string _url;
    uint32_t _timeoutMs = 8000;
    HttpBufferStream _stream;
};

#endif // MLB_LIVE_STUB_HTTPCLIENT_H
