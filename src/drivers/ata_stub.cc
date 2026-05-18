#include <drivers/ata.h>
#include <string.h>
#include <stdint.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/emscripten.h>
#endif

#ifdef __EMSCRIPTEN__
extern "C" void printf(char*);
#else
void printf(char*);
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
            if (!Module._ata_cache) Module._ata_cache = {};

            // Pre-populate the cache via a single getAll() — orders
            // of magnitude faster than the previous per-sector cursor
            // walk that did byte-by-byte ArrayBuffer copies in JS.
            var tx = Module._ata_db.transaction(['sectors'], 'readonly');
            var store = tx.objectStore('sectors');
            var allReq = store.getAll();
            allReq.onsuccess = function(ev) {
                var rows = ev.target.result || [];
                for (var k = 0; k < rows.length; k++) {
                    var row = rows[k];
                    if (!(row.data instanceof ArrayBuffer)) continue;
                    var sourceView = new Uint8Array(row.data);
                    var targetLength = Math.max(512, sourceView.length);
                    var cacheCopy = new Uint8Array(targetLength);
                    cacheCopy.set(sourceView.subarray(0, Math.min(sourceView.length, targetLength)));
                    Module._ata_cache['sector_' + row.sector] = cacheCopy;
                }
                Module._ata_cache_populated = true;
                if (Module._osakaFileSystemPtr && Module._refreshFileSystem) {
                    try { Module._refreshFileSystem(Module._osakaFileSystemPtr); }
                    catch (e) { console.warn('[ATA] Refresh fs failed:', e); }
                }
            };
            allReq.onerror = function(ev) {
                console.error('[ATA] Cache populate failed:', ev.target.error);
                Module._ata_cache_populated = true;
                if (Module._osakaFileSystemPtr && Module._refreshFileSystem) {
                    try { Module._refreshFileSystem(Module._osakaFileSystemPtr); }
                    catch (e) { /* swallow */ }
                }
            };
        };
        
        request.onupgradeneeded = function(event) {
            var db = event.target.result;
            if (!db.objectStoreNames.contains('sectors')) {
                db.createObjectStore('sectors', { keyPath: 'sector' });
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
    
    // First, try to read from cache (fast path)
    bool cacheHit = EM_ASM_INT({
        var sector = $0;
        var dataPtr = $1;
        var count = $2;
        var offset = $3;
        
        if (!Module._ata_db) {
            // Database not ready yet — fresh disk, zero the dest.
            HEAPU8.fill(0, dataPtr + offset, dataPtr + offset + count);
            return 1; // Handled (returned zeros)
        }
        
        // Try to read from cache first.
        var cacheKey = 'sector_' + sector;
        if (Module._ata_cache && Module._ata_cache[cacheKey]) {
            var cachedData = Module._ata_cache[cacheKey];

            // If the cached entry is all zeros but the sector might
            // actually exist on disk, kick off an async verify to
            // refresh the cache. The current read still returns the
            // zeros — subsequent reads will see the real data.
            var allZeros = true;
            for (var i = 0; i < Math.min(512, cachedData.length); i++) {
                if (cachedData[i] !== 0) { allZeros = false; break; }
            }
            if (allZeros && Module._ata_db && sector > 1024) {
                var verifyTx = Module._ata_db.transaction(['sectors'], 'readonly');
                var verifyStore = verifyTx.objectStore('sectors');
                var verifyRequest = verifyStore.get(sector);
                verifyRequest.onsuccess = function(event) {
                    var result = event.target.result;
                    if (result && result.data instanceof ArrayBuffer) {
                        var sourceView = new Uint8Array(result.data);
                        var targetLength = Math.max(512, sourceView.length);
                        var cacheCopy = new Uint8Array(targetLength);
                        cacheCopy.set(sourceView.subarray(0, Math.min(sourceView.length, targetLength)));
                        Module._ata_cache[cacheKey] = cacheCopy;
                    }
                };
            }

            // Bulk-copy via subarray.set — orders of magnitude faster
            // than a per-byte JS loop, which is the path that was
            // dominating boot time.
            var copyCount = Math.min(count, cachedData.length);
            HEAPU8.set(cachedData.subarray(0, copyCount), dataPtr + offset);
            for (var i = copyCount; i < count; i++) {
                HEAPU8[dataPtr + offset + i] = 0;
            }
            return 1; // Cache hit
        }
        return 0; // Cache miss - need to read from IndexedDB
    }, sector, (uintptr_t)data, count, offset);

    if (cacheHit) {
        return; // Data already copied from cache
    }
    
    // Cache miss — kick off an async IndexedDB read to populate
    // the cache. The current read returns zeros (see below); the
    // next read of the same sector will hit the cache.
    EM_ASM_({
        var sector = $0;
        if (!Module._ata_db) return;
        var cacheKey = 'sector_' + sector;
        var transaction = Module._ata_db.transaction(['sectors'], 'readonly');
        var store = transaction.objectStore('sectors');
        var request = store.get(sector);
        request.onsuccess = function(event) {
            var result = event.target.result;
            if (result && result.data instanceof ArrayBuffer) {
                var sourceView = new Uint8Array(result.data);
                var targetLength = Math.max(512, sourceView.length);
                var cacheCopy = new Uint8Array(targetLength);
                cacheCopy.set(sourceView.subarray(0, Math.min(sourceView.length, targetLength)));
                if (!Module._ata_cache) Module._ata_cache = {};
                Module._ata_cache[cacheKey] = cacheCopy;
            }
        };
    }, sector);
    
    // We've already kicked off an async IndexedDB load above. Return
    // zeros for this call — the next read of the same sector will
    // find the populated cache entry. Avoiding emscripten_sleep here
    // keeps us out of asyncify suspension (which conflicts with
    // emscripten_set_main_loop's stack model).
    EM_ASM_({ HEAPU8.fill(0, $0, $0 + $1); }, (uintptr_t)data + offset, count);
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
    
    // Update cache synchronously (so subsequent reads see the data
    // immediately) and persist to IndexedDB asynchronously.
    EM_ASM_({
        var sector = $0;
        var dataPtr = $1;
        var count = $2;
        var offset = $3;
        var cacheKey = 'sector_' + sector;
        if (!Module._ata_cache) Module._ata_cache = {};

        // Start from the existing cached sector (if any) so a
        // partial write doesn't clobber the rest of the 512-byte
        // sector. Then overlay the new bytes from wasm memory.
        var sectorData;
        if (Module._ata_cache[cacheKey]) {
            sectorData = new Uint8Array(Module._ata_cache[cacheKey]);
        } else {
            sectorData = new Uint8Array(512);
        }
        var clamp = Math.min(count, 512 - offset);
        if (clamp > 0) {
            sectorData.set(HEAPU8.subarray(dataPtr, dataPtr + clamp), offset);
        }
        Module._ata_cache[cacheKey] = sectorData;

        // Persist to IndexedDB (best-effort; errors stay in the
        // console but don't block).
        if (!Module._ata_db) return;
        try {
            var tx = Module._ata_db.transaction(['sectors'], 'readwrite');
            var store = tx.objectStore('sectors');
            var bufferCopy = new ArrayBuffer(sectorData.length);
            new Uint8Array(bufferCopy).set(sectorData);
            store.put({ sector: sector, data: bufferCopy });
        } catch (e) {
            console.error('[ATA] Write28 IndexedDB put failed', e);
        }
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
    // No-op: IndexedDB writes from Write28 are already enqueued on
    // their own transactions and will be persisted by the browser.
}

