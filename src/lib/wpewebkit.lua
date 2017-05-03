local ffi = require("ffi")

local lib = ffi.load("libWPEWebKit.so")

ffi.cdef[[
    typedef const struct OpaqueWKString* WKStringRef;
    typedef const struct OpaqueWKContext* WKContextRef;
    typedef const struct OpaqueWKPageGroup* WKPageGroupRef;
    typedef const struct OpaqueWKPageConfiguration* WKPageConfigurationRef;
    typedef const struct OpaqueWKView* WKViewRef;
    typedef const struct OpaqueWKPage* WKPageRef;
    typedef const struct OpaqueWKURL* WKURLRef;

    WKStringRef WKStringCreateWithUTF8CString(const char* string);
    size_t WKStringGetMaximumUTF8CStringSize(WKStringRef);
    size_t WKStringGetUTF8CString(WKStringRef, char* buffer, size_t bufferSize);

    WKContextRef WKContextCreate();

    WKPageGroupRef WKPageGroupCreateWithIdentifier(WKStringRef identifier);
    WKPageConfigurationRef WKPageConfigurationCreate();

    void WKPageConfigurationSetContext(WKPageConfigurationRef configuration, WKContextRef context);
    void WKPageConfigurationSetPageGroup(WKPageConfigurationRef configuration, WKPageGroupRef pageGroup);

    WKViewRef WKViewCreate(WKPageConfigurationRef);
    WKPageRef WKViewGetPage(WKViewRef);
    void WKPageLoadURL(WKPageRef page, WKURLRef url);

    WKURLRef WKURLCreateWithUTF8CString(const char* string);
    WKStringRef WKURLCopyString(WKURLRef);

    void WKRelease(void*);
]]

return lib
