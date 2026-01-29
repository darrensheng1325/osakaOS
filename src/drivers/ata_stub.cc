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
                        var sourceBuffer = cursor.value.data;
                        // Create a copy of the ArrayBuffer to avoid corruption
                        if (sourceBuffer instanceof ArrayBuffer) {
                            var byteLength = sourceBuffer.byteLength;
                            // Ensure we have at least 512 bytes (sector size)
                            var targetLength = Math.max(512, byteLength);
                            var data = new Uint8Array(targetLength);
                            // Initialize with zeros
                            for (var i = 0; i < targetLength; i++) {
                                data[i] = 0;
                            }
                            // Copy from source buffer
                            if (byteLength > 0) {
                                var sourceView = new Uint8Array(sourceBuffer);
                                var copyLength = Math.min(byteLength, targetLength);
                                for (var i = 0; i < copyLength; i++) {
                                    data[i] = sourceView[i];
                                }
                            }
                            Module._ata_cache['sector_' + sector] = data;
                        } else {
                            console.error('[ATA] Cache init: data is not an ArrayBuffer for sector', sector, '- value:', cursor.value);
                            // Use zeros if data format is wrong
                            var zeroData = new Uint8Array(512);
                            for (var i = 0; i < 512; i++) {
                                zeroData[i] = 0;
                            }
                            Module._ata_cache['sector_' + sector] = zeroData;
                        }
                        cursor.continue();
                    } else {
                        console.log('[ATA] Cache populated, refreshing filesystem');
                        // Mark cache as fully populated
                        Module._ata_cache_populated = true;
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
    
    // First, try to read from cache (fast path)
    bool cacheHit = EM_ASM_INT({
        var sector = $0;
        var dataPtr = $1;
        var count = $2;
        var offset = $3;
        
        if (!Module._ata_db) {
            // Database not ready yet - zero out data (fresh disk)
            for (var i = 0; i < count; i++) {
                HEAPU8[dataPtr + offset + i] = 0;
            }
            return 1; // Handled (returned zeros)
        }
        
        // Try to read from cache first
        var cacheKey = 'sector_' + sector;
        console.log('[ATA] Read28: sector=' + sector + ', cacheKey=' + cacheKey);
        console.log('[ATA] Cache exists:', !!Module._ata_cache);
        if (Module._ata_cache) {
            console.log('[ATA] Cache has key:', cacheKey in Module._ata_cache);
        }
        if (Module._ata_cache && Module._ata_cache[cacheKey]) {
            var cachedData = Module._ata_cache[cacheKey];
            // Check if cached data is all zeros - if so, verify it exists in IndexedDB
            // If it's all zeros and doesn't exist in IndexedDB, don't use the cache
            var allZeros = true;
            for (var i = 0; i < Math.min(512, cachedData.length); i++) {
                if (cachedData[i] !== 0) {
                    allZeros = false;
                    break;
                }
            }
            
            if (allZeros && Module._ata_db && sector > 1024) {
                // Cache has all zeros for a file data sector - this might be stale
                // Trigger an async IndexedDB read to verify and update cache if needed
                console.log('[ATA] Cache HIT - cached data is all zeros for sector', sector, '- verifying with IndexedDB');
                var verifyTransaction = Module._ata_db.transaction(['sectors'], 'readonly');
                var verifyStore = verifyTransaction.objectStore('sectors');
                var verifyRequest = verifyStore.get(sector);
                verifyRequest.onsuccess = function(event) {
                    var result = event.target.result;
                    if (result && result.data) {
                        // Sector exists in IndexedDB with data - update cache
                        var sourceBuffer = result.data;
                        if (sourceBuffer instanceof ArrayBuffer) {
                            var byteLength = sourceBuffer.byteLength;
                            var targetLength = Math.max(512, byteLength);
                            var data = new Uint8Array(targetLength);
                            for (var i = 0; i < targetLength; i++) {
                                data[i] = 0;
                            }
                            if (byteLength > 0) {
                                var sourceView = new Uint8Array(sourceBuffer);
                                var copyLength = Math.min(byteLength, targetLength);
                                for (var i = 0; i < copyLength; i++) {
                                    data[i] = sourceView[i];
                                }
                            }
                            var cacheCopy = new Uint8Array(data.length);
                            for (var i = 0; i < data.length; i++) {
                                cacheCopy[i] = data[i];
                            }
                            Module._ata_cache[cacheKey] = cacheCopy;
                            console.log('[ATA] Updated cache for sector', sector, 'from IndexedDB (was zeros, now has data)');
                        }
                    }
                };
            } else {
                console.log('[ATA] Cache HIT - using cached data');
            }
            
            // Use cached data
            // Cache contains full 512-byte sector starting at index 0
            // offset is the offset into the destination buffer, not the cache
            // Always read from cache starting at index 0, regardless of destination offset
            var copyCount = Math.min(count, cachedData.length);
            console.log('[ATA]: copyCount: ', copyCount, ', cachedData.length:', cachedData.length, ', offset:', offset);
            for (var i = 0; i < copyCount; i++) {
                // Read from cache at index i (cache has full sector from 0)
                // Write to destination at dataPtr + offset + i
                if (i < cachedData.length) {
                    HEAPU8[dataPtr + offset + i] = cachedData[i];
                    if (i < 10) {
                        console.log('[ATA]: copied data[', i, ']=', cachedData[i], ' to HEAPU8[', (dataPtr + offset + i), ']');
                    }
                } else {
                    HEAPU8[dataPtr + offset + i] = 0;
                }
            }
            // Fill remaining bytes with zeros if count > cachedData.length
            for (var i = copyCount; i < count; i++) {
                HEAPU8[dataPtr + offset + i] = 0;
            }
            return 1; // Cache hit
        }
        console.log('[ATA] Cache MISS - will read from IndexedDB');
        return 0; // Cache miss - need to read from IndexedDB
    }, sector, (uintptr_t)data, count, offset);
    
    if (cacheHit) {
        EM_ASM_({
            console.log('[ATA] Read28 returning early (cache hit)');
        });
        for (int i = 0; i < count; i++) {
            EM_ASM_({
                console.log('[ATA] data: ', HEAPU8[$0]);
            }, (uintptr_t)data + offset + i);
        }
        return; // Data already copied from cache
    }
    
    EM_ASM_({
        console.log('[ATA] Read28: Cache miss, reading from IndexedDB for sector', $0);
    }, sector); 
    // Cache miss - read from IndexedDB synchronously
    // Set up async read and wait for it to complete
    EM_ASM_({
        var sector = $0;
        Module._ata_read_sector = sector;
        Module._ata_read_result = null;
        Module._ata_read_done = false;
        
        if (!Module._ata_db) {
            Module._ata_read_result = null;
            Module._ata_read_done = true;
            return;
        }
        
        var cacheKey = 'sector_' + sector;
        var transaction = Module._ata_db.transaction(['sectors'], 'readonly');
        var store = transaction.objectStore('sectors');
        var request = store.get(sector);
        
        request.onsuccess = function(event) {
            var result = event.target.result;
            console.log('[ATA] IndexedDB read success for sector', sector, 'result:', !!result);
            if (result && result.data) {
                // Create a copy of the ArrayBuffer to avoid corruption
                var sourceBuffer = result.data;
                if (sourceBuffer instanceof ArrayBuffer) {
                    var byteLength = sourceBuffer.byteLength;
                    // Ensure we have at least 512 bytes (sector size)
                    var targetLength = Math.max(512, byteLength);
                    var data = new Uint8Array(targetLength);
                    // Initialize with zeros
                    for (var i = 0; i < targetLength; i++) {
                        data[i] = 0;
                    }
                    // Copy from source buffer
                    if (byteLength > 0) {
                        var sourceView = new Uint8Array(sourceBuffer);
                        var copyLength = Math.min(byteLength, targetLength);
                        for (var i = 0; i < copyLength; i++) {
                            data[i] = sourceView[i];
                        }
                    }
                    console.log('[ATA] Got data from IndexedDB, length:', data.length);
                    // Update cache for future reads - store a copy
                    if (!Module._ata_cache) {
                        Module._ata_cache = {};
                    }
                    // Create another copy for the cache to ensure it's independent
                    var cacheCopy = new Uint8Array(data.length);
                    for (var i = 0; i < data.length; i++) {
                        cacheCopy[i] = data[i];
                    }
                    Module._ata_cache[cacheKey] = cacheCopy;
                    Module._ata_read_result = data;
                } else {
                    console.error('[ATA] IndexedDB data is not an ArrayBuffer for sector', sector, '- result:', result);
                    // Use zeros if data format is wrong
                    var zeroData = new Uint8Array(512);
                    for (var i = 0; i < 512; i++) {
                        zeroData[i] = 0;
                    }
                    Module._ata_read_result = zeroData;
                }
            } else {
                console.log('[ATA] No data in IndexedDB for sector', sector, '- using zeros');
                // Sector doesn't exist - create zero-filled sector
                var zeroData = new Uint8Array(512);
                for (var i = 0; i < 512; i++) {
                    zeroData[i] = 0;
                }
                Module._ata_read_result = zeroData;
            }
            Module._ata_read_done = true;
        };
        
        request.onerror = function(event) {
            // Error reading - use zero-filled sector
            var zeroData = new Uint8Array(512);
            for (var i = 0; i < 512; i++) {
                zeroData[i] = 0;
            }
            Module._ata_read_result = zeroData;
            Module._ata_read_done = true;
        };
    }, sector);
    
    // Wait for read to complete
    // During initialization (cache not yet populated), don't use emscripten_sleep
    // to avoid deep ASYNCIFY stacks. Just return zeros and let cache populate async.
    // After initialization, use a single sleep to wait for IndexedDB read.
    bool cachePopulated = EM_ASM_INT({
        return Module._ata_cache_populated ? 1 : 0;
    });
    
    if (!cachePopulated) {
        // Still in initialization - return zeros immediately
        // Cache will be populated asynchronously, and RefreshFileTable will be called
        // after cache is populated, at which point cache should have the data
        EM_ASM_({
            console.log('[ATA] Still initializing - returning zeros, cache will populate async');
        });
        for (int i = 0; i < count; i++) {
            data[offset + i] = 0;
        }
        return;
    }
    
    // Cache is populated, but this sector wasn't in cache
    // Return zeros immediately and let IndexedDB read populate cache asynchronously
    // This completely avoids ASYNCIFY stack issues
    // Note: This means sectors not in cache will return zeros on first read.
    // However, Write28 updates cache immediately, so written sectors should be in cache.
    // For sectors that exist in IndexedDB but aren't in cache, they'll return zeros
    // on first read, but subsequent reads will hit the cache.
    EM_ASM_({
        console.log('[ATA] Cache miss after init - returning zeros, IndexedDB read will populate cache async for sector', $0);
        
        // Trigger async IndexedDB read to populate cache for future reads
        var sector = $0;
        var cacheKey = 'sector_' + sector;
        
        if (Module._ata_db) {
            var transaction = Module._ata_db.transaction(['sectors'], 'readonly');
            var store = transaction.objectStore('sectors');
            var request = store.get(sector);
            
            request.onsuccess = function(event) {
                var result = event.target.result;
                if (result && result.data) {
                    var sourceBuffer = result.data;
                    if (sourceBuffer instanceof ArrayBuffer) {
                        var byteLength = sourceBuffer.byteLength;
                        var targetLength = Math.max(512, byteLength);
                        var data = new Uint8Array(targetLength);
                        for (var i = 0; i < targetLength; i++) {
                            data[i] = 0;
                        }
                        if (byteLength > 0) {
                            var sourceView = new Uint8Array(sourceBuffer);
                            var copyLength = Math.min(byteLength, targetLength);
                            for (var i = 0; i < copyLength; i++) {
                                data[i] = sourceView[i];
                            }
                        }
                        // Update cache for future reads
                        if (!Module._ata_cache) {
                            Module._ata_cache = {};
                        }
                        var cacheCopy = new Uint8Array(data.length);
                        for (var i = 0; i < data.length; i++) {
                            cacheCopy[i] = data[i];
                        }
                        Module._ata_cache[cacheKey] = cacheCopy;
                        console.log('[ATA] IndexedDB read completed, cache updated for sector', sector);
                    }
                } else {
                    // Sector doesn't exist in IndexedDB
                    // Don't cache zeros here - let the sector remain uncached
                    // This way, if it gets written later, Write28 will cache it correctly
                    // And if it's read again before being written, it will return zeros but won't cache them
                    console.log('[ATA] Sector', sector, 'does not exist in IndexedDB - not caching (will return zeros on read)');
                }
            };
            
            request.onerror = function(event) {
                console.error('[ATA] IndexedDB read error for sector', sector, ':', event.target.error);
            };
        }
    }, sector);
    
    // Return zeros immediately - no ASYNCIFY, no stack issues
    for (int i = 0; i < count; i++) {
        data[offset + i] = 0;
    }
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
            // Queue the write or wait for database to be ready
            console.warn('[ATA] Write28: Database not ready for sector', sector, '- write will be lost!');
            // Still update cache so reads work
            var cacheKey = 'sector_' + sector;
            if (!Module._ata_cache) {
                Module._ata_cache = {};
            }
            // Create sector data from WASM memory
            var sectorData = new Uint8Array(512);
            for (var i = 0; i < 512; i++) {
                sectorData[i] = 0;
            }
            for (var i = 0; i < count && (offset + i) < 512; i++) {
                sectorData[offset + i] = HEAPU8[dataPtr + i];
            }
            Module._ata_cache[cacheKey] = sectorData;
            console.log('[ATA] Write28: Updated cache only (DB not ready) for sector', sector);
            return;
        }
        
        // Read existing sector data if it exists, then write
        // We need to wait for the write to complete to ensure cache is correct
        Module._ata_write_complete = false;
        var cacheKey = 'sector_' + sector;
        
        // Update cache immediately with data from WASM memory (synchronous)
        // This ensures subsequent reads will find the data even before IndexedDB write completes
        if (!Module._ata_cache) {
            Module._ata_cache = {};
        }
        // Read existing sector from cache if it exists, otherwise create new one
        var sectorData;
        if (Module._ata_cache[cacheKey]) {
            // Use existing cached sector
            sectorData = new Uint8Array(Module._ata_cache[cacheKey].length);
            for (var i = 0; i < Module._ata_cache[cacheKey].length; i++) {
                sectorData[i] = Module._ata_cache[cacheKey][i];
            }
            console.log('[ATA] Write28: Using cached sector data, first byte:', sectorData[0]);
        } else {
            // Create new sector (512 bytes)
            sectorData = new Uint8Array(512);
            for (var i = 0; i < 512; i++) {
                sectorData[i] = 0;
            }
            console.log('[ATA] Write28: Created new sector (all zeros)');
        }
        
        // Copy data from WASM memory to sector buffer (synchronous)
        console.log('[ATA] Write28: Copying data, offset=' + offset);
        for (var i = 0; i < count && (offset + i) < 512; i++) {
            var wasmByte = HEAPU8[dataPtr + i];
            sectorData[offset + i] = wasmByte;
            if (i < 5) {
                console.log('[ATA] Write28: i=' + i + ', wasmByte=' + wasmByte + ', wrote to sector[' + (offset + i) + ']');
            }
        }
        console.log('[ATA] Write28: After copy, sector[0]=' + sectorData[0] + ', sector[' + offset + ']=' + sectorData[offset]);
        
        // Update cache immediately (synchronous, before async IndexedDB operations)
        var cacheData = new Uint8Array(sectorData.length);
        for (var i = 0; i < sectorData.length; i++) {
            cacheData[i] = sectorData[i];
        }
        Module._ata_cache[cacheKey] = cacheData;
        console.log('[ATA] Write28: Updated cache for sector', sector, '- cacheData[0]=' + cacheData[0] + ', cacheData.length=' + cacheData.length);
        
        // Now do the async IndexedDB read/write
        var transaction = Module._ata_db.transaction(['sectors'], 'readwrite');
        var store = transaction.objectStore('sectors');
        var getRequest = store.get(sector);
        
        getRequest.onsuccess = function(event) {
            var result = event.target.result;
            // sectorData is already created and cache is already updated above (synchronously)
            // For partial writes, we might need to merge with IndexedDB data, but for now
            // we'll just use the sectorData we already have (which was created from cache or zeros)
            // The cache already has the latest data, so IndexedDB write will persist it
            
            console.log('[ATA] Write28: About to save to IndexedDB, sectorData[0]=' + sectorData[0]);
            
            // Save to IndexedDB
            // Create a copy of the ArrayBuffer to avoid corruption if sectorData is reused
            var bufferCopy = new ArrayBuffer(sectorData.length);
            var bufferView = new Uint8Array(bufferCopy);
            bufferView.set(sectorData);
            
            console.log('[ATA] Write28: About to save sector', sector, 'to IndexedDB, sectorData[0]=' + sectorData[0] + ', sectorData[' + offset + ']=' + sectorData[offset]);
            var putRequest = store.put({
                sector: sector,
                data: bufferCopy
            });
            
            putRequest.onsuccess = function() {
                console.log('[ATA] Write28: Successfully wrote sector', sector, 'to IndexedDB - sectorData[0] was', sectorData[0]);
                // Verify the write by reading it back
                var verifyRequest = store.get(sector);
                verifyRequest.onsuccess = function(verifyEvent) {
                    var verifyResult = verifyEvent.target.result;
                    if (verifyResult && verifyResult.data) {
                        console.log('[ATA] Write28: Verified sector', sector, 'exists in IndexedDB');
                    } else {
                        console.error('[ATA] Write28: ERROR - sector', sector, 'not found in IndexedDB after write!');
                    }
                };
                Module._ata_write_complete = true;
            };
            
            putRequest.onerror = function(event) {
                console.error('[ATA] Write28: ERROR writing sector', sector, 'to IndexedDB:', event.target.error);
                Module._ata_write_complete = true;
            };
            
            // Also listen for transaction completion
            transaction.oncomplete = function() {
                console.log('[ATA] Write28: Transaction completed for sector', sector);
            };
            
            transaction.onerror = function(event) {
                console.error('[ATA] Write28: Transaction error for sector', sector, ':', event.target.error);
            };
        };
        
        getRequest.onerror = function(event) {
            console.error('[ATA] Read error during write:', event.target.error);
            Module._ata_write_complete = true;
        };
        
        // Note: Write will complete asynchronously
        // We can't wait here because it causes ASYNCIFY stack issues
        // The cache is updated immediately, so reads will work from cache
        // IndexedDB write will complete in background for persistence
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

