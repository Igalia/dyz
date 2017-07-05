local ffi = require "ffi"

local wk = ffi.load("libWPEWebKit.so")

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
    WKViewRef WKViewCreateWithViewBackend(struct wpe_view_backend*, WKPageConfigurationRef);
    WKPageRef WKViewGetPage(WKViewRef);
    void WKPageLoadURL(WKPageRef page, WKURLRef url);

    WKURLRef WKURLCreateWithUTF8CString(const char* string);
    WKStringRef WKURLCopyString(WKURLRef);
    WKStringRef WKURLCopyHostName(WKURLRef);
    WKStringRef WKURLCopyScheme(WKURLRef);
    WKStringRef WKURLCopyPath(WKURLRef);
    WKStringRef WKURLCopyLastPathComponent(WKURLRef);
    bool WKURLIsEqual(WKURLRef, WKURLRef other);

    void WKRelease(void*);
]]

local function gc_release(object)
    wk.WKRelease(ffi.gc(object, nil))
end

local module = {}

local function TypeAlias(alias)
    return function (ctype)
        return function (meta)
            if not meta.__gc then
                meta.__gc = gc_release
            end
            local ct = ffi.metatype(ctype, meta)
            module[alias] = ct
            return ct
        end
    end
end

local function Type(name)
    return TypeAlias(name)("struct OpaqueWK" .. name)
end

local CharArray = ffi.typeof("char[?]")

-- The "wpe" module does *not* expose the WKStringRef type, but we want
-- to use it in a more convenient way so we define a metatype for it.
local String = ffi.metatype("struct OpaqueWKString", {
    __gc = gc_release;
    __new = function (ct, utf8)
        return wk.WKStringCreateWithUTF8CString(utf8)
    end;
    __tostring = function (self)
        local n = wk.WKStringGetMaximumUTF8CStringSize(self)
        local s = CharArray(n + 1)
        local r = wk.WKStringGetUTF8CString(self, s, n + 1)
        return s
    end;
})

-- URL gets exposed, *and* we also keep a local reference for convenience.
local URL = Type "URL" {
    __new = function (ct, url)
        return wk.WKURLCreateWithUTF8CString(url)
    end;
    __tostring = function (self)
        return tostring(wk.WKURLCopyString(self))
    end;
    __eq = function (self, other)
        return ffi.istype(URL, other) and wk.WKURLIsEqual(self, other)
    end;
    __index = {
        get_hostname = function (self)
            return tostring(wk.WKURLCopyHostName(self))
        end;
        get_scheme = function (self)
            return tostring(wk.WKURLCopyScheme(self))
        end;
        get_path = function (self)
            return tostring(wk.WKURLCopyPath(self))
        end;
        get_last_path_component = function (self)
            return tostring(wk.WKURLCopyLastPathComponent(self))
        end;
    };
}

Type "Context" {
    __new = function (ct)
        return wk.WKContextCreate()
    end;
}

Type "PageGroup" {
    __new = function (ct, identifier)
        return wk.WKPageGroupCreateWithIdentifier(String(identifier))
    end;
}

Type "PageConfiguration" {
    __new = function (ct)
        return wk.WKPageConfigurationCreate()
    end;
    __index = {
        set_context = wk.WKPageConfigurationSetContext;
        set_page_group = wk.WKPageConfigurationSetPageGroup;
    };
}

Type "View" {
    __new = function (ct, configuration)
        return wk.WKViewCreate(configuration)
    end;
    __new = function (ct, view_backend, configuration)
        return wk.WKViewCreateWithViewBackend(view_backend, configuration)
    end;
    __index = {
        get_page = wk.WKViewGetPage;
    };
}

Type "Page" {
    __new = function (ct)
        error("wpe.Page cannot be manually instantiated")
    end;
    __index = {
        load_url = function (self, url)
            if type(url) == "string" then
                url = URL(url)
            end
            return wk.WKPageLoadURL(self, url)
        end;
    };
}

return module
