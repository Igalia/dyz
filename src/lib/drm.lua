local ffi = require("ffi")

local drm = ffi.load("libdrm.so")

ffi.cdef[[

typedef unsigned int drm_magic_t;

int drmGetMagic(int fd, drm_magic_t* magic);
int drmAuthMagic(int fd, drm_magic_t magic);

typedef struct _drmModeModeInfo {
	uint32_t clock;
	uint16_t hdisplay, hsync_start, hsync_end, htotal, hskew;
	uint16_t vdisplay, vsync_start, vsync_end, vtotal, vscan;

	uint32_t vrefresh;

	uint32_t flags;
	uint32_t type;
	char name[32];
} drmModeModeInfo, *drmModeModeInfoPtr;

typedef struct _drmModeRes {

	int count_fbs;
	uint32_t *fbs;

	int count_crtcs;
	uint32_t *crtcs;

	int count_connectors;
	uint32_t *connectors;

	int count_encoders;
	uint32_t *encoders;

	uint32_t min_width, max_width;
	uint32_t min_height, max_height;
} drmModeRes, *drmModeResPtr;

typedef struct _drmModeEncoder {
	uint32_t encoder_id;
	uint32_t encoder_type;
	uint32_t crtc_id;
	uint32_t possible_crtcs;
	uint32_t possible_clones;
} drmModeEncoder, *drmModeEncoderPtr;

typedef enum {
	DRM_MODE_CONNECTED         = 1,
	DRM_MODE_DISCONNECTED      = 2,
	DRM_MODE_UNKNOWNCONNECTION = 3
} drmModeConnection;

typedef enum {
	DRM_MODE_SUBPIXEL_UNKNOWN        = 1,
	DRM_MODE_SUBPIXEL_HORIZONTAL_RGB = 2,
	DRM_MODE_SUBPIXEL_HORIZONTAL_BGR = 3,
	DRM_MODE_SUBPIXEL_VERTICAL_RGB   = 4,
	DRM_MODE_SUBPIXEL_VERTICAL_BGR   = 5,
	DRM_MODE_SUBPIXEL_NONE           = 6
} drmModeSubPixel;

typedef struct _drmModeConnector {
	uint32_t connector_id;
	uint32_t encoder_id; /**< Encoder currently connected to */
	uint32_t connector_type;
	uint32_t connector_type_id;
	drmModeConnection connection;
	uint32_t mmWidth, mmHeight; /**< HxW in millimeters */
	drmModeSubPixel subpixel;

	int count_modes;
	drmModeModeInfoPtr modes;

	int count_props;
	uint32_t *props; /**< List of property ids */
	uint64_t *prop_values; /**< List of property values */

	int count_encoders;
	uint32_t *encoders; /**< List of encoder ids */
} drmModeConnector, *drmModeConnectorPtr;

drmModeResPtr drmModeGetResources(int fd);

drmModeEncoderPtr drmModeGetEncoder(int fd, uint32_t encoder_id);

drmModeConnectorPtr drmModeGetConnector(int fd, uint32_t connectorId);

int drmModeAddFB(int fd, uint32_t width, uint32_t height, uint8_t depth,
    uint8_t bpp, uint32_t pitch, uint32_t bo_handle, uint32_t *buf_id);

int drmModePageFlip(int fd, uint32_t crtc_id, uint32_t fb_id,
    uint32_t flags, void *user_data);

enum {
    DRM_MODE_PAGE_FLIP_EVENT = 0x01,
};

enum {
    DRM_EVENT_CONTEXT_VERSION = 2,
};

typedef struct _drmEventContext {

	/* This struct is versioned so we can add more pointers if we
	 * add more events. */
	int version;

	void (*vblank_handler)(int fd,
			       unsigned int sequence, 
			       unsigned int tv_sec,
			       unsigned int tv_usec,
			       void *user_data);

	void (*page_flip_handler)(int fd,
				  unsigned int sequence,
				  unsigned int tv_sec,
				  unsigned int tv_usec,
				  void *user_data);

} drmEventContext, *drmEventContextPtr;

int drmHandleEvent(int fd, drmEventContextPtr evctx);

]]

return drm
