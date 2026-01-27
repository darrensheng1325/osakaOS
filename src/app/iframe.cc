#include <app/iframe.h>
#include <string.h>
#include <stdint.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace os;
using namespace os::common;
using namespace os::filesystem;
using namespace os::gui;

uint16_t strlen(char*);

IframeApp::IframeApp(char* urlStr) {
	this->appType = 4; // Iframe app type
	
	// Copy URL
	uint16_t len = strlen(urlStr);
	if (len > 255) len = 255;
	for (uint16_t i = 0; i < len; i++) {
		this->url[i] = urlStr[i];
	}
	this->url[len] = '\0';
	
	this->initialized = false;
	this->iframeId = 0;
}

IframeApp::~IframeApp() {
#ifdef __EMSCRIPTEN__
	// Remove iframe and cleanup when app is destroyed
	// Note: Close() should be called first, but this is a safety net
	if (this->iframeId != 0) {
		EM_ASM_({
			var iframeId = $0; // Numeric ID used as key in Module.iframeApps
			var iframeIdStr = 'osaka_iframe_' + iframeId;
			var captureCanvasId = 'osaka_capture_' + iframeId;
			
			console.log('[IframeApp C++] Destructor called, cleaning up iframe:', iframeId);
			
			// Stop intervals - use numeric ID as key
			if (Module.iframeApps && Module.iframeApps[iframeId]) {
				if (Module.iframeApps[iframeId].captureInterval) {
					clearInterval(Module.iframeApps[iframeId].captureInterval);
				}
				if (Module.iframeApps[iframeId].positionInterval) {
					clearInterval(Module.iframeApps[iframeId].positionInterval);
				}
				delete Module.iframeApps[iframeId];
			}
			
			// Remove DOM elements using string IDs
			var iframe = document.getElementById(iframeIdStr);
			if (iframe) {
				iframe.parentNode.removeChild(iframe);
			}
			var canvas = document.getElementById(captureCanvasId);
			if (canvas) {
				canvas.parentNode.removeChild(canvas);
			}
		}, this->iframeId);
	}
#endif
}

void IframeApp::ComputeAppState(GraphicsContext* gc, CompositeWidget* widget) {
#ifdef __EMSCRIPTEN__
	// Initialize iframe and setup capture
	if (!this->initialized) {
		this->iframeId = (uint32_t)this;
		
		// Debug logging via JavaScript console
		EM_ASM_({
			console.log('[IframeApp C++] Initializing iframe app, ID:', $0);
		}, this->iframeId);
		EM_ASM_({
			console.log('[IframeApp C++] Widget: x=' + $0 + ', y=' + $1 + ', w=' + $2 + ', h=' + $3);
		}, widget->x, widget->y, widget->w, widget->h);
		
		// Call JavaScript function to setup iframe
		EM_ASM_({
			console.log('[IframeApp C++] Calling setupIframeApp');
			if (Module.setupIframeApp) {
				console.log('[IframeApp C++] setupIframeApp exists, calling it');
				Module.setupIframeApp($0, $1, $2, $3, $4, $5);
			} else {
				console.error('[IframeApp C++] setupIframeApp not found!');
			}
		}, (uintptr_t)this->url, this->iframeId, widget->x, widget->y, widget->w, widget->h);
		
		this->initialized = true;
		EM_ASM_({
			console.log('[IframeApp C++] Initialization complete');
		});
	}
	
	// Capture iframe and render to window buffer
	// Note: widget->buf and widget->windowBuffer point to the same memory
	// The window content area starts at (x+1, y+10) with size (w-1, h-10)
	// But the buffer is relative to (0,0) of the widget, so we write at (1, 10) relative to widget
	// Also update widget coordinates for iframe positioning
	EM_ASM_({
		if (Module.captureIframeToBuffer) {
			// Update widget coordinates in the app object (for cross-origin iframe positioning)
			if (Module.iframeApps && Module.iframeApps[$0]) {
				Module.iframeApps[$0].widgetX = $1;
				Module.iframeApps[$0].widgetY = $2;
				Module.iframeApps[$0].widgetW = $3;
				Module.iframeApps[$0].widgetH = $4;
			}
			// Write to the content area of the window (skip header)
			Module.captureIframeToBuffer($0, 1, 10, $3 - 1, $4 - 10, $5);
		} else {
			console.error('[IframeApp C++] captureIframeToBuffer not found!');
		}
	}, this->iframeId, widget->x, widget->y, widget->w, widget->h, (uintptr_t)widget->buf);
#else
	// Non-web version: just draw a placeholder
	gc->FillRectangle(widget->x, widget->y, widget->w, widget->h, 0x07);
	gc->PutText("Iframe (web only)", widget->x + 5, widget->y + 10, 0x0f);
#endif
}

void IframeApp::DrawAppMenu(GraphicsContext* gc, CompositeWidget* widget) {
	// No menu for iframe
}

void IframeApp::SaveOutput(char* file, CompositeWidget* widget, FileSystem* filesystem) {
	// Not applicable for iframe
}

void IframeApp::ReadInput(char* file, CompositeWidget* widget, FileSystem* filesystem) {
	// Not applicable for iframe
}

void IframeApp::Close() {
	// Explicitly cleanup iframe when window is closed
#ifdef __EMSCRIPTEN__
	if (this->iframeId != 0) {
		EM_ASM_({
			var iframeId = $0; // Numeric ID used as key in Module.iframeApps
			var iframeIdStr = 'osaka_iframe_' + iframeId;
			var captureCanvasId = 'osaka_capture_' + iframeId;
			
			console.log('[IframeApp C++] Close() called, cleaning up iframe:', iframeId, 'string:', iframeIdStr);
			
			// Stop intervals - use numeric ID as key (matches how it's stored in setupIframeApp)
			if (Module.iframeApps && Module.iframeApps[iframeId]) {
				if (Module.iframeApps[iframeId].captureInterval) {
					clearInterval(Module.iframeApps[iframeId].captureInterval);
					console.log('[IframeApp C++] Cleared captureInterval');
				}
				if (Module.iframeApps[iframeId].positionInterval) {
					clearInterval(Module.iframeApps[iframeId].positionInterval);
					console.log('[IframeApp C++] Cleared positionInterval');
				}
				delete Module.iframeApps[iframeId];
				console.log('[IframeApp C++] Deleted iframeApps entry');
			} else {
				console.warn('[IframeApp C++] iframeApps entry not found for ID:', iframeId);
			}
			
			// Remove DOM elements using string IDs
			var iframe = document.getElementById(iframeIdStr);
			if (iframe) {
				iframe.parentNode.removeChild(iframe);
				console.log('[IframeApp C++] Removed iframe element');
			} else {
				console.warn('[IframeApp C++] Iframe element not found:', iframeIdStr);
			}
			var canvas = document.getElementById(captureCanvasId);
			if (canvas) {
				canvas.parentNode.removeChild(canvas);
				console.log('[IframeApp C++] Removed capture canvas element');
			} else {
				console.warn('[IframeApp C++] Capture canvas not found:', captureCanvasId);
			}
		}, this->iframeId);
	}
	this->initialized = false;
	this->iframeId = 0;
#endif
}

void IframeApp::OnKeyDown(char ch, CompositeWidget* widget) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		if (Module.forwardKeyToIframe) {
			Module.forwardKeyToIframe($0, $1, 'keydown');
		}
	}, this->iframeId, (uint8_t)ch);
#endif
}

void IframeApp::OnKeyUp(char ch, CompositeWidget* widget) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		if (Module.forwardKeyToIframe) {
			Module.forwardKeyToIframe($0, $1, 'keyup');
		}
	}, this->iframeId, (uint8_t)ch);
#endif
}

void IframeApp::OnMouseDown(common::int32_t x, common::int32_t y, common::uint8_t button, CompositeWidget* widget) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		if (Module.forwardMouseToIframe) {
			Module.forwardMouseToIframe($0, $1, $2, $3, $4, $5, 'mousedown');
		}
	}, this->iframeId, x, y, button, widget->x, widget->y);
#endif
}

void IframeApp::OnMouseUp(common::int32_t x, common::int32_t y, common::uint8_t button, CompositeWidget* widget) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		if (Module.forwardMouseToIframe) {
			Module.forwardMouseToIframe($0, $1, $2, $3, $4, $5, 'mouseup');
		}
	}, this->iframeId, x, y, button, widget->x, widget->y);
#endif
}

void IframeApp::OnMouseMove(common::int32_t oldx, common::int32_t oldy, 
			common::int32_t newx, common::int32_t newy,
			CompositeWidget* widget) {
#ifdef __EMSCRIPTEN__
	EM_ASM_({
		if (Module.forwardMouseToIframe) {
			Module.forwardMouseToIframe($0, $1, $2, 0, $3, $4, 'mousemove');
		}
	}, this->iframeId, newx, newy, widget->x, widget->y);
#endif
}

