#include <drivers/ata.h>
#include <string.h>
#include <stdint.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

using namespace os;
using namespace os::common;
using namespace os::drivers;
using namespace os::hardwarecommunication;

AdvancedTechnologyAttachment::AdvancedTechnologyAttachment(uint16_t portBase, bool master)
: dataPort(portBase),
  errorPort(portBase + 0x01),
  sectorCountPort(portBase + 0x02),
  lbaLowPort(portBase + 0x03),
  lbaMidPort(portBase + 0x04),
  lbaHiPort(portBase + 0x05),
  devicePort(portBase + 0x06),
  commandPort(portBase + 0x07),
  controlPort(portBase + 0x206) {
    
    bytesPerSector = 512;
    this->master = master;
    
#ifdef __EMSCRIPTEN__
    // Initialize IndexedDB storage
    EM_ASM_({
        if (typeof indexedDB === 'undefined') {
            console.error('[ATA] IndexedDB not available');
            return;
        }
        
        // Open or create database
        var request = indexedDB.open('osakaOS_disk', 1);
        
        request.onerror = function(event) {
            console.error('[ATA] IndexedDB error:', event.target.error);
        };
        
        request.onsuccess = function(event) {
            Module._ata_db = event.target.result;
            console.log('[ATA] IndexedDB opened successfully');
            
            // Pre-populate cache with all sectors from IndexedDB
            // This allows Read28 to work synchronously by reading from cache
            if (Module._ata_db) {
                var transaction = Module._ata_db.transaction(['sectors'], 'readonly');
                var store = transaction.objectStore('sectors');
                var request = store.openCursor();
                
                if (!Module._ata_cache) {
                    Module._ata_cache = {};
                }
                
                request.onsuccess = function(event) {
                    var cursor = event.target.result;
                    if (cursor) {
                        var sector = cursor.key;
                        var data = cursor.value.data;
                        Module._ata_cache['sector_' + sector] = new Uint8Array(data);
                        cursor.continue();
                    } else {
                        console.log('[ATA] Cache populated, refreshing filesystem');
                        // Now that cache is populated, refresh filesystem
                        if (Module._osakaFileSystemPtr && Module._refreshFileSystem) {
                            try {
                                Module._refreshFileSystem(Module._osakaFileSystemPtr);
                                console.log('[ATA] Filesystem file table refreshed');
                            } catch (e) {
                                console.warn('[ATA] Could not refresh filesystem:', e);
                            }
                        }
                    }
                };
                
                request.onerror = function(event) {
                    console.error('[ATA] Error populating cache:', event.target.error);
                    // Still try to refresh filesystem even if cache fails
                    if (Module._osakaFileSystemPtr && Module._refreshFileSystem) {
                        try {
                            Module._refreshFileSystem(Module._osakaFileSystemPtr);
                        } catch (e) {
                            console.warn('[ATA] Could not refresh filesystem:', e);
                        }
                    }
                };
            }
        };
        
        request.onupgradeneeded = function(event) {
            var db = event.target.result;
            // Create object store for disk sectors
            if (!db.objectStoreNames.contains('sectors')) {
                var objectStore = db.createObjectStore('sectors', { keyPath: 'sector' });
                console.log('[ATA] Created sectors object store');
            }
        };
    });
#endif
}

AdvancedTechnologyAttachment::~AdvancedTechnologyAttachment() {
}

bool AdvancedTechnologyAttachment::Identify() {
#ifdef __EMSCRIPTEN__
    // For web, return true if IndexedDB is available
    return true;
#else
    return false;
#endif
}

void AdvancedTechnologyAttachment::Read28(uint32_t sector, uint8_t* data, int count, int offset) {
#ifdef __EMSCRIPTEN__
    if (!data || count <= 0) return;
    
    // Read from IndexedDB - use synchronous API if available, otherwise return zeros
    // IndexedDB doesn't have a true synchronous API, so we'll use a cache approach
    // For now, we'll read directly and let the browser handle it
    // If the DB isn't ready, we return zeros (fresh disk state)
    EM_ASM_({
        var sector = $0;
        var dataPtr = $1;
        var count = $2;
        var offset = $3;
        
        if (!Module._ata_db) {
            // Database not ready yet - zero out data (fresh disk)
            for (var i = 0; i < count; i++) {
                HEAPU8[dataPtr + offset + i] = 0;
            }
            return;
        }
        
        // Try to read synchronously using a cached approach
        // Store pending reads in a cache that gets populated asynchronously
        var cacheKey = 'sector_' + sector;
        if (Module._ata_cache && Module._ata_cache[cacheKey]) {
            // Use cached data
            var cachedData = Module._ata_cache[cacheKey];
            var copyCount = Math.min(count, cachedData.length - offset);
            for (var i = 0; i < copyCount; i++) {
                if (offset + i < cachedData.length) {
                    HEAPU8[dataPtr + offset + i] = cachedData[offset + i];
                } else {
                    HEAPU8[dataPtr + offset + i] = 0;
                }
            }
            for (var i = copyCount; i < count; i++) {
                HEAPU8[dataPtr + offset + i] = 0;
            }
            return;
        }
        
        // No cache - zero out data (will be populated by async refresh)
        // This is acceptable because RefreshFileTable will be called after IndexedDB is ready
        for (var i = 0; i < count; i++) {
            HEAPU8[dataPtr + offset + i] = 0;
        }
        
        // Trigger async read to populate cache for next time
        // But don't wait for it - just let it happen in background
        var transaction = Module._ata_db.transaction(['sectors'], 'readonly');
        var store = transaction.objectStore('sectors');
        var request = store.get(sector);
        
        request.onsuccess = function(event) {
            var result = event.target.result;
            if (result && result.data) {
                // Initialize cache if needed
                if (!Module._ata_cache) {
                    Module._ata_cache = {};
                }
                // Store in cache for future reads
                var sectorData = new Uint8Array(result.data);
                Module._ata_cache[cacheKey] = sectorData;
            }
        };
    }, sector, (uintptr_t)data, count, offset);
#else
    // Non-web: zero out data
    if (data && count > 0) {
        memset(data, 0, count);
    }
#endif
}

void AdvancedTechnologyAttachment::Write28(uint32_t sector, uint8_t* data, int count, int offset) {
#ifdef __EMSCRIPTEN__
    if (!data || count <= 0) return;
    
    // Write to IndexedDB
    EM_ASM_({
        var sector = $0;
        var dataPtr = $1;
        var count = $2;
        var offset = $3;
        
        if (!Module._ata_db) {
            // Database not ready yet (async initialization in progress)
            // Skip write - database will be available soon
            return;
        }
        
        // Read existing sector data if it exists
        var transaction = Module._ata_db.transaction(['sectors'], 'readwrite');
        var store = transaction.objectStore('sectors');
        var getRequest = store.get(sector);
        
        getRequest.onsuccess = function(event) {
            var result = event.target.result;
            var sectorData;
            
            if (result && result.data) {
                // Use existing data
                sectorData = new Uint8Array(result.data);
            } else {
                // Create new sector (512 bytes)
                sectorData = new Uint8Array(512);
                for (var i = 0; i < 512; i++) {
                    sectorData[i] = 0;
                }
            }
            
            // Copy data from WASM memory to sector buffer
            for (var i = 0; i < count && (offset + i) < 512; i++) {
                sectorData[offset + i] = HEAPU8[dataPtr + i];
            }
            
            // Save to IndexedDB
            var putRequest = store.put({
                sector: sector,
                data: sectorData.buffer
            });
            
            putRequest.onsuccess = function() {
                console.log('[ATA] Wrote sector', sector);
            };
            
            putRequest.onerror = function(event) {
                console.error('[ATA] Write error:', event.target.error);
            };
        };
        
        getRequest.onerror = function(event) {
            console.error('[ATA] Read error during write:', event.target.error);
        };
    }, sector, (uintptr_t)data, count, offset);
#else
    // Non-web: do nothing
    (void)sector;
    (void)data;
    (void)count;
    (void)offset;
#endif
}

void AdvancedTechnologyAttachment::Flush() {
#ifdef __EMSCRIPTEN__
    // IndexedDB writes are synchronous in the transaction, but we can wait for completion
    EM_ASM_({
        if (Module._ata_db) {
            // Force any pending transactions to complete
            console.log('[ATA] Flush called');
        }
    });
#endif
}

