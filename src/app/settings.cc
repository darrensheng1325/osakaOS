#include <app/settings.h>
#include <new>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
extern "C" void printf(char*);
#else
void printf(char*);
#endif

// Forward declarations
bool strcmp(char* str1, char* str2);
uint16_t strlen(char* str);

using namespace os;
using namespace os::gui;
using namespace os::common;
using namespace os::filesystem;

Settings::Settings() {
	this->appType = 5; // Settings app type
	
	this->init = false;
	this->selectedCategory = 0;
	this->selectedItem = 0;
	
	// Default settings
	this->desktopBgColor = 0x11; // Default dark gray
	this->windowTopBarColor = 0x19; // Default window top bar color
	this->iframeDisplayMode = 1; // Default to direct overlay
	for (int i = 0; i < 256; i++) {
		this->iframeProxy[i] = '\0';
	}
	
	// Initialize button positions
	this->saveButtonX = 0;
	this->saveButtonY = 0;
	
	// Load saved settings
	this->LoadSettings();
}

Settings::~Settings() {
}

void Settings::ComputeAppState(GraphicsContext* gc, CompositeWidget* widget) {
	if (!init) {
		widget->Menu = false;
		init = true;
	}
	
	// Draw the settings UI (tabs, content, save button)
	this->DrawSettingsUI(gc, widget);
	
	App::ComputeAppState(gc, widget);
}

void Settings::DrawSettingsUI(GraphicsContext* gc, CompositeWidget* widget) {
	uint16_t offsetx = 1 * !widget->Fullscreen;
	uint8_t offsety = 10 * !widget->Fullscreen;
	
	// Absolute coordinates for drawing (relative to screen)
	int32_t startX = widget->x + offsetx + 1;
	int32_t startY = widget->y + offsety + 1;
	
	// Draw category tabs
	const char* categories[] = {"Desktop", "Windows", "Iframes"};
	for (int i = 0; i < 3; i++) {
		int32_t tabX = startX + i * 60;
		int32_t tabY = startY;
		uint8_t bgColor = (selectedCategory == i) ? 0x3f : 0x07;
		uint8_t textColor = (selectedCategory == i) ? 0x00 : 0x3f;
		
		gc->FillRectangle(tabX, tabY, 58, 12, bgColor);
		gc->DrawRectangle(tabX, tabY, 58, 12, 0x3f);
		gc->PutText((char*)categories[i], tabX + 2, tabY + 2, textColor);
	}
	
	// Draw settings for selected category
	int32_t contentY = startY + 15;
	
	switch (selectedCategory) {
		case 0: // Desktop
			{
				// Desktop background color
				gc->PutText("Background Color:", startX, contentY, 0x3f);
				contentY += 12;
				
				// Color picker (show current color and allow selection)
				gc->PutText("Current:", startX, contentY, 0x3f);
				gc->FillRectangle(startX + 50, contentY, 20, 12, desktopBgColor);
				gc->DrawRectangle(startX + 50, contentY, 20, 12, 0x3f);
				
				// Show color value
				char colorStr[10];
				colorStr[0] = '0';
				colorStr[1] = 'x';
				uint8_t high = (desktopBgColor >> 4) & 0x0f;
				uint8_t low = desktopBgColor & 0x0f;
				colorStr[2] = (high < 10) ? ('0' + high) : ('a' + high - 10);
				colorStr[3] = (low < 10) ? ('0' + low) : ('a' + low - 10);
				colorStr[4] = '\0';
				gc->PutText(colorStr, startX + 75, contentY, 0x3f);
				
				contentY += 15;
				gc->PutText("Use +/- to change", startX, contentY, 0x07);
			}
			break;
			
		case 1: // Windows
			{
				// Window top bar color
				gc->PutText("Top Bar Color:", startX, contentY, 0x3f);
				contentY += 12;
				
				// Color picker
				gc->PutText("Current:", startX, contentY, 0x3f);
				gc->FillRectangle(startX + 50, contentY, 20, 12, windowTopBarColor);
				gc->DrawRectangle(startX + 50, contentY, 20, 12, 0x3f);
				
				// Show color value
				char colorStr[10];
				colorStr[0] = '0';
				colorStr[1] = 'x';
				uint8_t high = (windowTopBarColor >> 4) & 0x0f;
				uint8_t low = windowTopBarColor & 0x0f;
				colorStr[2] = (high < 10) ? ('0' + high) : ('a' + high - 10);
				colorStr[3] = (low < 10) ? ('0' + low) : ('a' + low - 10);
				colorStr[4] = '\0';
				gc->PutText(colorStr, startX + 75, contentY, 0x3f);
				
				contentY += 15;
				gc->PutText("Use +/- to change", startX, contentY, 0x07);
			}
			break;
			
		case 2: // Iframes
			{
				// Iframe display mode
				gc->PutText("Display Mode:", startX, contentY, 0x3f);
				contentY += 12;
				
				const char* mode0 = "Canvas Capture";
				const char* mode1 = "Direct Overlay";
				gc->PutText((char*)(iframeDisplayMode == 0 ? mode0 : mode1), startX + 10, contentY, 0x3f);
				contentY += 15;
				gc->PutText("Press Space to toggle", startX, contentY, 0x07);
				
				contentY += 15;
				
				// Iframe proxy
				gc->PutText("Proxy URL:", startX, contentY, 0x3f);
				contentY += 12;
				
				if (strlen(iframeProxy) > 0) {
					// Show first 30 chars of proxy
					char displayProxy[31];
					int len = strlen(iframeProxy);
					if (len > 30) len = 30;
					for (int i = 0; i < len; i++) {
						displayProxy[i] = iframeProxy[i];
					}
					displayProxy[len] = '\0';
					gc->PutText(displayProxy, startX + 10, contentY, 0x3f);
				} else {
					gc->PutText("(none)", startX + 10, contentY, 0x07);
				}
				contentY += 15;
				gc->PutText("Type to edit, Enter to save", startX, contentY, 0x07);
			}
			break;
	}
	
	// Save button (store position for click detection)
	contentY += 10;
	int32_t saveX = startX;
	int32_t saveY = contentY;
	gc->FillRectangle(saveX, saveY, 50, 12, 0x07);
	gc->DrawRectangle(saveX, saveY, 50, 12, 0x3f);
	gc->PutText("Save", saveX + 2, saveY + 2, 0x3f);
	
	// Store save button position relative to content area (for click detection)
	// With coordinate system: relX = x - widget->x - 1 (where x is already absolute - offsetx from Window)
	// startX = widget->x + offsetx + 1, so in absolute: saveX = widget->x + offsetx + 1
	// Window passes: (widget->x + offsetx + 1) - offsetx = widget->x + 1
	// So relX = (widget->x + 1) - widget->x - 1 = 0
	// But tabs are also at startX = widget->x + offsetx + 1, so they're also at relX = 0
	// So saveButtonX should be 0 (same as tabs start at 0)
	this->saveButtonX = 0; // Relative to content area (same as tabStartX)
	// Calculate relative Y: contentY starts at startY + 15, then adds category content, then +10
	// startY = widget->y + offsety + 1, so in absolute: contentY = widget->y + offsety + 1 + 15 + categoryHeight + 10
	// Window passes: (widget->y + offsety + 1 + 15 + categoryHeight + 10) - offsety = widget->y + 1 + 15 + categoryHeight + 10
	// So relY = (widget->y + 1 + 15 + categoryHeight + 10) - widget->y - 1 = 15 + categoryHeight + 10
	int32_t categoryHeight = 0;
	switch (selectedCategory) {
		case 0: categoryHeight = 27; break; // 12 + 15
		case 1: categoryHeight = 27; break; // 12 + 15
		case 2: categoryHeight = 42; break; // 12 + 15 + 15
	}
	this->saveButtonY = 15 + categoryHeight + 10; // Relative to content area
}

void Settings::DrawAppMenu(GraphicsContext* gc, CompositeWidget* widget) {
	// No menu for settings
}

void Settings::OnMouseDown(int32_t x, int32_t y, uint8_t button, CompositeWidget* widget) {
	if (button != 1) return; // Only handle left mouse button
	
	// Calculate relative position in content area
	// Window::OnMouseDown passes (x-offsetx, y-offsety) where x,y are absolute screen coordinates
	// Content area starts at widget->x + offsetx + 1, widget->y + offsety + 1
	// So relative coordinates are: (x - offsetx) - (widget->x + offsetx + 1) = x - widget->x - 2*offsetx - 1
	// But wait, let's check Journal's approach...
	uint16_t offsetx = 1 * !widget->Fullscreen;
	uint8_t offsety = 10 * !widget->Fullscreen;
	
	// Journal calculates: relX = x - widget->x - offsetx
	// Where x is already (absolute - offsetx) from Window
	// So: relX = (absolute - offsetx) - widget->x - offsetx = absolute - widget->x - 2*offsetx
	// But content starts at widget->x + offsetx + 1, so we need to subtract that:
	// relX = (absolute - offsetx) - (widget->x + offsetx + 1) = absolute - widget->x - 2*offsetx - 1
	// But Journal uses: relX = x - widget->x - offsetx, which would be wrong...
	// Actually, let me recalculate: if Window passes x-offsetx, and content starts at widget->x+offsetx+1,
	// then relX should be: (x-offsetx) - (widget->x+offsetx+1) = x - widget->x - 2*offsetx - 1
	// But Journal uses: x - widget->x - offsetx, which suggests Window might not be subtracting offsetx?
	
	// Window passes (x-offsetx, y-offsety), so x is already adjusted for border
	// Content area starts at widget->x + offsetx + 1
	// Absolute click at content position: widget->x + offsetx + 1 + relContentX
	// Window passes: (widget->x + offsetx + 1 + relContentX) - offsetx = widget->x + 1 + relContentX
	// So: relContentX = (widget->x + 1 + relContentX) - (widget->x + offsetx + 1) = relContentX - offsetx
	// That's wrong! Let me recalculate:
	// If Window passes x where x = absolute - offsetx, and content starts at widget->x + offsetx + 1,
	// then: relX = x - (widget->x + offsetx + 1) = (absolute - offsetx) - widget->x - offsetx - 1
	// But we can also write: relX = x - widget->x - offsetx - 1
	// Actually, since x is already (absolute - offsetx), we need: relX = x - widget->x - 1
	// Because content starts at widget->x + offsetx + 1, and x is absolute - offsetx,
	// so: relX = (absolute - offsetx) - (widget->x + offsetx + 1) = absolute - widget->x - 2*offsetx - 1
	// But that's still wrong. Let me think differently:
	// If absolute click is at widget->x + offsetx + 1 (first pixel of content):
	// Window passes: (widget->x + offsetx + 1) - offsetx = widget->x + 1
	// We want relX = 0, so: 0 = (widget->x + 1) - widget->x - offsetx - 1 = 1 - offsetx - 1 = -offsetx
	// That's -1, not 0! So we need: relX = x - widget->x - 1 (without subtracting offsetx again)
	int32_t relX = x - widget->x - 1;
	int32_t relY = y - widget->y - 1;
	
	// Check category tabs (relative to content area, starting at 0,0)
	int32_t tabStartX = 0;
	int32_t tabStartY = 0;
	
	for (int i = 0; i < 3; i++) {
		int32_t tabX = tabStartX + i * 60;
		int32_t tabY = tabStartY;
		// Check if click is within tab bounds (58px wide, 12px tall)
		// Tab spans from tabX to tabX+57 (inclusive), so check relX < tabX + 58
		if (relX >= tabX && relX < tabX + 58 && relY >= tabY && relY < tabY + 12) {
			selectedCategory = i;
			selectedItem = 0;
			return;
		}
	}
	
	// Check save button (using stored position from DrawSettingsUI - already relative to content area)
	if (relX >= saveButtonX && relX < saveButtonX + 50 && relY >= saveButtonY && relY < saveButtonY + 12) {
		this->SaveSettings();
		this->ApplySettings();
		printf("Settings saved and applied\n");
		return;
	}
}

void Settings::OnKeyDown(char ch, CompositeWidget* widget) {
	switch (selectedCategory) {
		case 0: // Desktop
			if (ch == '+' || ch == '=') {
				if (desktopBgColor < 0x3f) desktopBgColor++;
			} else if (ch == '-' || ch == '_') {
				if (desktopBgColor > 0) desktopBgColor--;
			}
			break;
			
		case 1: // Windows
			if (ch == '+' || ch == '=') {
				if (windowTopBarColor < 0x3f) windowTopBarColor++;
			} else if (ch == '-' || ch == '_') {
				if (windowTopBarColor > 0) windowTopBarColor--;
			}
			break;
			
		case 2: // Iframes
			if (ch == ' ') {
				iframeDisplayMode = (iframeDisplayMode == 0) ? 1 : 0;
			} else if (ch >= 32 && ch < 127) {
				// Allow typing proxy URL (simple implementation)
				int len = strlen(iframeProxy);
				if (len < 255) {
					iframeProxy[len] = ch;
					iframeProxy[len + 1] = '\0';
				}
			} else if (ch == 8 || ch == 127) { // Backspace
				int len = strlen(iframeProxy);
				if (len > 0) {
					iframeProxy[len - 1] = '\0';
				}
			} else if (ch == 13 || ch == 10) { // Enter
				this->SaveSettings();
				this->ApplySettings();
				printf("Settings saved\n");
			}
			break;
	}
}

void Settings::LoadSettings() {
#ifdef __EMSCRIPTEN__
	// Load settings from IndexedDB
	EM_ASM_({
		if (Module._ata_db) {
			var transaction = Module._ata_db.transaction(['sectors'], 'readonly');
			var store = transaction.objectStore('sectors');
			// Settings stored in a special sector
			var request = store.get(999999); // Use a special sector number for settings
			
			request.onsuccess = function(event) {
				var result = event.target.result;
				if (result && result.data) {
					var data = new Uint8Array(result.data);
					// Parse settings from data
					// Format: [desktopBgColor, windowTopBarColor, iframeDisplayMode, proxyLen, ...proxy bytes...]
					if (data.length >= 3) {
						Module._settings_desktopBg = data[0];
						Module._settings_windowTopBar = data[1];
						Module._settings_iframeMode = data[2];
						
						if (data.length > 3) {
							var proxyLen = data[3];
							if (proxyLen > 0 && proxyLen < 256 && data.length >= 4 + proxyLen) {
								var proxyStr = '';
								for (var i = 0; i < proxyLen; i++) {
									proxyStr += String.fromCharCode(data[4 + i]);
								}
								Module._settings_proxy = proxyStr;
							} else {
								Module._settings_proxy = '';
							}
						} else {
							Module._settings_proxy = '';
						}
					}
				}
			};
		}
	});
	
	// Read settings from Module
	int desktopBg = EM_ASM_INT({
		return Module._settings_desktopBg || $0;
	}, desktopBgColor);
	int windowTopBar = EM_ASM_INT({
		return Module._settings_windowTopBar || $0;
	}, windowTopBarColor);
	int iframeMode = EM_ASM_INT({
		return Module._settings_iframeMode || $0;
	}, iframeDisplayMode);
	
	desktopBgColor = (uint8_t)desktopBg;
	windowTopBarColor = (uint8_t)windowTopBar;
	iframeDisplayMode = (uint8_t)iframeMode;
	
	// Load proxy string
	EM_ASM_({
		var proxy = Module._settings_proxy || '';
		var proxyPtr = $0;
		for (var i = 0; i < proxy.length && i < 255; i++) {
			HEAPU8[proxyPtr + i] = proxy.charCodeAt(i);
		}
		HEAPU8[proxyPtr + proxy.length] = 0;
	}, (uintptr_t)iframeProxy);
#endif
}

void Settings::SaveSettings() {
#ifdef __EMSCRIPTEN__
	// Save settings to IndexedDB
	EM_ASM_({
		if (Module._ata_db) {
			var desktopBg = $0;
			var windowTopBar = $1;
			var iframeMode = $2;
			var proxyPtr = $3;
			
			// Build settings data array
			var proxyStr = '';
			var proxyLen = 0;
			for (var i = 0; i < 255; i++) {
				var ch = HEAPU8[proxyPtr + i];
				if (ch == 0) break;
				proxyStr += String.fromCharCode(ch);
				proxyLen++;
			}
			
			var data = new Uint8Array(4 + proxyLen);
			data[0] = desktopBg;
			data[1] = windowTopBar;
			data[2] = iframeMode;
			data[3] = proxyLen;
			for (var i = 0; i < proxyLen; i++) {
				data[4 + i] = proxyStr.charCodeAt(i);
			}
			
			var transaction = Module._ata_db.transaction(['sectors'], 'readwrite');
			var store = transaction.objectStore('sectors');
			store.put({
				sector: 999999,
				data: data.buffer
			});
		}
	}, desktopBgColor, windowTopBarColor, iframeDisplayMode, (uintptr_t)iframeProxy);
#endif
}

void Settings::ApplySettings() {
#ifdef __EMSCRIPTEN__
	// Apply settings globally
	EM_ASM_({
		// Store settings in Module for other components to access
		Module._settings_desktopBg = $0;
		Module._settings_windowTopBar = $1;
		Module._settings_iframeMode = $2;
		var proxyPtr = $3;
		
		var proxyStr = '';
		for (var i = 0; i < 255; i++) {
			var ch = HEAPU8[proxyPtr + i];
			if (ch == 0) break;
			proxyStr += String.fromCharCode(ch);
		}
		Module._settings_proxy = proxyStr;
		
		console.log('[Settings] Applied:', {
			desktopBg: Module._settings_desktopBg,
			windowTopBar: Module._settings_windowTopBar,
			iframeMode: Module._settings_iframeMode,
			proxy: Module._settings_proxy
		});
	}, desktopBgColor, windowTopBarColor, iframeDisplayMode, (uintptr_t)iframeProxy);
#endif
}

void Settings::SaveOutput(char* fileName, CompositeWidget* widget, FileSystem* filesystem) {
	// Not applicable
}

void Settings::ReadInput(char* fileName, CompositeWidget* widget, FileSystem* filesystem) {
	// Not applicable
}

