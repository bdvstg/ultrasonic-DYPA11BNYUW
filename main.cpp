#include <chrono>
#include <iostream>
#include <thread>
#include <vector>
#include <windows.h>


/* Returns the last Win32 error, in string format. Returns an empty string if there is no error.
code from:
    https://stackoverflow.com/questions/1387064/how-to-get-the-error-message-from-the-error-code-returned-by-getlasterror
*/
std::string GetLastErrorAsString(DWORD errorMessageID)
{// this function will return localized error message
    if(errorMessageID == 0)
        return std::string(); //No error message has been recorded

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                 NULL, errorMessageID, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    std::string message(messageBuffer, size);

    //Free the buffer.
    LocalFree(messageBuffer);

    return message;
}

HANDLE g_hCommPort = nullptr;
bool gNeedExit = false;
bool gIsExited = false;

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    printf("event handler\n");
    switch (fdwCtrlType)
    {
    case CTRL_C_EVENT: // ctrl+c
    case CTRL_CLOSE_EVENT:
    case CTRL_BREAK_EVENT:
        gNeedExit = true;
        printf("close com port\n");
        if(g_hCommPort != nullptr)
            CloseHandle(g_hCommPort);
        g_hCommPort = nullptr;
        std::fflush(stdout);
        return TRUE; // event handled by this function
    default:
        return FALSE; // let other handler to handle
    }
}

int main(int argc, const char * argv[]) {
    BOOL success;
    DWORD err;
    setbuf(stdout, NULL); // print message without buffer

    if(argc != 2) {
        printf("usage: DYP_A11BNYUW.exe COMx\n");
        exit(1);
    }

    printf("setup system event\n");
    if(!SetConsoleCtrlHandler(CtrlHandler, TRUE)) {
        err = GetLastError();
        std::string errMsg = GetLastErrorAsString(err);
        printf("can not set ctrl handler, error message:%s\n", errMsg.c_str());
        exit(2);
    }

    printf("opening ComPort: %s\n", argv[1]);
    g_hCommPort = ::CreateFile((LPCSTR)argv[1],
        GENERIC_READ|GENERIC_WRITE, //access ( read and write)
        0, //(share) 0:cannot share the COM port
        0, //security  (None)
        OPEN_EXISTING, // creation : open_existing
        0, // no overlapped operation
        0 // no templates file for COM port...
        );
    if(g_hCommPort == INVALID_HANDLE_VALUE) {
        err = GetLastError();
        std::string errMsg = GetLastErrorAsString(err);
        printf("can not open ComPort: %s, error message:%s\n", argv[1], errMsg.c_str());
        exit(3);
    }

    // setting baudrate, bytesize, stopbits, parity
    DCB serialParams = { 0 };
    serialParams.DCBlength = sizeof(serialParams);

    printf("get com port params\n");
    success = GetCommState(g_hCommPort, &serialParams);
    if(!success) {
        err = GetLastError();
        std::string errMsg = GetLastErrorAsString(err);
        printf("can not GetCOMMState, error message: %s\n", errMsg.c_str());
        exit(4);
    }
    serialParams.BaudRate = 9600;
    serialParams.ByteSize = 8;
    serialParams.StopBits = ONESTOPBIT;
    serialParams.Parity = NOPARITY;
    printf("set com port params\n");
    success = SetCommState(g_hCommPort, &serialParams);
    if(!success) {
        err = GetLastError();
        std::string errMsg = GetLastErrorAsString(err);
        printf("can not SetCOMMState, error message: %s\n", errMsg.c_str());
        exit(5);
    }

    // Set timeouts
    printf("setup com port timeout params\n");
    COMMTIMEOUTS timeout = { 0 };
    timeout.ReadIntervalTimeout = 50;
    timeout.ReadTotalTimeoutConstant = 50;
    timeout.ReadTotalTimeoutMultiplier = 50;
    timeout.WriteTotalTimeoutConstant = 50;
    timeout.WriteTotalTimeoutMultiplier = 10;
    success = SetCommTimeouts(g_hCommPort, &timeout);
    if(!success) {
        err = GetLastError();
        std::string errMsg = GetLastErrorAsString(err);
        printf("can not SetCommTimeouts, error message: %s\n", errMsg.c_str());
        exit(6);
    }

    DWORD dwBytesRead;
    unsigned char buff[64];
    
    gIsExited = false;
    printf("start read\n");
    while(!gNeedExit) {
        if(g_hCommPort == nullptr)
            break;
        success = ::ReadFile(g_hCommPort,
            buff ,sizeof(buff), &dwBytesRead, nullptr);
        if(success) {
            for(int i = 0; i < (int)dwBytesRead ; i++) {
                printf("0x%02X ", buff[i]);
            }
            // 0xFF, HighByte, LowByte, CheckSum
            int distance = (int)buff[1] * 256 + buff[2];
            int sum = ((int)buff[0] + (int)buff[1] + (int)buff[2]) & 0x00FF;
            bool match = (sum == buff[3]);
            printf(", distance=%d, check sum %s\n",
                distance, match?"OK":"FAILED");

        } else {
            err = GetLastError();
            std::string errMsg = GetLastErrorAsString(err);
            printf("read error, error message: %s\n", errMsg.c_str());
            exit(7);
        }
    }
    gIsExited = true;
    return 0;
}
